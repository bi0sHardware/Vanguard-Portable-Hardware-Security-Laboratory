#include "lora.h"
#include "../../include/pins.h"
#include "../../include/config.h"
#include "EspHal.h"
#include <RadioLib.h>

namespace rf {

// LoRa RESET is hardware pull-up only (no GPIO) -> RADIOLIB_NC. DIO1/BUSY are read-only chip outputs.
//
// CHIP IDENTITY: module silkscreen/datasheet say LLCC68, but it actually
// answers RadioLib's version probe as "SX1261 V2D 2D02" -- using the LLCC68
// class made begin() fail with RADIOLIB_ERR_CHIP_NOT_FOUND despite SPI being
// fine. SX1261 power is capped at 14 dBm (vs LLCC68's 22); don't raise pwr
// past 14 without checking RADIOLIB_ERR_INVALID_OUTPUT_POWER.
//
// HAL: uses native ESP-IDF HAL (EspHal) instead of ArduinoHal/SPIClass --
// chip detection was unreliable under Arduino-as-ESP-IDF-component due to
// delay()/delayMicroseconds() timing differences; this HAL removes that variable.
static EspHal s_hal(pins::LORA_SCK, pins::LORA_MISO, pins::LORA_MOSI);
static Module s_module(&s_hal, pins::LORA_NSS, pins::LORA_DIO1, RADIOLIB_NC, pins::LORA_BUSY);
static SX1261 s_radio(&s_module);
static bool s_ready = false;
static bool s_rxOngoing = false;
static bool s_txOngoing = false;
static int16_t s_lastRssi = -128;

// Mirrors init()'s hardcoded begin() args, named so applyProfile() has
// something to revert to and radio_link something to restore on exit.
const Profile kMissionProfile   = { cfg::LORA_FREQUENCY_MHZ, 125.0f, 9, 7, 10, 8 };
const Profile kBadgeLinkProfile = { cfg::LORA_BADGELINK_FREQUENCY_MHZ, 125.0f, 7, 5, 10, 8 };
static Profile s_profile = kMissionProfile;

void deselect() {
    pinMode(pins::LORA_NSS, OUTPUT);
    digitalWrite(pins::LORA_NSS, HIGH);
}

// Best-effort recovery for an SX1261 left unresponsive. RESET has no GPIO
// here, so a truly wedged chip (BUSY stuck high) can't be forced back --
// only a power cycle clears that. What we can do is the datasheet's
// wake-from-sleep sequence (falling edge on NSS, then BUSY goes low when
// ready), which recovers the "chip went to sleep" case.
static bool tryWakeAndSettle() {
    pinMode(pins::LORA_BUSY, INPUT);
    pinMode(pins::LORA_NSS, OUTPUT);
    digitalWrite(pins::LORA_NSS, HIGH);
    delay(2);
    digitalWrite(pins::LORA_NSS, LOW);   // falling edge = wake request
    delayMicroseconds(100);
    digitalWrite(pins::LORA_NSS, HIGH);
    // Datasheet allows ~1ms to become ready; give it 10x before giving up.
    for (int i = 0; i < 100; i++) {
        if (digitalRead(pins::LORA_BUSY) == LOW) return true;
        delayMicroseconds(100);
    }
    return false;
}

void init() {
    bool busyLow = tryWakeAndSettle();
    if (!busyLow) {
        Serial.println("[LORA] BUSY stuck high before init -- radio unresponsive.");
        Serial.println("[LORA] Switch the badge OFF and back ON to clear it (the");
        Serial.println("[LORA] module's RESET has no GPIO, so a power cycle is the");
        Serial.println("[LORA] only way to reset it).");
    }

    int state = s_radio.begin(cfg::LORA_FREQUENCY_MHZ, 125.0, 9, 7,
                              RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 0.0);
    if (state != RADIOLIB_ERR_NONE && busyLow) {
        // One retry: some SX126x errata need a second begin() after a marginal first attempt.
        Serial.println("[LORA] first begin() failed, retrying once...");
        tryWakeAndSettle();
        state = s_radio.begin(cfg::LORA_FREQUENCY_MHZ, 125.0, 9, 7,
                              RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 0.0);
    }
    s_ready = (state == RADIOLIB_ERR_NONE);
    Serial.print("[LORA] init: ");
    Serial.print(s_ready ? "OK" : "FAILED");
    Serial.print(" (state=");
    Serial.print(state);
    Serial.println(")");
}

bool selfTest() {
    return s_ready;
}

bool transmit(const String& payload) {
    if (!s_ready) return false;
    s_rxOngoing = false; // transmit() internally switches out of RX mode
    int state = s_radio.transmit(payload.c_str());
    return state == RADIOLIB_ERR_NONE;
}

bool pollReceive(String& outPayload) {
    if (!s_ready) return false;
    // A transmission physically in flight (radio_link's non-blocking TX) must never be interrupted by arming RX.
    if (s_txOngoing) return false;

    if (!s_rxOngoing) {
        int rc = s_radio.startReceive();
        if (rc != RADIOLIB_ERR_NONE) {
            Serial.print("[LORA] startReceive failed, state=");
            Serial.println(rc);
            return false;
        }
        s_rxOngoing = true;
    }

    // No per-poll/per-packet logging: at 115200 baud it costs enough
    // blocking serial I/O to miss back-to-back packets (caused Mission 04
    // stalls previously).
    uint16_t irq = s_radio.getIrqStatus();
    // DIAGNOSTIC: surfaces radio activity that never reaches RX_DONE (CRC/
    // header error = something arrived but failed a check). Excludes
    // PREAMBLE_DETECTED deliberately -- it fires on nearly every packet,
    // which would reintroduce the per-packet logging cost this avoids.
    //
    // These IRQ flags are only cleared by clearIrqStatus() (inside
    // readData()/startReceive()), neither of which runs on this branch, so
    // a latched flag would otherwise print every tick forever. Forcing
    // s_rxOngoing false makes the next tick re-issue startReceive(), clearing it.
    if (irq & (RADIOLIB_SX126X_IRQ_CRC_ERR | RADIOLIB_SX126X_IRQ_HEADER_ERR |
               RADIOLIB_SX126X_IRQ_TIMEOUT)) {
        if (Serial) {
            Serial.print("[LORA] RX activity without a clean packet, irq=0b");
            Serial.println(irq, BIN);
        }
        s_rxOngoing = false; // re-arm RX next tick, clearing the latched flags
        return false;
    }
    if (!(irq & RADIOLIB_SX126X_IRQ_RX_DONE)) {
        return false; // nothing received yet this tick — non-blocking
    }

    uint8_t buf[256];
    size_t len = s_radio.getPacketLength();
    if (len > sizeof(buf) - 1) len = sizeof(buf) - 1;
    int state = s_radio.readData(buf, len);
    s_rxOngoing = false; // readData() takes the radio out of RX; restart next tick

    if (state == RADIOLIB_ERR_NONE) {
        buf[len] = 0;
        outPayload = String((char*)buf);
        s_lastRssi = (int16_t)s_radio.getRSSI();
        return true;
    }
    return false;
}

void stopReceiving() {
    if (!s_ready || !s_rxOngoing) return;
    s_radio.standby();
    s_rxOngoing = false;
}

// ---------------------------------------------------------------------
// Radio Chat / Ship Battle additions
// ---------------------------------------------------------------------

// Applies every field of a Profile in sequence, stopping at first failure.
// Shared by applyProfile()'s forward attempt and its revert-on-failure.
static int16_t applyProfileRaw(const Profile& p) {
    int16_t state = s_radio.setFrequency(p.freqMhz);
    if (state == RADIOLIB_ERR_NONE) state = s_radio.setBandwidth(p.bwKhz);
    if (state == RADIOLIB_ERR_NONE) state = s_radio.setSpreadingFactor(p.sf);
    if (state == RADIOLIB_ERR_NONE) state = s_radio.setCodingRate(p.cr);
    if (state == RADIOLIB_ERR_NONE) state = s_radio.setOutputPower(p.powerDbm);
    if (state == RADIOLIB_ERR_NONE) state = s_radio.setPreambleLength(p.preambleSym);
    return state;
}

bool applyProfile(const Profile& p) {
    if (!s_ready) return false;
    // SX126x rejects freq/SF changes outside standby; setFrequency() also
    // triggers a recalibration that must not race an in-flight RX/TX.
    s_radio.standby();
    s_rxOngoing = false;
    s_txOngoing = false;
    int16_t state = applyProfileRaw(p);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] applyProfile failed (state=%d) -- reverting to previous profile\n", state);
        applyProfileRaw(s_profile); // best-effort; s_profile is left as the source of truth either way
        return false;
    }
    s_profile = p;
    return true;
}

const Profile& currentProfile() { return s_profile; }
int16_t lastRssiDbm() { return s_lastRssi; }

uint32_t airtimeMs(size_t payloadChars) {
    return (uint32_t)(s_radio.getTimeOnAir(payloadChars) / 1000);
}

bool beginTransmit(const String& payload) {
    if (!s_ready || s_txOngoing) return false;
    s_rxOngoing = false; // leaving RX regardless, same as the blocking transmit() above
    int state = s_radio.startTransmit(payload.c_str());
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] beginTransmit failed, state=%d\n", state);
        return false;
    }
    s_txOngoing = true;
    return true;
}

bool txInFlight() { return s_txOngoing; }

bool txComplete() {
    if (!s_txOngoing) return false;
    return (s_radio.getIrqStatus() & RADIOLIB_SX126X_IRQ_TX_DONE) != 0;
}

bool endTransmit() {
    if (!s_txOngoing) return false;
    int state = s_radio.finishTransmit();
    s_txOngoing = false;
    return state == RADIOLIB_ERR_NONE;
}

bool channelBusy() {
    if (!s_ready || s_txOngoing) return false;
    s_rxOngoing = false; // scanChannel() leaves the radio in standby afterward
    int state = s_radio.scanChannel();
    return state != RADIOLIB_CHANNEL_FREE;
}

} // namespace rf

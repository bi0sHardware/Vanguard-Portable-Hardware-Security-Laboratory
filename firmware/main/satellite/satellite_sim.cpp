// Vanguard Satellite Simulator — the other side of Missions 02-04. A second badge
// running the same project (shared pins.h/EspHal/rf:: stack) playing the satellite role.
// Build with VANGUARD_SATELLITE_SIM=1 (idf.py -DVANGUARD_SATELLITE_SIM=1 build).
//
// Continuously broadcasts 10 fixed frames over framecodec:: (AX.25-style UI-frame,
// never named that to players). Packets 1-5 plaintext, 6-10 XOR'd with a fixed key.
// The badge filters this one stream client-side per mission screen; the satellite
// doesn't know which mission is open.
//
// Mission 03's uplink is additive: once a badge sends a correctly-authenticated uplink
// (matching the badge's inscription phrase, sent in clear), the satellite also starts
// interleaving Mission 04's payload chunks (VGDIMG:...) alongside the still-running loop.
//
// Operator UI: this build uses the badge's own TFT + joystick for a status screen and a
// JoyLeft/JoyRight TX/RX mode select. AUTO (broadcast + listen, default) is the only mode
// that's mission-correct for an actual event; TX ONLY / RX ONLY exist for setup/diagnostics
// and never change the protocol logic below, only which of transmit/receive this file calls.
#include <Arduino.h>
#include "../rf/lora.h"
#include "../rf/frame_codec.h"
#include "../challenges/console.h"
#include "../challenges/payload_data.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../ui/widgets.h"
#include "../ui/theme.h"
#include "../../include/pins.h"
#include <cstring>

namespace {

// Downlink encryption key for Packets 6-10.
constexpr uint8_t kDownlinkKey = 0x50;

// The badge's PCB inscription translated, lowercase, no further transformation.
// Compared exact-case in handleReceive().
constexpr const char* kExpectedUplinkPhrase = "kneelorbeerased";

// The ten frames transmitted forever. 1-5 plaintext; 6-10 encrypted with kDownlinkKey
// (done once at boot in buildPacketFrames(), not per-transmission).
constexpr const char* kPacketInfo[10] = {
    "SYS=OK",           // Packet 1
    "MODE=NOMINAL",     // Packet 2
    "LINK=GOOD",        // Packet 3
    "435.500",          // Packet 4 -- uplink frequency (narrative value)
    "AUTH=0x50",        // Packet 5
    "MISSION=VG01",     // Packet 6  (encrypted on air)
    "LINK=SECURE",      // Packet 7  (encrypted on air)
    // Packets 8-10 (encrypted) are the Level 3 directive, split across three packets so
    // a player must capture all three in order. Kept under LoRa's ~255-byte payload limit.
    "MISSION DIRECTIVE\n\nRecover the inscription",   // Packet 8
    // Must keep at least one uppercase/non-printable-after-XOR char, or this could XOR
    // back into printable ASCII by coincidence and get misclassified as plaintext by
    // isPrintableAscii(), breaking mission3.cpp's Packet-6 boundary sync.
    "engraved above the\nCommunications module.",     // Packet 9
    "Identify its language.\n\nTransmit the decoded message.", // Packet 10
};
constexpr bool kPacketEncrypted[10] = {
    false, false, false, false, false, true, true, true, true, true,
};
constexpr int kPacketCount = 10;

String s_packetFrame[kPacketCount]; // built once at boot

// Telemetry cadence. Radio is half-duplex, so this trades loop liveliness against
// receiver listening time for Mission 03's uplink.
constexpr unsigned long kLoopPeriodMs  = 5000;
// Spacing between payload chunk transmissions. rf::transmit() for one chunk measured
// ~1032ms blocking on hardware, far exceeding this period, so once authenticated the
// satellite is TX-locked almost continuously — a concurrent uplink attempt has little
// listening time to land in. Raising this value was tried and reverted (made
// s_loopPausedForTransfer stretch to minutes without reliably fixing the contention);
// left at the original value.
constexpr unsigned long kChunkPeriodMs = 250;

// Operator-selected mode (JoyLeft/JoyRight). AUTO reproduces the original unconditional
// broadcast+listen behavior exactly and is the default; TX ONLY / RX ONLY are diagnostics.
enum class SatMode { Auto, TxOnly, RxOnly };
SatMode s_mode = SatMode::Auto;

bool     s_authenticated = false;
int      s_authCount = 0; // total successful auths this boot, shown on the status screen
// Set on every successful uplink auth, cleared after one full pass of payload chunks.
// While set, the Packet 1-10 loop is skipped so the newly authenticated badge gets an
// uncontended pass instead of competing with the telemetry loop.
bool     s_loopPausedForTransfer = false;
// Deadline until which the loop also stays paused, refreshed on every frame addressed to
// this satellite (correct or not) — the only signal available that a badge is mid-retry.
unsigned long s_loopPauseUntil = 0;
constexpr unsigned long kUplinkPauseWindowMs = 4000;
int      s_loopIdx  = 0;
int      s_chunkIdx = 0;
unsigned long s_lastLoop  = 0;
unsigned long s_lastChunk = 0;

// ---------------------------------------------------------------- display
//
// No AppState/Region machinery here — the satellite build never enters that
// state machine (see main.cpp) — so this manages its own dirty-checking:
// redraw only the line whose value actually changed, each tick.

constexpr int16_t kEventLogLines = 3;
String s_eventLog[kEventLogLines]; // [0] = most recent
bool s_eventLogDirty = true;

void logEvent(const String& line) {
    for (int i = kEventLogLines - 1; i > 0; i--) s_eventLog[i] = s_eventLog[i - 1];
    s_eventLog[0] = line;
    s_eventLogDirty = true;
    if (Serial) Serial.println(line);
}

const char* modeLabel(SatMode m) {
    switch (m) {
        case SatMode::Auto:   return "AUTO";
        case SatMode::TxOnly: return "TX ONLY";
        case SatMode::RxOnly: return "RX ONLY";
    }
    return "?";
}

uint16_t modeColor(SatMode m) {
    return m == SatMode::Auto ? theme::COLOR_ACCENT_DARK : theme::COLOR_DANGER;
}

constexpr int16_t kModeY    = 44;
constexpr int16_t kLoopY    = 78;
constexpr int16_t kUplinkY  = 100;
constexpr int16_t kPayloadY = 122;
constexpr int16_t kLogY     = 154;
constexpr int16_t kLogLineH = 14;
constexpr int16_t kFooterY  = 226;

void drawChrome() {
    Adafruit_ST7789& tft = display::tft();
    ui::widgets::clearScreen(tft);
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)ui::widgets::screenWidth(),
                                      (int16_t)ui::widgets::headerHeight()},
                        "VANGUARD SATELLITE");
    theme::drawCentered(tft, "< JoyLeft / JoyRight >  change mode", kFooterY,
                        theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
}

// Last-drawn snapshot, so each tick only repaints the one line that changed.
SatMode s_drawnMode = (SatMode)-1;
int s_drawnLoopIdx = -1;
bool s_drawnLoopPaused = false;
int s_drawnAuthCount = -1;
bool s_drawnAuthenticated = false;
bool s_drawnStreaming = false;
int s_drawnChunkIdx = -1;

void drawModeLine() {
    Adafruit_ST7789& tft = display::tft();
    tft.fillRect(0, kModeY - 10, ui::widgets::screenWidth(), 20, theme::COLOR_BG);
    char buf[32];
    snprintf(buf, sizeof(buf), "MODE: %s", modeLabel(s_mode));
    theme::drawCentered(tft, buf, kModeY, theme::LIST_TEXT_SIZE, modeColor(s_mode));
}

void drawStatusLine(int16_t y, const char* label, const String& value) {
    Adafruit_ST7789& tft = display::tft();
    tft.fillRect(theme::MARGIN_X, y - 9, ui::widgets::screenWidth() - 2 * theme::MARGIN_X, 16,
                theme::COLOR_BG);
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    tft.setTextColor(theme::COLOR_ACCENT_DARK);
    tft.setCursor(theme::MARGIN_X, y - 5);
    tft.print(label);
    tft.setTextColor(theme::COLOR_TEXT);
    tft.setCursor(theme::MARGIN_X + 70, y - 5);
    tft.print(value);
}

void drawEventLog() {
    Adafruit_ST7789& tft = display::tft();
    tft.fillRect(theme::MARGIN_X, kLogY - 10,
                ui::widgets::screenWidth() - 2 * theme::MARGIN_X,
                kEventLogLines * kLogLineH + 6, theme::COLOR_BG);
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    for (int i = 0; i < kEventLogLines; i++) {
        if (s_eventLog[i].length() == 0) continue;
        tft.setTextColor(i == 0 ? theme::COLOR_TEXT : theme::COLOR_ACCENT);
        tft.setCursor(theme::MARGIN_X, kLogY + i * kLogLineH - 6);
        tft.print(s_eventLog[i]);
    }
}

void updateDisplay() {
    if (s_mode != s_drawnMode) {
        drawModeLine();
        s_drawnMode = s_mode;
    }

    bool loopPaused = s_loopPausedForTransfer || millis() < s_loopPauseUntil;
    if (s_loopIdx != s_drawnLoopIdx || loopPaused != s_drawnLoopPaused) {
        char buf[24];
        if (loopPaused) snprintf(buf, sizeof(buf), "paused");
        else snprintf(buf, sizeof(buf), "Packet %d/%d", s_loopIdx + 1, kPacketCount);
        drawStatusLine(kLoopY, "Loop:", buf);
        s_drawnLoopIdx = s_loopIdx;
        s_drawnLoopPaused = loopPaused;
    }

    if (s_authCount != s_drawnAuthCount || s_authenticated != s_drawnAuthenticated) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%d authenticated", s_authCount);
        drawStatusLine(kUplinkY, "Uplink:", buf);
        s_drawnAuthCount = s_authCount;
        s_drawnAuthenticated = s_authenticated;
    }

    bool streaming = s_authenticated;
    if (streaming != s_drawnStreaming || (streaming && s_chunkIdx != s_drawnChunkIdx)) {
        String buf = streaming
            ? (String("chunk ") + String(s_chunkIdx) + "/" + String(satpayload::kChunks))
            : String("idle");
        drawStatusLine(kPayloadY, "Payload:", buf);
        s_drawnStreaming = streaming;
        s_drawnChunkIdx = s_chunkIdx;
    }

    if (s_eventLogDirty) {
        drawEventLog();
        s_eventLogDirty = false;
    }
}

void handleModeInput() {
    if (input::wasPressed(input::Button::JoyRight)) {
        s_mode = (s_mode == SatMode::Auto) ? SatMode::TxOnly
               : (s_mode == SatMode::TxOnly) ? SatMode::RxOnly : SatMode::Auto;
        logEvent(String("Mode -> ") + modeLabel(s_mode));
    } else if (input::wasPressed(input::Button::JoyLeft)) {
        s_mode = (s_mode == SatMode::Auto) ? SatMode::RxOnly
               : (s_mode == SatMode::RxOnly) ? SatMode::TxOnly : SatMode::Auto;
        logEvent(String("Mode -> ") + modeLabel(s_mode));
    }
}

// ---------------------------------------------------------------- protocol

void buildPacketFrames() {
    // Downlink: ground is destination, satellite is source.
    uint8_t dest[framecodec::kAddrBytes], src[framecodec::kAddrBytes];
    framecodec::groundAddress(false, dest);
    framecodec::satelliteAddress(true, src);

    for (int i = 0; i < kPacketCount; i++) {
        uint8_t info[128];
        size_t n = strlen(kPacketInfo[i]);
        memcpy(info, kPacketInfo[i], n);
        if (kPacketEncrypted[i]) framecodec::xorInPlace(info, n, kDownlinkKey);
        s_packetFrame[i] = framecodec::buildFrame(dest, src, framecodec::kPid, info, n);
    }
}

void transmitLoopPacket() {
    const String& frame = s_packetFrame[s_loopIdx];
    int shown = s_loopIdx + 1; // 1-indexed to match "Packet 1..10" everywhere else
    s_loopIdx = (s_loopIdx + 1) % kPacketCount;
    if (rf::transmit(frame)) {
        logEvent(String("TX Packet ") + String(shown));
    } else {
        // Surfaces oversized-payload failures immediately instead of a packet silently
        // never appearing in the loop.
        logEvent(String("[!!] TX FAILED Packet ") + String(shown));
    }
}

void transmitChunk() {
    size_t off = (size_t)s_chunkIdx * satpayload::kChunkBytes;
    if (off >= satpayload::kLength) { s_chunkIdx = 0; off = 0; }

    size_t n = satpayload::kLength - off;
    if (n > satpayload::kChunkBytes) n = satpayload::kChunkBytes;

    String msg = "VGDIMG:" + String(s_chunkIdx) + ":" + String(satpayload::kChunks) +
                 ":" + String((unsigned long)off) + ":";
    char hex[3];
    for (size_t i = 0; i < n; i++) {
        snprintf(hex, sizeof(hex), "%02X", satpayload::kData[off + i]);
        msg += hex;
    }
    rf::transmit(msg);
    s_chunkIdx++;
    if ((size_t)s_chunkIdx * satpayload::kChunkBytes >= satpayload::kLength) {
        s_chunkIdx = 0;
        if (s_loopPausedForTransfer) {
            // Full pass done uncontended; give other badges' telemetry loop its airtime back.
            s_loopPausedForTransfer = false;
            logEvent("Payload pass complete.");
        }
    }
}

// Parses received frames; authenticates if the Information Field matches the expected
// phrase (sent in clear). Anything unparseable, misaddressed, or non-matching is ignored.
void handleReceive(const String& pkt) {
    uint8_t dest[framecodec::kAddrBytes], src[framecodec::kAddrBytes], pid, info[128];
    size_t infoLen;
    if (!framecodec::parseFrame(pkt, dest, src, pid, info, sizeof(info), infoLen)) return;

    uint8_t expectedDest[framecodec::kAddrBytes];
    framecodec::satelliteAddress(false, expectedDest);
    if (memcmp(dest, expectedDest, framecodec::kAddrBytes) != 0) return; // not addressed to us

    // A badge is mid retry-burst -- give it a clear channel, before the phrase check, since
    // even a wrong attempt means a badge is actively trying.
    s_loopPauseUntil = millis() + kUplinkPauseWindowMs;

    String raw;
    raw.reserve(infoLen + 1);
    for (size_t i = 0; i < infoLen; i++) raw += (char)info[i];
    raw.trim(); // case is significant, no toUpperCase()

    // All badges share the same satellite/ground addressing, so mission3.cpp appends a
    // per-attempt token (":XXXX") that must be echoed back — otherwise a badge can't tell
    // its own attempt succeeded from another badge's nearby success. Token is opaque here.
    String token, phrase = raw;
    if (raw.length() >= 5 && raw.charAt(raw.length() - 5) == ':') {
        token = raw.substring(raw.length() - 4);
        phrase = raw.substring(0, raw.length() - 5);
    }

    logEvent("RX uplink frame.");

    if (phrase != kExpectedUplinkPhrase) {
        logEvent("Auth REJECTED (bad phrase)");
        // Explicit NACK, not silence, so "wrong payload" is distinguishable from a timeout.
        delay(60);
        String nackInfo = String("ACK=AUTH_FAIL:") + token;
        uint8_t nackDest[framecodec::kAddrBytes], nackSrc[framecodec::kAddrBytes];
        framecodec::groundAddress(false, nackDest);
        framecodec::satelliteAddress(true, nackSrc);
        String nackFrame = framecodec::buildFrame(nackDest, nackSrc, framecodec::kPid,
                                                   (const uint8_t*)nackInfo.c_str(), nackInfo.length());
        rf::transmit(nackFrame);
        return;
    }

    delay(60); // guard interval so the ground terminal finishes switching TX->RX first
    String ackInfo = String("ACK=AUTH_OK:") + token;
    uint8_t ackDest[framecodec::kAddrBytes], ackSrc[framecodec::kAddrBytes];
    framecodec::groundAddress(false, ackDest);
    framecodec::satelliteAddress(true, ackSrc);
    String ackFrame = framecodec::buildFrame(ackDest, ackSrc, framecodec::kPid,
                                             (const uint8_t*)ackInfo.c_str(), ackInfo.length());
    if (rf::transmit(ackFrame)) {
        s_authCount++;
        logEvent(String("Auth OK (#") + String(s_authCount) + ")");
        if (!s_authenticated) s_authenticated = true;
        s_loopPausedForTransfer = true;
        s_chunkIdx = 0; // start the priority pass from the beginning
        // Seed s_lastChunk to now, else chunk 0 would fire this same tick on top of the
        // ACK, leaving no clear window for the badge to catch the ACK first.
        s_lastChunk = millis();
    }
}

} // namespace

void satelliteSetup() {
    Serial.begin(115200);
    delay(300);

    // Same shared-SPI-bus ordering constraint as the player badge (main.cpp):
    // rf::deselect() must run before display::init() touches the bus.
    rf::deselect();
    display::init();
    input::init();
    rf::init();
    buildPacketFrames();

    console::banner("VANGUARD SATELLITE SIMULATOR",
                    "Packets 1-10 on a loop / uplink authentication / payload downlink");
    if (rf::selfTest()) {
        console::ok("LoRa radio online.");
    } else {
        console::err("LoRa radio FAILED to initialise.");
        console::warn("Switch the board OFF and back ON, then retry.");
    }
    char buf[48];
    snprintf(buf, sizeof(buf), "%u bytes / %d chunks",
             (unsigned)satpayload::kLength, satpayload::kChunks);
    console::field("Payload", buf);
    console::info("Boot mode: AUTO. JoyLeft/JoyRight on-screen selects TX ONLY / RX ONLY.");

    drawChrome();
    logEvent("Satellite online.");
}

void satelliteLoop() {
    input::update();
    handleModeInput();

    if (s_mode != SatMode::TxOnly) {
        String pkt;
        if (rf::pollReceive(pkt)) handleReceive(pkt);
    }

    unsigned long now = millis();
    // Loop suspended by RX ONLY mode, s_loopPausedForTransfer, or s_loopPauseUntil — all
    // three give RX (an uplink attempt or priority payload pass) the whole radio.
    bool loopSuspended = s_mode == SatMode::RxOnly || s_loopPausedForTransfer || now < s_loopPauseUntil;
    if (s_mode != SatMode::RxOnly && !loopSuspended && now - s_lastLoop >= kLoopPeriodMs) {
        s_lastLoop = now;
        transmitLoopPacket();
    }
    // Payload streaming is additive once authenticated but also respects s_loopPauseUntil,
    // since chunks every 250ms otherwise degrade a second uplink attempt's chance of landing.
    if (s_mode != SatMode::RxOnly && s_authenticated && now >= s_loopPauseUntil &&
        now - s_lastChunk >= kChunkPeriodMs) {
        s_lastChunk = now;
        transmitChunk();
    }

    updateDisplay();
    delay(1);
}

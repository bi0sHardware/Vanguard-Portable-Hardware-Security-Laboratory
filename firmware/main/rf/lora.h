#pragma once
#include <Arduino.h>

// RA-01SC (LLCC68) LoRa module — spec §2, §11. Not a standalone main-menu
// entry; encountered in-context during Challenges Stage 2/3 (RF Intelligence
// / Protocol Exploitation), and exercised by the Settings Hardware Test Suite.
namespace rf {

// Drives LoRa's NSS high (chip deselected) and nothing else. Must be called
// before any other device drives the shared FSPI bus (see main.cpp setup()).
void deselect();

void init();
bool selfTest();      // confirms SPI/chip presence (used by Hardware Test Suite)
bool transmit(const String& payload);

// Non-blocking: call every frame tick. Returns true (with outPayload set)
// only on the tick a packet completes. Keeps the radio in continuous RX
// between calls, per the cooperative main loop's needs.
bool pollReceive(String& outPayload);

// Standby, taking the radio out of RX; safe regardless of RX state. Used to
// guarantee LoRa is quiescent before a BLE session starts.
void stopReceiving();

// ---------------------------------------------------------------------
// Radio Chat / Ship Battle additions -- everything above this line is
// unchanged and still governs Missions 02-04 and the satellite build.
// ---------------------------------------------------------------------

// A complete radio configuration. kMissionProfile matches init()'s existing
// settings; kBadgeLinkProfile is a separate frequency+SF for radio_link's
// channel, chosen not to collide with mission/satellite traffic and to cost
// less airtime (mission screens still use blocking transmit(), which must
// never run this fast/this far off-frequency).
struct Profile {
    float    freqMhz;
    float    bwKhz;
    uint8_t  sf;   // 5..12
    uint8_t  cr;   // 5..8 (denominator of 4/N)
    int8_t   powerDbm;
    uint16_t preambleSym;
};
extern const Profile kMissionProfile;    // matches init()'s hardcoded settings exactly
extern const Profile kBadgeLinkProfile;  // 433.7 MHz, SF7/CR4:5 -- radio_link's own channel

// Reconfigures the radio to a new profile: standby first (required by
// SX126x before changing freq/SF/etc.), clears in-flight RX/TX state.
// On failure, best-effort reverts and returns false -- callers must check.
bool applyProfile(const Profile& p);
const Profile& currentProfile();

// dBm of the packet most recently returned by pollReceive(); -128 until first successful receive.
int16_t lastRssiDbm();

// Estimated time-on-air, ms, for a payload under the CURRENT profile.
uint32_t airtimeMs(size_t payloadChars);

// Non-blocking transmit -- radio_link only; blocking transmit() above is
// unaffected. beginTransmit() returns almost immediately; caller must poll
// txComplete() and call endTransmit() once true (or after an airtimeMs()-based deadline).
bool beginTransmit(const String& payload);
bool txInFlight();   // true from beginTransmit() until endTransmit()
bool txComplete();   // true once TX_DONE is latched -- caller should then call endTransmit()
bool endTransmit();

// Listen-before-talk: true if channel appears occupied (SX126x CAD).
// Leaves radio in standby. Reduces collisions; doesn't catch mid-payload packets or hidden terminals.
bool channelBusy();

} // namespace rf

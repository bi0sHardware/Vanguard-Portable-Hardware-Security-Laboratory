#pragma once
#include <Arduino.h>

// Shared framing for Missions 02-04's over-the-air link. Real AX.25 UI-frame
// layout (incl. 7-byte AX.25 addressing), deliberately never named "AX.25"
// participant-facing; callsigns VANGRD-7/GRNDCT-0 are fictional so a generic
// AX.25 decoder can't give the protocol away for free.
//
// Wire format: hex-encoded, and that hex string is the literal ASCII payload
// rf::transmit() puts on air (not raw binary).
//
//   7E | DEST(7B) | SRC(7B) | CTRL(03) | PID(F0) | INFO(N bytes) | FCS(2B) | 7E
//
// Fixed field positions: Information Field starts at hex offset 34 (17
// fixed bytes) and ends 6 hex chars before the end (FCS + closing flag).
namespace framecodec {

constexpr uint8_t kFlag = 0x7E;
constexpr uint8_t kCtrl = 0x03;  // UI frame control field
constexpr uint8_t kPid  = 0xF0;
constexpr size_t  kAddrBytes = 7; // 6-char callsign (shifted <<1) + SSID/extension byte

// Encodes one AX.25-style 7-byte address field: callsign (space-padded,
// each byte <<1), then an SSID byte with the address-extension bit.
// `last` sets that bit: true for SOURCE, false for DESTINATION.
void buildAddress(const char* callsign, uint8_t ssid, bool last, uint8_t out[kAddrBytes]);

// This link's two fixed stations, pre-encoded via buildAddress() so both
// ends of the link can't drift apart on addressing.
void satelliteAddress(bool last, uint8_t out[kAddrBytes]);
void groundAddress(bool last, uint8_t out[kAddrBytes]);

// Builds a complete frame and returns its hex-ASCII wire representation.
// `info` is the Information Field (already encrypted if applicable) -- this only frames it, never encrypts.
String buildFrame(const uint8_t dest[kAddrBytes], const uint8_t src[kAddrBytes],
                   uint8_t pid, const uint8_t* info, size_t infoLen);

// Parses a received hex string back into its fields. Returns false on a
// malformed string or wrong opening flag byte, so callers can silently
// ignore non-ours noise. destOut/srcOut stay in raw encoded form (compare
// via memcmp, not decoded). infoOut/infoCap bound the write; infoLen gets the actual count.
bool parseFrame(const String& hexPkt, uint8_t destOut[kAddrBytes], uint8_t srcOut[kAddrBytes],
                 uint8_t& pid, uint8_t* infoOut, size_t infoCap, size_t& infoLen);

// XORs `len` bytes in place with a single-byte key (self-inverse: same fn for encrypt/decrypt).
void xorInPlace(uint8_t* buf, size_t len, uint8_t key);

// True if every byte is printable ASCII (0x20-0x7E). Used to distinguish
// plaintext packets 1-5 from XOR-0x54-encrypted packets 6-10 without decoding.
bool isPrintableAscii(const uint8_t* buf, size_t len);

// ---------------------------------------------------------------------
// Radio Chat / Ship Battle additions. buildFrame()/parseFrame() above are
// UNCHANGED -- parseFrame() still never checks the FCS, since Missions
// 02-04 depend on it accepting frames that would otherwise be rejected.
// ---------------------------------------------------------------------

// The frame's additive checksum over dest/src/ctrl/pid/info. Not a
// cryptographic check -- mainly catches truncation/hex-decode desync.
uint16_t computeFcs(const uint8_t dest[kAddrBytes], const uint8_t src[kAddrBytes],
                     uint8_t pid, const uint8_t* info, size_t infoLen);

// True if `hexPkt` parses and its trailing FCS matches computeFcs() over
// its full content (unlike parseFrame(), no truncation to a caller cap).
bool verifyFcs(const String& hexPkt);

} // namespace framecodec

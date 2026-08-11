#pragma once
#include <cstdint>
#include <cstddef>

// Wire format ONLY for the badge-to-badge link (Radio Chat, Ship Battle) --
// zero ESP-IDF/Arduino dependencies so it can be reused by a future Pi
// ground-station. This header is the wire contract itself.
//
// Layered inside framecodec's own INFO field (rides on top, not a replacement):
//
//   framecodec frame: 7E | DEST(7B) | SRC(7B) | CTRL | PID | INFO(N) | FCS | 7E
//                                                        ^ this file describes INFO's layout
//
// DEST/SRC are built via framecodec::buildAddress() using a 6-hex-char
// badge link ID as the callsign and kSsidBadgeLink as the SSID nibble
// (actual construction lives in radio_link.cpp).
namespace radiolink {

// ---- addressing -----------------------------------------------------

// Service selector in framecodec address SSID nibbles. Existing
// mission/satellite traffic uses kSsidMission/kSsidSatellite (untouched),
// listed here so every reservation is visible in one place.
constexpr uint8_t kSsidMission   = 0;
constexpr uint8_t kSsidSatellite = 7;
constexpr uint8_t kSsidBadgeLink = 1; // everything in this file
constexpr uint8_t kSsidPiGateway = 2; // reserved for a future Pi relay/ground-station (see §below)
// 3..6, 8..14 unused/reserved. 15 = experimental/dev builds.

// Broadcast destination callsign. Link IDs are always 6 hex digits
// [0-9A-F]; "ALLBDG" contains non-hex letters (L, G), so it can never
// collide with a real badge's link ID.
constexpr const char* kBroadcastId = "ALLBDG";
// Reserved callsign for a future Pi ground-station node -- same
// non-collision trick (P, I, S, T are non-hex).
constexpr const char* kPiGatewayId = "PISAT0";

// ---- INFO sub-header (6 bytes, fixed) --------------------------------
//
//   off  size  field
//   0    1     verType   bits 7..5 = version (kVersion), bits 4..0 = Type
//   1    1     seq       per-sender, increments per NEW message (not retries), wraps 255->0
//   2    1     flags     see kFlag* below
//   3    1     session   0 = sessionless; else an app-defined game/conversation id
//   4    1     ackSeq    seq being acknowledged, valid iff flags & kFlagIsAck
//   5    1     payLen    payload byte count (redundant with parseFrame's own
//                        infoLen ON PURPOSE -- parseFrame() truncates rather
//                        than rejects an oversized frame; this catches that)
//   6..  N     payload   0..kMaxPayload bytes, meaning depends on Type
//
// Fixed-size header (no variable-length fields before payload) keeps
// parsing branch-free and makes a future v2 a pure append, not a reshuffle.
constexpr uint8_t kVersion     = 1;
constexpr uint8_t kPid         = 0xBB; // distinct from framecodec::kPid; belt-and-suspenders (receivers ignore PID anyway)
constexpr size_t  kHeaderBytes = 6;
constexpr size_t  kMaxPayload  = 96;   // INFO practical ceiling ~107B; 6 + 96 = 102

enum class Type : uint8_t {
    // ---- 0x00-0x07: transport, presence, chat ----
    Reserved    = 0x00,
    Beacon      = 0x01,  // [nameLen:1][name: 0..16 ASCII][caps:1]
    BeaconReq   = 0x02,  // (no payload) active-scan poke -- "is anyone there, beacon now"
    Ack         = 0x03,  // [status:1] (AckStatus) -- ackSeq in the header identifies what's being acked
    TextMsg     = 0x04,  // [text: ASCII, 1..80 bytes]
    MorseMsg    = 0x05,  // [morse: ASCII from ".-/ ", 1..80 bytes]
    Ping        = 0x06,  // (no payload) keepalive / liveness probe
    Bye         = 0x07,  // [reason:1] -- leaving the channel or the open session
    // 0x08-0x0F reserved for a future Pi gateway (TimeSync, ScoreReport,
    // Announce, StoreAndForwardPoll, WhoHas, ...) -- do not allocate a new
    // feature into this range without updating this comment.

    // ---- 0x10-0x1F: Ship Battle ----
    HostAdv     = 0x10,  // [session:1][rules:1][nameLen:1][name:0..16]
    JoinReq     = 0x11,  // [session:1] (echoes the host's advertised session)
    JoinAccept  = 0x12,  // [session:1][boardW:1][boardH:1][fleetMask:1][firstMover:1][secret:4]
    JoinReject  = 0x13,  // [reason:1]
    FleetReady  = 0x14,  // [fleetHashHi:1][fleetHashLo:1]
    Fire        = 0x15,  // [turn:1][cell:1]                                  -- cell = (row<<4)|col
    FireResult  = 0x16,  // [turn:1][cell:1][result:1][shipId:1][remaining:1]
    GameSync    = 0x17,  // [turn:1][whoseTurn:1][myRemaining:1][lastCell:1][lastResult:1]
    GameOver    = 0x18,  // [reason:1][winner:1]
    Forfeit     = 0x19,  // [reason:1]
    // 0x1A-0x1F reserved for a Ship Battle v2 (do not reuse for anything else).

    // ---- 0x09-0x0B: Badge Attack (see main/glitch/glitch.h) ----
    // Borrowed from the unused Pi-gateway range; verType's Type field is
    // only 5 bits (0..0x1F), so 0x20 would alias to 0x00 (a bug this once
    // caused). Sent via sendReliable(); the unlock condition specifically
    // needs AttackLanded back, not just the transport ack, since the ack
    // can't distinguish "landed" from "defended". No payload on any of the three.
    Attack        = 0x09,
    AttackBlocked = 0x0A, // defender's reply: isSecureBadgeEnabled() was true, no glitch fired
    AttackLanded  = 0x0B, // defender's reply: not defended, glitch::trigger() fired
};

// flags bits
constexpr uint8_t kFlagAckReq   = 0x01; // sender wants an Ack (or a piggybacked kFlagIsAck) for this seq
constexpr uint8_t kFlagIsAck    = 0x02; // this packet's ackSeq field is meaningful (bare Ack, or piggybacked on any other type)
constexpr uint8_t kFlagRetry    = 0x04; // this is a retransmission of the same seq, not a new message
constexpr uint8_t kFlagAuth     = 0x08; // last 2 payload bytes are a truncated session MAC (Ship Battle only, optional)
constexpr uint8_t kFlagHopMask  = 0x30; // 2-bit hop counter, reserved for a future Pi digipeater/relay
constexpr uint8_t kFlagHopShift = 4;
// 0x40, 0x80 reserved.

enum class AckStatus : uint8_t { Ok = 0, Busy = 1, BadVersion = 2, Refused = 3 };
enum class FireResultCode : uint8_t { Miss = 0, Hit = 1, Sunk = 2, Invalid = 3, Repeat = 4 };
enum class OverReason : uint8_t { FleetDestroyed = 0, Forfeit = 1, PeerLost = 2, Aborted = 3 };

// ---- forward-compatibility notes --
// A future Pi ground-station is just another addressable node, no format
// change needed. kFlagHopMask is a hop counter, NOT AX.25's real digipeater
// mechanism -- parseFrame() assumes exactly two fixed address fields, so a
// third would corrupt existing parses. The version nibble lets a v1 badge
// ignore a v2 frame instead of misparsing it.

} // namespace radiolink

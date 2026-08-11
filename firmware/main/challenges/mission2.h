#pragma once

// Level 2 — Satellite Reconnaissance, driven by rf:: (LoRa).
//  1. Forwards only the printable/plaintext frames (Packets 1-5) of the
//     satellite's 10-frame loop to Mission Control over USB Serial; the
//     encrypted half (6-10) is Mission 03's to forward.
//  2. "Protocol Verification" prompt sequence validates the frame fields
//     the player recovered from raw hex. Success reveals the flag on-device;
//     player still submits via FlagDesk to officially mark it complete.
//
// Simplification vs spec: merges "Signal Locked"/"Streaming" into one
// screen, and treats "Connected" as first byte seen on Serial (no reliable
// USB-CDC host-listening signal available).
namespace mission2 {

void enter();
bool frame(); // returns true exactly on the tick BACK returns to the stage list

} // namespace mission2

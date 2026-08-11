#pragma once

// Level 4 — Operation Vanguard.
// Satellite streams a payload over LoRa in numbered chunks; badge
// reassembles in RAM and serves it over USB Serial on request.
//
// LoRa chunk format (satellite_sim/): VGDIMG:<index>:<total>:<byte offset>:<hex bytes>
// Offset is explicit (not index*stride) since the badge can start
// listening mid-stream and chunks may arrive out of order/repeated —
// each is written at its own offset and tracked in a received-bitmap.
//
// USB Serial commands (case-insensitive, newline-terminated):
//   STATUS    - report chunk progress
//   DOWNLOAD  - dump assembled payload as hex, BEGIN/END framed
//   RESET     - discard partial payload and listen again
//
// Payload is opaque to the firmware; the badge never decodes/validates it.
namespace mission4 {

void enter();
bool frame(); // returns true exactly on the tick BACK returns to the stage list

} // namespace mission4

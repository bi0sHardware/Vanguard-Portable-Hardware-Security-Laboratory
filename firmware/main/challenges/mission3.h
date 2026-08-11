#pragma once

// Level 3 — Establishing the Uplink.
//
// Mirrors Mission 02's receive flow exactly, but forwards the
// non-printable/encrypted half of the loop (Packets 6-10) instead of the
// printable half. Badge never decrypts/labels anything — player recovers
// the key and directive themselves from raw hex.
//
// Second clue is a physical silkscreen inscription on the PCB (not
// reproduced in firmware/OLED/serial) that the player translates and
// XOR-encrypts with the uplink key to form the expected payload.
//
// Badge doesn't perform that encryption; player submits the ciphertext as
// a hex line over PuTTY (OK -> "Submit Uplink"). Screen decodes, frames,
// and transmits it verbatim; ack reveals the flag and arms Mission 04.
namespace mission3 {

void enter();
bool frame(); // returns true exactly on the tick BACK returns to the stage list

} // namespace mission3

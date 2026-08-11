#pragma once

// Flag submission desk — PuTTY text UI over USB-CDC Serial. Validates
// against challenge_data.h's registry (data-driven, no per-level logic
// here). A correct flag calls challenges::completeChallenge(), the only
// path that marks a stage done and updates LEDs/notification.
// This is only a local pass/fail check; external scoring is separate.
namespace flagdesk {

void enter();
bool frame(); // returns true exactly on the tick BACK is pressed on-device

} // namespace flagdesk

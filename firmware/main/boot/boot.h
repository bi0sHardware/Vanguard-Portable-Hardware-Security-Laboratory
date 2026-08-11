#pragma once
#include "../../include/state.h"

// Boot sequence (animation regenerated via tools/gen_boot_anim.py):
//  1. Boot animation: JPEG-flipbook "video" (no video decoder on the badge), LED sweep + chime start with it,
//     a second LED effect kicks in at the animation's logo-reveal moment (kRevealAtMs).
//  2. Screensaver (home screen, ui/screensaver.h), composites the Vanguard logo directly.
// Runs as a normal frame()-ticked AppState, millis()-gated with no delay(), so input/watchdog stay live.
namespace boot {

void enter();
AppState frame(); // returns AppState::Boot while still in progress, then AppState::Screensaver

} // namespace boot

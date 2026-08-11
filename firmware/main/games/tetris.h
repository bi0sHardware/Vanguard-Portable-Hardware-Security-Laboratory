#pragma once
#include "../../include/state.h"

// Tetris — spec §8.2/§8.3: LEFT/RIGHT=move, DOWN=soft drop, OK(S5)=rotate
// (unified scheme: joystick is movement-only, OK is the rotate action),
// PAUSE=pause, BACK=exit, START=start/restart. Backside LEDs (LED9-14) show
// live stack height (spec §6).
namespace games::tetris {

void enter();
AppState frame();

} // namespace games::tetris

#pragma once
#include "../../include/state.h"

// Snake — spec §8.3: UP/DOWN/LEFT/RIGHT=direction, PAUSE=pause, BACK=exit,
// START=start/restart, SFX for eat/crash.
namespace games::snake {

void enter();
AppState frame();

} // namespace games::snake

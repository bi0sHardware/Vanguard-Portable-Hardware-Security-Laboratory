#pragma once
#include "../../include/state.h"

// Space Shooter — spec §8.3: LEFT/RIGHT=move ship, OK=fire, PAUSE=pause,
// BACK=exit, START=start/restart. New addition, arcade sci-fi tie-in.
namespace games::space_shooter {

void enter();
AppState frame();

} // namespace games::space_shooter

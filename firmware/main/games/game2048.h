#pragma once
#include "../../include/state.h"

// 2048 — classic sliding-tile puzzle. UP/DOWN/LEFT/RIGHT=slide, PAUSE=pause,
// BACK=exit, START=start/restart. Same enter()/frame() shape as the other
// three games (tetris/snake/space_shooter) so main.cpp's dispatch tables
// stay uniform.
namespace games::game2048 {

void enter();
AppState frame();

} // namespace games::game2048

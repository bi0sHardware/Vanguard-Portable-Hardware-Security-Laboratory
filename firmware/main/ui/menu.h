#pragma once
#include "../../include/state.h"

// Main menu + Games submenu — spec §8.1: 6-option main menu; Games opens a submenu instead of jumping straight into a game.
namespace ui {

void mainMenuEnter();
AppState mainMenuFrame(); // returns next state (self if staying)

void gamesMenuEnter();
AppState gamesMenuFrame();

} // namespace ui

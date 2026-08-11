#pragma once
#include "../../include/state.h"

// Ship Battle -- LoRa multiplayer Battleship. Presentation only; protocol
// and turn-state logic live in ship_battle_net.h/.cpp.
namespace games::ship_battle {

void enter();
AppState frame();

} // namespace games::ship_battle

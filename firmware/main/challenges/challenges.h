#pragma once
#include "../../include/state.h"

// Challenges — spec §8.1, §9: entered from main menu, exits on BACK. LoRa
// TX/RX isn't a standalone menu entry; it's encountered in-context during
// Stage 2/3.
namespace challenges {

void enter();
AppState frame();

} // namespace challenges

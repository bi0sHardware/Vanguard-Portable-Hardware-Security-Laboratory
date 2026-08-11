#pragma once
#include "../../include/state.h"

// Full-screen takeover shown once, when all four Challenge levels complete
// (see challenge_engine.h's consumeMissionCompleteEvent()). Not reachable
// from the menu tree; one-shot celebration, not a normal screen.
namespace mission_complete {

void enter();
AppState frame(); // returns MainMenu once dismissed, MissionComplete otherwise

} // namespace mission_complete

#pragma once
#include "../../include/state.h"

// Retro Music Player: track library (see tracks.h) with pitch-to-LED visualizer
// (bass=red, mid=green, treble=white). Monophonic chiptune melody via the buzzer.
namespace music {

void enter();
AppState frame();

} // namespace music

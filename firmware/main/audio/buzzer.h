#pragma once
#include <cstdint>

// BUZZER1 (GPIO7) drives a passive piezo buzzer — pitch comes from the bit-banged
// drive square wave's frequency, no LEDC dependency.
namespace buzzer {

void init();

// Non-blocking continuous tone via hardware timer ISR so playback doesn't stall the
// main loop. Call stopAsync() or toneAsync(0) to silence.
void toneAsync(unsigned int freqHz);
void stopAsync();

void off();

} // namespace buzzer

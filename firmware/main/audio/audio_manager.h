#pragma once
#include <cstdint>

// Shared non-blocking melody/SFX player (wraps buzzer::toneAsync/stopAsync) used by
// all sound-producing screens. No priority/ducking logic: audio sources are mutually
// exclusive AppStates, so a new SFX/melody simply replaces whatever was playing.
namespace audio {

struct Note {
    uint16_t freqHz;     // 0 = rest (silence for durationMs)
    uint16_t durationMs;
};

void init();

// Call once per main loop tick; never blocks.
void update();

// Fire-and-forget short sound effect (single note or short fixed sequence).
// `notes` must outlive playback (pass a static/const table, not a stack array
// that goes out of scope) — the manager stores the pointer, not a copy.
void playSfx(const Note* notes, uint8_t count);

// Longer melody. onNoteChange (optional) fires on each note advance — used by the
// music player to drive the LED pitch visualizer without this module knowing about LEDs.
void playMelody(const Note* notes, uint16_t count, bool loop,
                 void (*onNoteChange)(int index, uint16_t freqHz, void* ctx) = nullptr,
                 void* ctx = nullptr);

void stop();
bool isPlaying();

// Pause/resume in place (unlike stop(), doesn't reset to note 0). resume() restarts
// just the current note, not the whole melody. Both no-op if already in that state.
// isPlaying() stays true while paused; only stop() clears it.
void pause();
void resume();
bool isPaused();

// Global sound on/off (NVS "settings"/"sound"), centralized so all sound paths respect
// one toggle. Muting still advances note timing/callbacks; only silences the buzzer.
void setSoundEnabled(bool enabled);
bool isSoundEnabled();

} // namespace audio

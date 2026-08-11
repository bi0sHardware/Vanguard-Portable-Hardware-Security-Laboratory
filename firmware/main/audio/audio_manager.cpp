#include "audio_manager.h"
#include "buzzer.h"
#include "../storage/storage.h"
#include <Arduino.h>
#include <cstddef>

namespace audio {

static bool s_soundEnabled = true;
static const Note* s_notes = nullptr;
static uint16_t s_count = 0;
static uint16_t s_index = 0;
static bool s_loop = false;
static bool s_playing = false;
static bool s_paused = false;
static unsigned long s_noteStartMs = 0;
static bool s_gapApplied = false;
static void (*s_onNoteChange)(int, uint16_t, void*) = nullptr;
static void* s_ctx = nullptr;

static void startNoteAt(uint16_t index) {
    s_index = index;
    s_noteStartMs = millis();
    s_gapApplied = false;
    uint16_t freq = s_notes[index].freqHz;

    if (freq == 0 || !s_soundEnabled) buzzer::stopAsync();
    else buzzer::toneAsync(freq);

    // Fires regardless of sound toggle so the LED visualizer stays in sync while muted.
    if (s_onNoteChange) s_onNoteChange((int)index, freq, s_ctx);
}

void init() {
    buzzer::init();
    s_soundEnabled = storage::loadSoundEnabled();
}

void update() {
    if (!s_playing || s_paused) return;

    unsigned long now = millis();
    unsigned long elapsed = now - s_noteStartMs;
    uint16_t duration = s_notes[s_index].durationMs;

    // Cut tone short for the last ~15% of the note so consecutive same-pitch notes stay separated.
    if (!s_gapApplied && s_notes[s_index].freqHz != 0) {
        unsigned long toneMs = (duration > 20) ? (duration * 85UL / 100UL) : duration;
        if (elapsed >= toneMs) {
            buzzer::stopAsync();
            s_gapApplied = true;
        }
    }

    if (elapsed < duration) return;

    uint16_t next = s_index + 1;
    if (next >= s_count) {
        if (s_loop) {
            next = 0;
        } else {
            stop();
            return;
        }
    }
    startNoteAt(next);
}

static void begin(const Note* notes, uint16_t count, bool loop,
                   void (*onNoteChange)(int, uint16_t, void*), void* ctx) {
    if (count == 0 || notes == nullptr) {
        stop();
        return;
    }
    s_notes = notes;
    s_count = count;
    s_loop = loop;
    s_onNoteChange = onNoteChange;
    s_ctx = ctx;
    s_playing = true;
    startNoteAt(0);
}

void playSfx(const Note* notes, uint8_t count) {
    begin(notes, count, false, nullptr, nullptr);
}

void playMelody(const Note* notes, uint16_t count, bool loop,
                void (*onNoteChange)(int, uint16_t, void*), void* ctx) {
    begin(notes, count, loop, onNoteChange, ctx);
}

void stop() {
    s_playing = false;
    s_paused = false;
    s_notes = nullptr;
    s_count = 0;
    s_index = 0;
    s_onNoteChange = nullptr;
    s_ctx = nullptr;
    buzzer::stopAsync();
}

bool isPlaying() { return s_playing; }

void pause() {
    if (!s_playing || s_paused) return;
    s_paused = true;
    buzzer::stopAsync();
}

void resume() {
    if (!s_playing || !s_paused) return;
    s_paused = false;
    startNoteAt(s_index);
}

bool isPaused() { return s_paused; }

void setSoundEnabled(bool enabled) {
    s_soundEnabled = enabled;
    if (!enabled) buzzer::stopAsync(); // silence immediately, even mid-note
}

bool isSoundEnabled() { return s_soundEnabled; }

} // namespace audio

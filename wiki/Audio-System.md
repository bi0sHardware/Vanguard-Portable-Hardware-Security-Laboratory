# Audio System

**Module:** `firmware/main/audio/audio_manager.*`, `firmware/main/audio/buzzer.*`

## Purpose

Buzzer-driven melody and short-SFX playback, shared by every feature that
needs sound (boot chime, music player, game SFX, UI feedback).

## User flow

Not directly user-facing beyond a global sound on/off toggle in Settings.

## Technical design

One shared player: a new sound replaces whatever is currently active
rather than mixing or queuing, matching the badge's single-buzzer
hardware. `audio::playMelody()` supports looping and a note-change
callback used by features that need to react to playback progress (e.g.
the Music Player's elapsed-time display and LED pitch visualizer).

## Dependencies

Buzzer hardware (PWM-driven tone output).

## Storage usage

None — the sound on/off preference is stored by [Settings](Settings.md),
not by the audio subsystem itself.

## Known limitations

No polyphony or mixing — a single active sound at a time, by hardware
design, not a software limitation to be lifted later.

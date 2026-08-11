# Music Player

**Module:** `firmware/main/music/music.*`, `firmware/main/music/tracks.h`

## Purpose

Chiptune playback with an LED pitch visualizer, presented as a retro
cassette-tape player UI.

## User flow

Browse tracks (Up/Down while stopped), Ok to play/stop. No true pause
exists in the underlying audio engine — it is play/stop only, so
"Paused" in the UI is represented as "Stopped" rather than inventing
pause/resume state the engine doesn't support.

## Technical design

Elapsed track time is recomputed from the note index the playback
engine's `onNoteChange` callback reports, summing the (small) track's own
note table each callback rather than requiring new state from the
playback engine. A pitch-to-LED visualizer maps note frequency ranges to
bass/mid/treble LED colors.

## Dependencies

`audio::`, `led::`, [Cassette Animation](Cassette-Animation.md) for the
visual shell.

## Storage usage

None — track selection is not persisted across sessions.

## Known limitations

No pause/resume distinct from stop, by design (the playback engine has
no pause primitive).

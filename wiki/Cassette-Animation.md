# Cassette Animation

**Module:** `firmware/main/music/music.cpp` (rendering portion)

## Purpose

The visual shell for the [Music Player](Music-Player.md): a cassette-tape
body with spinning reels and a track-switch eject/load transition.

## User flow

Purely visual — reels spin continuously while a track plays; switching
tracks briefly shows an "EJECTING…" then "LOADING…" state card before the
next track's reel view appears.

## Technical design

The display has no local framebuffer, so every draw is a direct SPI
write; a pixel-by-pixel slide transition would mean repainting a large
area from scratch on every tick, which visibly flashes. Track switching
is instead two brief static states held for a few hundred milliseconds
each, matching a reference mockup, avoiding both the flash and any
residue from a moving shell crossing into the static header row.

Reel rotation uses fixed angle steps ticked independently of note timing,
so it stays a smooth, steady spin regardless of which notes are currently
playing, and only the reel body repaints when the wound-tape amount
changes — the spokes, which move every tick, are erased/redrawn as three
short lines rather than the whole circle.

## Dependencies

`display::`, `theme::` (shell/accent colors).

## Storage usage

None.

## Known limitations

Tied to the display's direct-draw, no-framebuffer constraint — see
[`docs/architecture/display-rendering.md`](../docs/architecture/display-rendering.md).

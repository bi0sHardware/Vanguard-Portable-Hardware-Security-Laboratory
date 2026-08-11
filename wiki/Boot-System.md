# Boot System

**Module:** `firmware/main/boot/`

## Purpose

Plays the badge's boot animation and hands off into the Screensaver home
screen.

## User flow

Power-on → boot animation (full-screen JPEG flipbook, since the badge has
no video decoder) with a synchronized LED sweep and chime → automatic
transition into the Screensaver. No user input is required or expected
during boot.

## Technical design

Implemented as a normal `frame()`-ticked `AppState`, not a one-shot
blocking call from `setup()` — timing is `millis()`-gated with no
`delay()` in the path, so input polling and the watchdog stay live
throughout. The animation buffer is a single reused 320×240 RGB565
frame (not one buffer per frame), freed once the animation completes.
Audio and LED cues are timed to specific animation milestones
(reveal, outro) rather than fixed offsets from start/end, computed by
proportionally scaling from the source animation's native timeline.

## Dependencies

`display::`, `led::`, `audio::`, the JPEG decoder component, and the
generated frame data in `boot_anim_frames/` (regenerated via
`tools/gen_boot_anim.py`).

## Storage usage

None — boot has no persistent state of its own.

## Known limitations

Boot animation timing constants are derived from the specific source
video used to generate `boot_anim_frames/`; regenerating the animation
from a different source requires re-deriving those constants (documented
in `boot.cpp`).

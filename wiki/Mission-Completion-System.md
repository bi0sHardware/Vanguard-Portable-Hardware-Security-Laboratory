# Mission Completion System

**Module:** `firmware/main/challenges/mission_complete.*`,
`challenges::consumeMissionCompleteEvent()`

## Purpose

Detects the moment all four challenge levels become complete and
transitions the badge into a dedicated completion screen.

## User flow

Completing Level 4 (with Levels 1–3 already complete) triggers an
automatic transition to the Mission Complete screen on the next tick, no
matter what screen the player is currently on.

## Technical design

`consumeMissionCompleteEvent()` returns true exactly once, on the tick
all four levels first become complete. `main.cpp` polls it every loop
iteration at high priority and force-switches `AppState` to
`MissionComplete`, the same cross-cutting pre-emption pattern used by the
Badge Attack trigger.

## Dependencies

`challenges::` (completion tracking), `led::`/`audio::` for the
completion presentation.

## Storage usage

Reads the same per-stage completion flags in the `challenges` NVS
namespace that drive the unlock chain.

## Known limitations

None beyond the general challenge-framework limitations tracked in
[`docs/release/RELEASE-1.0-CERTIFICATION.md`](../docs/release/RELEASE-1.0-CERTIFICATION.md).

# Screensaver

**Module:** `firmware/main/ui/screensaver.*`, `firmware/main/ui/starfield.*`

## Purpose

The badge's permanent home screen. Boot lands here, and every major
section's Back button returns here.

## User flow

Shows an idle starfield/logo animation compositing the Vanguard logo.
Any joystick movement or Select opens the Main Menu; Ok opens Profile
(WiFi) Setup directly as a shortcut.

## Technical design

Implemented as its own always-reachable `AppState` (not an idle-timeout
overlay scoped inside the Main Menu), so it is reachable directly from
boot rather than only after a period of inactivity. The starfield
rendering uses a fixed set of procedurally-placed elements (stars,
planets, moons) redrawn incrementally rather than a full-screen repaint
every tick.

## Dependencies

`ui::` renderer/widgets, `display::`.

## Storage usage

None.

## Known limitations

None beyond the general display/rendering constraints described in
[Storage System](Storage-System.md) and
[`docs/architecture/display-rendering.md`](../docs/architecture/display-rendering.md).

## Future extension points

Additional idle-state visual themes could be added as alternate
starfield/backdrop renderers without changing the screen's role as the
permanent home state.

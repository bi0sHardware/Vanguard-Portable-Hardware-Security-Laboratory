# Notification System

## Purpose

There is no single, dedicated "notification system" module in this
firmware. Notification-like feedback is composed from two existing
building blocks: the shared popup/overlay widget base used for
pause/confirm/game-over-style overlays (`firmware/main/ui/widgets.h`),
and dedicated LED effect classes (`led::EffectId::Notification`,
`led::EffectId::Warning`) that blink continuously until explicitly
stopped.

## User flow

Varies by feature — e.g. a confirm overlay in a game, or a continuous LED
blink flagging an unread state — each feature composes its own
notification behavior from these primitives rather than going through a
shared queue or manager.

## Technical design

See [LED System](LED-System.md) for the effect primitives and
`firmware/main/ui/widgets.h` for the overlay base class.

## Dependencies

`led::`, `ui::` widgets.

## Storage usage

None.

## Known limitations

There is no unified notification queue, priority system, or
cross-feature notification manager — this is intentionally documented as
a gap rather than an undocumented subsystem, since no such system exists
in the current firmware.

## Future extension points

A shared notification manager (queuing, priority, dismissal) could be
built on top of the existing LED effect and overlay-widget primitives
without changing how individual features already request them.

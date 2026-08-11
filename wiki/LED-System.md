# LED System

**Module:** `firmware/main/leds/led_chain.*`, `firmware/main/leds/led_manager.*`

## Purpose

Non-blocking, named LED effects on top of the badge's 14-LED addressable
chain, so screens request an effect by name instead of hand-rolling
`delay()`-based sequencing.

## User flow

Not directly user-facing — LED effects accompany actions across nearly
every feature (boot sweep, PeerDrop exchange pulse, challenge completion,
Radio Chat TX indicator, Badge Attack).

## Technical design

Effects are generic — any module can request any effect. The chain is
addressed via bit-group masks (red/green/white segments) derived from the
physical LED-to-bit mapping. Effect classes include continuous states
(e.g. `Notification`, `Warning` — fast/faster blink until explicitly
stopped) alongside one-shot effects.

## Dependencies

The physical LED chain driver (`led_chain.*`).

## Storage usage

None.

## Known limitations

Effects are cooperative — a screen that requests a new effect implicitly
replaces whatever effect was previously running; there is no effect
priority/stacking system.

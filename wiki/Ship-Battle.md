# Ship Battle

**Module:** `firmware/main/games/ship_battle.*` (presentation),
`firmware/main/games/ship_battle_net.*` (protocol/turn state)

## Purpose

A badge-to-badge multiplayer Battleship game built on the same LoRa
`radiolink` layer as Radio Chat, with an optional Badge Attack minigame
layered on top.

## User flow

Host/join a game against a nearby badge, place a fleet, then take turns
firing at grid coordinates until one fleet is sunk.

## Technical design

Presentation (`ship_battle.cpp`) and protocol/turn-state
(`ship_battle_net.cpp`) are deliberately separate. Games are identified
by the pair (peer link ID, session ID) together, never session ID alone,
since two hosts could coincidentally pick the same session ID. Turn
resolution is idempotent: the defender caches the last (turn, outcome) it
resolved and resends that result if the same turn's Fire message arrives
again (a lost result or attacker retry), rather than re-resolving and
risking a double-counted hit.

## Dependencies

`radiolink` (`firmware/main/rf/radio_link.*`), `led::`, `audio::`.

## Storage usage

No persistent state — a game session lives only for its duration.

## Known limitations

Physical two-badge play-testing is tracked as pending in
[`docs/release/RELEASE-1.0-CERTIFICATION.md`](../docs/release/RELEASE-1.0-CERTIFICATION.md).

## Future extension points

The Badge Attack minigame reuses the same shared badge-link channel as
Radio Chat, so any future game built on `radiolink` can plug into the
same attack/defend mechanic without new wire-protocol work.

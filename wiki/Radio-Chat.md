# Radio Chat

**Module:** `firmware/main/radiochat/radio_chat.*`

## Purpose

A handheld LoRa messenger built on the shared `radiolink` link layer,
with live peer discovery and a manual Morse/CW mode (see
[Morse Mode](Morse-Mode.md)).

## User flow

Home screen shows nearby peers sorted by live RSSI. Selecting a peer
opens a contact picker leading into messaging; Quick Messages offers a
fast preset-message path. An optional Badge Attack minigame can be
triggered against another badge with Radio Chat or Ship Battle open (see
[Ship Battle](Ship-Battle.md)).

## Technical design

Peer rows are re-sorted by live RSSI on every call — since RSSI
fluctuates continuously, the selected peer is tracked by its stable link
ID and re-located in the (possibly reordered) table every tick, rather
than trusting a bare row index to still mean the same peer. See
[`docs/protocols/radio-chat.md`](../docs/protocols/radio-chat.md) for the
wire-level design.

## Dependencies

`radiolink` (`firmware/main/rf/radio_link.*`), `led::`, `audio::`,
`contacts`/`storage::` for saved peers.

## Storage usage

No dedicated persistence of its own beyond contacts saved via PeerDrop.

## Known limitations

See [`docs/protocols/radio-chat.md`](../docs/protocols/radio-chat.md) and
the pending hardware-validation items in
[`docs/release/RELEASE-1.0-CERTIFICATION.md`](../docs/release/RELEASE-1.0-CERTIFICATION.md).

## Future extension points

The link layer's addressing scheme was designed with external reuse in
mind (see `radio_link_types.h`), leaving room for a non-badge listener
(e.g. a ground-station receiver) in the future.

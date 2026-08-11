# PeerDrop

**Module:** `firmware/main/peerdrop/peerdrop.*`

## Purpose

BLE (NimBLE) contact exchange between two nearby badges: pick Receive to
grab a nearby badge's contact, Send to share your own.

## User flow

Explicit Send/Receive roles (not automatic mutual discovery). The
receiving badge scans for nearby senders; RSSI-based proximity gates
selectability, near-contact hold, and true-touch instant sharing at three
distinct thresholds. Both sides see a confirm step before the exchange
completes.

## Technical design

State flow: Scanning → Confirm → Waiting → Exchanging → Success/Failure.
`NimBLEClient::connect()` blocks the whole badge while connecting (input
included), so the connect path is bounded by an explicit timeout rather
than left unbounded. See
[`docs/protocols/peerdrop.md`](../docs/protocols/peerdrop.md) and
[`docs/architecture/ble-subsystem.md`](../docs/architecture/ble-subsystem.md).

## Dependencies

NimBLE, `storage::` (contact persistence), `led::`/`audio::` for
exchange feedback.

## Storage usage

Writes accepted contacts into the `contacts` NVS namespace, consumed by
[Contacts](Contacts.md).

## Known limitations

LoRa and BLE coexistence/mutual-exclusivity constraints apply — see
[`docs/architecture/ble-subsystem.md`](../docs/architecture/ble-subsystem.md).
Physical two-badge exchange verification is tracked as pending in
[`docs/release/RELEASE-1.0-CERTIFICATION.md`](../docs/release/RELEASE-1.0-CERTIFICATION.md).

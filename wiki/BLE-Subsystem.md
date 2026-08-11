# BLE Subsystem

**Module:** `firmware/main/peerdrop/peerdrop.*` (NimBLE integration)

## Purpose

Bluetooth Low Energy stack underlying [PeerDrop](PeerDrop.md).

## User flow

Not directly user-facing — see [PeerDrop](PeerDrop.md) for the exchange
flow built on top of it.

## Technical design

Built on NimBLE (`esp-nimble-cpp`). See
[`docs/architecture/ble-subsystem.md`](../docs/architecture/ble-subsystem.md)
for integration details, including LoRa/BLE coexistence considerations
and the blocking nature of `NimBLEClient::connect()`.

## Dependencies

`esp-nimble-cpp` / NimBLE.

## Storage usage

None directly — persistence of exchanged contacts is handled by
[PeerDrop](PeerDrop.md) via [Storage System](Storage-System.md).

## Known limitations

See [`docs/architecture/ble-subsystem.md`](../docs/architecture/ble-subsystem.md).

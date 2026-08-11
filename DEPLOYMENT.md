# Deployment

This document covers taking Vanguard firmware from a clean build to
badges ready for participants. For step-by-step detail, see
[`docs/deployment/`](docs/deployment/).

## Badge flashing

```bash
./tools/build.sh
./tools/flash.sh /dev/ttyACM0
```

`flash.sh` writes the firmware and erases the NVS partition (identity,
contacts, challenge progress, settings) on every flash, so every badge it
touches comes out fresh. See [Badge Provisioning](docs/deployment/badge-provisioning.md)
for first-time hardware bring-up, which additionally requires a one-time
eFuse fix.

## Badge validation

Before an event:

- Confirm the firmware boots to the Screensaver and the menu system is
  reachable.
- Confirm LoRa self-test passes (`Radio Offline` should not appear on any
  RF-dependent screen).
- Confirm Level 1's UART leak is observable with a logic analyzer on
  production hardware — this is the one verification step that requires
  physical instrumentation, not just firmware behavior, and it gates
  every subsequent challenge level.

## Deployment preparation

1. Build and flash a satellite simulator (required for Levels 2–4) — see
   [Satellite Simulator](docs/deployment/satellite-simulator.md).
2. Provision player badges — see [Badge Provisioning](docs/deployment/badge-provisioning.md).
3. Walk the [Competition Deployment Checklist](docs/deployment/event-checklist.md).

## Production checklist

See [`docs/release/RELEASE-1.0-CERTIFICATION.md`](docs/release/RELEASE-1.0-CERTIFICATION.md)
for what has and has not been verified ahead of a given release.

## Recovery procedures

See [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md) — in particular, the LoRa
radio has no firmware-accessible reset line, so a stuck radio needs a
power cycle, not a reflash.

## Competition deployment checklist

See [`docs/deployment/event-checklist.md`](docs/deployment/event-checklist.md).

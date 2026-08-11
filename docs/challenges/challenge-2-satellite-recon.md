# Level 2 — Satellite Recon

**Category:** RF / LoRa / Satellite Comms
**Module:** `firmware/main/challenges/mission2.cpp`
**StageKind:** `Telemetry`
**Unlock requirement:** Level 1 complete

## Mechanism

A companion satellite-simulator badge (see [Deployment](../deployment/))
broadcasts a fixed loop of frames on LoRa, on a continuous cycle, forever.
Some frames in the loop are plaintext, the rest are encrypted; the badge
filters incoming frames by whether their payload decodes to printable
ASCII, silently forwarding only the plaintext subset to this screen (the
encrypted remainder is what Level 3 works with instead — the two levels
consume disjoint halves of the same broadcast loop).

The player's job is protocol/frame-format reverse engineering: recovering
the meaning of specific fields from the raw frames observed on this
screen or over the serial console. The badge does not hold the expected
answers as bare string-literal constants that a firmware dump could
recover directly — see [Challenge Framework](challenge-framework.md) for
why that matters generally.

An audible chirp confirms each received telemetry packet independent of
whether the player is watching the display or the serial console at that
moment.

## Player workflow

1. Observe the incoming LoRa frame stream (display and/or serial console).
2. Reverse-engineer the frame/protocol fields from the plaintext frames.
3. Submit the recovered flag via the Submit Flag utility stage.

## Persistence and validation

Unlocks only after Level 1's completion is recorded in NVS. Flag
verification is the same SHA-256 digest comparison used by every level.

## Known limitations

End-to-end badge-side telemetry receive against a physical satellite
simulator is deployment-time hardware validation, tracked in
[`docs/release/RELEASE-1.0-CERTIFICATION.md`](../release/RELEASE-1.0-CERTIFICATION.md).

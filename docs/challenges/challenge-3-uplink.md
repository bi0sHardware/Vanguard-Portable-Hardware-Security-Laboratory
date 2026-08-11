# Level 3 — Establishing the Uplink

**Category:** RF / Protocol Implementation
**Module:** `firmware/main/challenges/mission3.cpp`
**StageKind:** `Uplink`
**Unlock requirement:** Level 2 complete

## Mechanism

This level consumes the other half of the same ten-frame satellite
broadcast loop Level 2 reads from (the encrypted frames Level 2's
printable-ASCII filter drops). The badge itself deliberately does not
know what a correct uplink answer looks like — there is no expected-value
constant anywhere in the firmware for this screen to compare against.
Instead, the correct value is recovered by the player from something
physically printed on the badge hardware itself, and the badge's only job
on this screen is to transmit exactly what the player enters, unmodified,
as an authenticated LoRa uplink to the satellite simulator.

The satellite simulator (not the badge) holds the actual expected value
and performs the real check. Because the LoRa link is half-duplex and the
simulator is itself cycling through its own broadcast loop, an uplink
transmitted at the wrong moment in that cycle may simply not be heard —
the badge retransmits automatically on a timeout rather than requiring
the player to manually retry.

## Player workflow

1. Recover the ciphertext half of the broadcast loop (continuing from
   Level 2's reverse-engineering work).
2. Locate and interpret the physical inscription on the badge hardware.
3. Transmit the recovered/translated value as an authenticated uplink.
4. The badge automatically retries on timeout if no acknowledgement is heard.

## Persistence and validation

Unlocks only after Level 2's completion is recorded in NVS. The
authoritative check happens on the satellite simulator, which returns an
acknowledgement the badge waits for (bounded by a timeout, with automatic
retransmission). Flag submission uses the same SHA-256 digest comparison
as every other level.

## Known limitations

The full uplink round-trip against a physical satellite simulator,
including every rejection path, is deployment-time hardware validation,
tracked in [`docs/release/RELEASE-1.0-CERTIFICATION.md`](../release/RELEASE-1.0-CERTIFICATION.md).

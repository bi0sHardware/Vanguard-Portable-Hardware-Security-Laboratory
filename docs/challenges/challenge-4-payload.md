# Level 4 — Operation Vanguard

**Category:** File Recovery / Reverse Engineering
**Module:** `firmware/main/challenges/mission4.cpp`
**StageKind:** `Payload`
**Unlock requirement:** Level 3 complete

## Mechanism

Once Level 3's authenticated uplink succeeds, the satellite simulator
streams a payload to the badge over LoRa in a sequence of numbered
chunks. The badge tracks which chunk indices it has seen, reassembles
them in order, and — because chunks may arrive out of order or need
retransmission — resumes from wherever it left off rather than starting
over if the player re-enters the screen mid-transfer.

Once every chunk has arrived, the payload is a complete recoverable file.
The badge does not interpret the file's contents itself; recovery happens
on the player's own computer, downloading the reassembled bytes over USB
and identifying/extracting the file from there (magic-byte identification
of a compressed format is part of the exercise).

## Player workflow

1. Wait for the chunked payload transfer to complete (or resume an
   interrupted one).
2. Download the reassembled payload to a computer over USB.
3. Identify the file format and recover the embedded content.
4. Extract the final flag from the recovered file.

## Persistence and validation

Unlocks only after Level 3's completion is recorded in NVS. Transfer
progress (which chunks have been received) is tracked in memory for the
duration of the transfer; completion of the level is what persists across
reboots. Flag submission uses the same SHA-256 digest comparison as every
other level.

## Known limitations

The full `DOWNLOAD` → extraction → image-recovery round trip against a
physical satellite simulator is deployment-time hardware validation,
tracked in [`docs/release/RELEASE-1.0-CERTIFICATION.md`](../release/RELEASE-1.0-CERTIFICATION.md).

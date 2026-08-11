# LoRa Subsystem

**Module:** `firmware/main/rf/lora.*`, `firmware/main/rf/EspHal.*`,
`firmware/main/rf/frame_codec.*`, `firmware/main/rf/radio_link.*`

## Purpose

The physical and link layer underlying the challenge arc's satellite
link, Radio Chat, and Ship Battle.

## User flow

Not directly user-facing — consumed by every LoRa-based feature above.

## Technical design

See [`docs/protocols/lora.md`](../docs/protocols/lora.md) and
[`docs/architecture/lora-hardware.md`](../docs/architecture/lora-hardware.md)
for the frame format, self-test/TX/RX flow, and hardware integration
(including the module's lack of a firmware-accessible RESET line).

## Dependencies

RadioLib, the LoRa module (RA-01SC/LLCC68) over SPI.

## Storage usage

None.

## Known limitations

No firmware-accessible radio RESET — recovery from a stuck radio state
requires a full badge power cycle, not a reflash. See
[`TROUBLESHOOTING.md`](../TROUBLESHOOTING.md).

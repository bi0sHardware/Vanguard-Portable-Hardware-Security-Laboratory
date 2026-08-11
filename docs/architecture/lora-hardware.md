# LoRa Hardware Integration

This document covers the physical integration reality behind the LoRa driver
described in `docs/protocols/lora.md`: pin roles, bus sharing, and what
happens — and what firmware can and cannot do about it — when the radio ends
up in an unresponsive state.

## Module and pins

The badge carries an RA-01SC module on an ESP32-S3-WROOM-1. Per
`firmware/include/pins.h`:

| Signal | GPIO | Notes |
|---|---|---|
| `LORA_NSS`  | 9  | chip select |
| `LORA_MOSI` | 11 | shared with TFT MOSI and the shift-register serial line |
| `LORA_MISO` | 13 | shared with TFT MISO pin (unused by the TFT itself) |
| `LORA_SCK`  | 12 | shared with TFT SCK and the shift-register clock line |
| `LORA_DIO1` | 6  | chip **output** — never driven as an MCU output |
| `LORA_BUSY` | 3  | chip **output** — never driven as an MCU output |
| RESET | — | hardware pull-up only, **no GPIO** (`RADIOLIB_NC` in firmware) |

## No firmware-accessible RESET line

The single most consequential fact about this hardware integration is that
the LoRa module's RESET pin is wired only to a hardware pull-up, with no
GPIO connection at all. RadioLib's `Module` is constructed with
`RADIOLIB_NC` in the reset-pin slot (`firmware/main/rf/lora.cpp`), which
makes RadioLib's own `reset()` call a no-op on this hardware. Concretely,
this means firmware has no way to force the SX1261 back to a known state —
if the chip ends up wedged (its `BUSY` line stuck high, which RadioLib
reports as a "GPIO post-transfer timeout"), that condition persists across
both an ESP32 soft reset and a full reflash, because nothing on the MCU side
can pulse the module's actual reset line.

`rf::init()` (see `firmware/main/rf/lora.cpp`) works around the absence of a
hard reset with the datasheet's documented wake-from-sleep sequence instead:
it drives NSS low then high (a falling edge wakes the chip from sleep) and
polls `BUSY` for it to go low, which recovers the *"chip went to sleep"*
condition — a state that presents identically to a genuine wedge from the
caller's point of view, but is not the same failure and is recoverable this
way. If `BUSY` does not go low within that wait, `init()` logs that the radio
is unresponsive and that switching the badge fully off and back on — a real
power cycle, not a soft reset — is the only way to clear it, since a power
cycle is the only path that actually removes power from the module itself.

## DIO1 and BUSY

`LORA_DIO1` and `LORA_BUSY` are both chip outputs read by the MCU, never
driven the other direction. `BUSY` is the SX126x's readiness signal — the
driver polls it as part of the wake sequence above, and RadioLib's own
transaction handling relies on it to know when the chip is ready for the
next SPI command. `DIO1` is the interrupt line RadioLib/the driver uses to
learn about radio events (such as TX/RX completion) without polling SPI
registers continuously.

## Shared SPI bus with the display and LED shift registers

`LORA_MOSI`/`LORA_SCK` are the same physical lines as the TFT display's
MOSI/SCK and the two 74HC595 shift registers driving the LED chain (see
`pins.h`'s comments on `TFT_MOSI`/`TFT_SCK`/`SR_SER`/`SR_SRCLK`). Every
device on that bus must have its own chip-select deasserted whenever another
device is being addressed, or it will interpret that traffic as its own.

This has a concrete boot-ordering consequence, documented in
`firmware/main/main.cpp`'s `setup()`: `rf::deselect()` (which does nothing
but drive `LORA_NSS` high) must run before `display::init()`, because
RadioLib does not raise the LoRa module's NSS line until `rf::init()` runs —
which happens after display initialization in the normal boot sequence — and
`display::init()` performs a full-screen fill over SPI before that point. If
`rf::deselect()` is skipped or run too late, the display's SPI traffic reads
to the LoRa module as addressed to it with `NSS` floating, and the resulting
confusion has been observed to wedge the module so thoroughly that `BUSY`
never returns low — surfacing later as a `CHIP_NOT_FOUND`-shaped failure that
is actually a bus-contention artifact, not a missing or dead chip.

## Operational implication

Given no software reset path exists, the practical recovery model for a
wedged radio is: `rf::init()`'s wake sequence handles the common "asleep, not
actually wedged" case transparently and automatically on every boot; anything
beyond that requires a user-initiated power cycle. Firmware-side mitigations
accordingly focus on *avoiding* the wedge in the first place — the
`rf::deselect()`-before-`display::init()` ordering above being the clearest
example — rather than attempting to recover from it after the fact, since no
in-firmware recovery is possible once the chip is genuinely stuck.

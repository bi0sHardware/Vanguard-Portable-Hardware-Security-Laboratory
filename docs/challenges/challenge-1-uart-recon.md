# Level 1 — UART Recon

**Category:** Embedded Hardware / UART
**Module:** `firmware/main/challenges/uart_leak.cpp`
**StageKind:** `UartLeak`
**Unlock requirement:** none (always unlocked)

## Mechanism

An undocumented UART peripheral is broadcast continuously and in the
background, independent of whatever screen is currently active on the
badge — it starts in `uartleak::init()` at boot and is serviced every
tick from `challenges::update()`, not scoped to a screen. It transmits a
line at a fixed baud rate on a fixed interval, at one of the standard
UART baud rates, deliberately not the badge's own USB-CDC serial rate, so
a player can't assume it matches the console they already have open.

The transmitted line is the flag, base64-encoded before being sent over
the wire, so the byte stream is printable ASCII once the correct baud
rate is found — locating the interface is the primary hurdle, decoding
the payload is a small secondary step.

## Player workflow

1. Locate the undocumented TX pin with a logic analyzer.
2. Determine the baud rate empirically (a fixed set of standard rates).
3. Capture and decode the transmitted line.

## Persistence and validation

This level has no `requiresId` — it is always available. Its flag is
submitted through the same "Submit Flag" utility stage used by every
other level (see [Challenge Framework](challenge-framework.md)): a
SHA-256 hash comparison, not a plaintext match, and the on-device data
never stores the flag as a plain string constant.

## Known limitations

Hardware-in-the-loop verification (confirming the signal is correctly
observable with a logic analyzer on production PCBs) is deployment-time
validation, tracked in [`docs/release/RELEASE-1.0-CERTIFICATION.md`](../release/RELEASE-1.0-CERTIFICATION.md)
rather than asserted here.

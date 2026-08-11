# Troubleshooting

## Build issues

**Added a `.cpp` file and got "undefined reference" at link time.**
ESP-IDF's CMake source globbing is evaluated at configure time and
forbids `CONFIGURE_DEPENDS`, so new files aren't picked up automatically.
Touch `firmware/main/CMakeLists.txt` (or run `./tools/clean.sh` and
rebuild) to force a reconfigure.

**First build is slow.**
Expected — it downloads the Espressif Docker image and every managed
component. Subsequent builds are incremental.

## Flash issues

**Flashing fails with an MD5 mismatch or "chip stopped responding".**
On Linux, stop ModemManager — it probes serial ports and can trigger
reset loops during flashing:

```bash
sudo systemctl stop ModemManager
```

If it persists: hold BOOT, tap RESET once, release RESET while still
holding BOOT, start the flash, then release BOOT. This badge's native
USB-Serial/JTAG has no dedicated auto-reset circuit, so esptool's
automatic reset-into-bootloader handshake is not always reliable — see
`tools/flash.sh` and `tools/provision_new_badge.sh` for how the
provisioning tooling works around this with retries and a
single-handshake flash sequence.

**A brand-new badge won't flash at all (JEDEC ID reads as all-Fs).**
It needs the one-time VDD_SPI eFuse fix — see
[Badge Provisioning](docs/deployment/badge-provisioning.md).

## USB issues

**`Serial.print()` produces nothing, but ESP-IDF boot logs appear.**
`ARDUINO_USB_MODE` / `ARDUINO_USB_CDC_ON_BOOT` compile definitions are
missing — check the root `firmware/CMakeLists.txt`.

**First serial output after a reset is missing.**
Expected — the native USB-CDC port re-enumerates on reset, losing the
first moment of output. Reconnect the serial monitor after the badge has
finished booting.

## Communication issues

**LoRa reports `BUSY stuck high` or `init: FAILED`.**
Power-cycle the badge with its physical power switch. The LoRa module's
RESET line is not connected to any GPIO, so firmware cannot reset the
radio in software — if it latches into an unresponsive state, a power
cycle is the only way to clear it. A reflash or soft reset will not help.

**BLE PeerDrop or LoRa features seem unavailable at the same time.**
See [`docs/architecture/ble-subsystem.md`](docs/architecture/ble-subsystem.md)
for how LoRa and BLE coexistence is handled on this hardware.

## Recovery procedures

- **Stuck radio:** power cycle (see above) — not a reflash.
- **Badge in an inconsistent state (progress, contacts, settings):**
  re-flash with `tools/flash.sh`, which erases NVS on every flash, or use
  `tools/provision_new_badge.sh` for a full re-provision.
- **Badge won't enter download mode:** manual BOOT+RESET sequence (see
  Flash issues above).

## Common failure modes

| Symptom | Likely cause | Fix |
|---|---|---|
| Flash fails immediately, all-Fs JEDEC ID | Un-provisioned board, wrong flash voltage | Run the eFuse fix via `provision_new_badge.sh` |
| MD5 mismatch during flash | ModemManager interference (Linux) | Stop ModemManager |
| Radio stuck / `BUSY stuck high` | LoRa module has no reset line | Power cycle the badge |
| No serial output after reset | USB-CDC re-enumeration | Reconnect monitor after boot |
| New `.cpp` not linked | CMake glob not re-evaluated | Touch `CMakeLists.txt` or clean-build |

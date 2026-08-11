# Badge Provisioning

## First-time hardware bring-up

Every Vanguard badge PCB requires a one-time eFuse fix before its first
flash. GPIO45 (MTDI), an ESP32-S3 strapping pin read at boot to select the
internal flash regulator's voltage, shares a net with a joystick button's
pull-up to the 3.3V rail. That pull-up holds GPIO45 high on every reset,
which tells the chip to run its flash at 1.8V — but the flash chip on this
board is a 3.3V part, so it browns out and never responds until the fix is
applied. The fix (`espefuse set_flash_voltage 3.3V`) is a permanent,
one-time-per-board eFuse burn; re-running it on an already-fixed board is
a harmless no-op.

`tools/provision_new_badge.sh` automates the full bring-up sequence for
one or many badges at once:

```bash
./tools/provision_new_badge.sh --watch      # recommended for a batch: auto-provisions
                                             # every badge as it's plugged in
./tools/provision_new_badge.sh              # one-shot: provision whatever's plugged in now
./tools/provision_new_badge.sh --skip-efuse # skip the eFuse step (already-fixed boards)
```

For each badge, it: burns the eFuse fix (unless skipped), writes the
current firmware build, erases the NVS partition (identity, contacts,
challenge progress, settings), and resets into the new firmware. Because
this badge's native USB-Serial/JTAG has no dedicated auto-reset circuit,
the script chains raw `esptool` calls with `--before/--after no_reset`
rather than `idf.py flash`, so the board only needs to enter download mode
once per boot cycle, with automatic retries on a failed handshake.

## Re-flashing a provisioned badge

```bash
./tools/build.sh
./tools/flash.sh /dev/ttyACM0
```

`flash.sh` also erases the NVS partition on every flash, so every badge
that comes off it is in a genuinely fresh-out-of-the-box state — not
still carrying whoever last tested it.

## Fresh-badge NVS state

A freshly provisioned or freshly flashed badge has no stored identity, no
contacts, and no challenge progress. The badge generates its own
deterministic identity (badge ID, AP SSID) from its MAC address on first
boot; no manual per-badge configuration step is required.

# Getting Started

## Build

```bash
git clone https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory.git
cd Vanguard-Portable-Hardware-Security-Laboratory
./tools/build.sh
```

Docker is the only prerequisite — no local ESP-IDF install, toolchain, or
Python environment is needed to build. The first build downloads the
Espressif Docker image and every managed component; subsequent builds are
incremental.

## Flash

```bash
./tools/flash.sh /dev/ttyACM0
```

Also erases the NVS partition on every flash, so the badge comes out in a
fresh-out-of-the-box state. Brand-new, never-flashed hardware needs a
one-time eFuse fix first — see [`tools/provision_new_badge.sh`](../tools/provision_new_badge.sh)
and [`docs/deployment/badge-provisioning.md`](../docs/deployment/badge-provisioning.md).

## Monitor

```bash
./tools/monitor.sh /dev/ttyACM0
```

Opens a live serial monitor. Note: the native USB-CDC port re-enumerates
on every reset, so the first moment of output after a reset is lost —
reconnect after the badge finishes booting.

## Recover

- **Flashing fails (MD5 mismatch / chip stopped responding):** on Linux,
  stop ModemManager (`sudo systemctl stop ModemManager`); if it persists,
  hold BOOT, tap RESET, release RESET while still holding BOOT, then
  start the flash and release BOOT.
- **LoRa radio stuck (`BUSY stuck high`):** power-cycle the badge — the
  LoRa module has no firmware-accessible reset line, so a reflash will
  not help.
- **Full reset:** `./tools/flash.sh` or `./tools/provision_new_badge.sh`
  both erase NVS, giving a genuinely clean badge.

See [`TROUBLESHOOTING.md`](../TROUBLESHOOTING.md) for the full list.

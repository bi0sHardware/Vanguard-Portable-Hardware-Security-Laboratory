# Vanguard — Portable Hardware Security Laboratory

Vanguard is a handheld hardware security platform: a self-contained,
battery-powered device built around an ESP32-S3 that doubles as a badge, a
field radio, a games console, and a live embedded-security CTF target. It
is designed for education, hardware-hacking research, wireless
experimentation, and competition deployment — something a participant can
be handed, play with, and hack, all on the same piece of hardware.

Firmware is one subsystem of the broader Vanguard platform: this
repository is the canonical engineering source for that firmware, its
companion tooling, and the documentation needed to build, flash, deploy,
and extend it without depending on any other repository.

## Core capabilities

- **Interactive hardware security challenges** — a four-level, on-device
  CTF spanning UART reconnaissance, RF/protocol reverse engineering,
  authenticated uplink scripting, and file-format forensics.
- **LoRa communications** — a shared physical/link layer used by Radio
  Chat, Ship Battle, and the challenge arc's satellite link.
- **BLE PeerDrop** — badge-to-badge contact exchange over Bluetooth Low
  Energy.
- **Radio Chat** — a handheld LoRa messenger with a real Morse/CW mode.
- **Morse Mode** — manual CW send/receive layered on the Radio Chat link.
- **Ship Battle** — a badge-to-badge game built on the same LoRa link
  layer as Radio Chat.
- **Logic-analyzer-based exercises** — Level 1 of the challenge arc is a
  hands-on UART sniffing exercise against real hardware.
- **Portable, field-deployable design** — battery-powered, no
  infrastructure dependency beyond the badges themselves.
- **Competition-grade challenge infrastructure** — sequential unlock
  chain, persistent progress, and a companion satellite simulator so both
  sides of the RF protocol ship from one codebase.

## Hardware overview

Vanguard runs on an ESP32-S3-WROOM-1 with a color TFT display, an
addressable LED chain, a joystick + button input cluster, a LoRa radio
module, and buzzer audio. See [`docs/architecture/`](docs/architecture/)
for the full subsystem breakdown and [`firmware/include/pins.h`](firmware/include/pins.h)
for the pin map. The `hardware/` directory tracks schematics, PCB,
enclosure, and manufacturing files as they become available.

## Firmware overview

The firmware is built on **ESP-IDF, with Arduino integrated as an ESP-IDF
component**, and builds entirely inside the official Espressif Docker
image — Docker is the only build prerequisite. `main.cpp` drives an
`AppState` state machine: exactly one screen is active at a time, each
screen implements `enter()`/`frame()`, and subsystem managers (LEDs,
audio, animation, power, challenges) tick independently of whichever
screen is active. See [`ARCHITECTURE.md`](ARCHITECTURE.md) for the full
picture.

## Feature summary

| Area | What it does |
|---|---|
| Boot & Screensaver | Animated boot sequence into an always-reachable idle home screen |
| Menu system | Central navigation hub for every feature below |
| Profile Setup / Settings | Per-badge identity, preferences, unlock state |
| Contacts | Stores identities exchanged via PeerDrop |
| Music Player | Chiptune playback with an LED pitch visualizer |
| Games | Tetris, Snake, Space Shooter, 2048, Ship Battle |
| Radio Chat / Morse Mode | LoRa handheld messenger with CW send/receive |
| PeerDrop | BLE badge-to-badge contact exchange |
| Challenge Framework | The four-level on-device CTF |
| Storage | NVS-backed persistence for identity, contacts, and progress |

## Architecture summary

See [`ARCHITECTURE.md`](ARCHITECTURE.md) and [`docs/architecture/`](docs/architecture/)
for full detail. In brief: a single `AppState` machine in `main.cpp`
dispatches to one active screen at a time; always-on subsystem managers
handle LEDs, audio, animation, and power independently of the active
screen; and screens never touch hardware drivers directly, only the
shared `ui::` renderer/widgets and subsystem manager APIs.

## Build instructions

```bash
git clone https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory.git
cd Vanguard-Portable-Hardware-Security-Laboratory

./tools/build.sh
```

The first build downloads the Espressif Docker image and all managed
components and takes several minutes; subsequent builds are incremental.
No local ESP-IDF install, toolchain, or Python environment is required to
build.

## Flash instructions

```bash
./tools/flash.sh /dev/ttyACM0        # flash a badge
./tools/monitor.sh /dev/ttyACM0      # open a serial monitor
./tools/clean.sh                     # full clean
```

For brand-new, never-flashed hardware (which needs a one-time eFuse fix —
see [`docs/deployment/`](docs/deployment/)):

```bash
./tools/provision_new_badge.sh --watch
```

On Linux, if flashing fails with MD5 mismatches or "chip stopped
responding", stop ModemManager first — it probes the serial port and can
trigger reset loops:

```bash
sudo systemctl stop ModemManager
```

## Repository structure

```
vanguard/
├── firmware/           ESP-IDF firmware source, vendored display libraries, build config
├── hardware/           Schematics, PCB, enclosure, and manufacturing files
├── docs/               Architecture, protocol, challenge, deployment, and release documentation
├── wiki/                GitHub Wiki source (per-feature pages)
├── tools/               Build/flash/monitor/provisioning scripts and PC-side companion tooling
├── README.md            This file
├── DEVELOPMENT.md        Development workflow and coding standards
├── ARCHITECTURE.md       System architecture
├── DEPLOYMENT.md         Badge preparation and event deployment
├── TROUBLESHOOTING.md    Build, flash, and hardware troubleshooting
├── CHANGELOG.md          Release history
├── CONTRIBUTING.md       Contribution guidelines
└── LICENSE               MIT
```

## Supported hardware

ESP32-S3-WROOM-1-based Vanguard badge PCBs, 16MB flash. See
[`docs/architecture/`](docs/architecture/) and
[`firmware/include/pins.h`](firmware/include/pins.h) for the full pinout
and hardware constraints (backlight wiring, LoRa RESET availability, SPI
bus sharing, VDD_SPI eFuse requirement).

## Quick start

1. Install Docker.
2. `git clone` this repository.
3. `./tools/build.sh`
4. `./tools/flash.sh /dev/ttyACM0`
5. `./tools/monitor.sh /dev/ttyACM0` to watch it boot.

For event/competition deployment, see [`DEPLOYMENT.md`](DEPLOYMENT.md).
For challenge design and progression, see [`docs/challenges/`](docs/challenges/)
(participant-facing, spoiler-free).

## Documentation

- [`ARCHITECTURE.md`](ARCHITECTURE.md), [`docs/architecture/`](docs/architecture/) — system design
- [`docs/protocols/`](docs/protocols/) — LoRa, Radio Chat, Morse, and PeerDrop wire protocols
- [`docs/challenges/`](docs/challenges/) — challenge framework and per-level design (no flags/solutions)
- [`DEPLOYMENT.md`](DEPLOYMENT.md), [`docs/deployment/`](docs/deployment/) — badge prep and event checklists
- [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md) — build, flash, and hardware failure modes
- [`DEVELOPMENT.md`](DEVELOPMENT.md) — workflow, standards, and how to extend the platform
- [`wiki/`](wiki/) — per-feature reference pages
- [`docs/release/RELEASE-1.0-CERTIFICATION.md`](docs/release/RELEASE-1.0-CERTIFICATION.md) — release readiness status

## License

MIT — see [`LICENSE`](LICENSE).

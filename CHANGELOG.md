# Changelog

## v1.0.0 — Initial Production Release

First public release of the Vanguard firmware platform.

### Platform

- Built on ESP-IDF, with Arduino integrated as an ESP-IDF component, built
  and flashed entirely inside the official Espressif Docker image.
- `AppState`-driven screen architecture with always-on subsystem managers
  for LEDs, audio, animation, and power (see [`ARCHITECTURE.md`](ARCHITECTURE.md)).

### Challenge framework

- Four-level, registry-driven, sequentially-unlocked on-device CTF with
  NVS-backed progress persistence.
- Companion satellite-simulator build variant sharing the same source
  tree as the player firmware, so both sides of the RF protocol cannot
  drift apart.

### Communications

- **LoRa** physical/link layer shared by Radio Chat, Ship Battle, and the
  challenge arc's satellite link.
- **Radio Chat** — handheld LoRa messenger with peer discovery and
  reliable delivery.
- **Morse Mode** — manual CW send/receive on the Radio Chat link.
- **PeerDrop** — BLE (NimBLE) badge-to-badge contact exchange.
- **Ship Battle** — badge-to-badge game on the shared LoRa link layer.

### Badge features

- Boot animation and an always-reachable Screensaver home screen.
- Menu system, Settings, Profile Setup, Contacts.
- Music Player with an LED pitch visualizer.
- Games: Tetris, Snake, Space Shooter, 2048, Ship Battle.
- Mission Completion screen, triggered once all four challenge levels are
  complete.

### Tooling

- Docker-based build/flash/monitor/clean scripts requiring no local
  ESP-IDF install.
- Batch badge provisioning (`tools/provision_new_badge.sh`), including the
  one-time VDD_SPI eFuse fix new hardware requires.

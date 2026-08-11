# Vanguard — Portable Hardware Security Laboratory

Vanguard is a handheld ESP32-S3 hardware security platform: part badge,
part field radio, part games console, part live embedded-security CTF
target.

## System overview

A single `AppState` state machine (`firmware/main/main.cpp`) dispatches to
one active screen at a time, while always-on subsystem managers (LEDs,
audio, animation, power, challenges) tick independently in the
background. See [Architecture](Architecture.md).

## Navigation

- [Getting Started](Getting-Started.md) — build, flash, monitor, recover
- [Architecture](Architecture.md) — system design, state flow, subsystem relationships
- Feature pages: [Boot System](Boot-System.md), [Screensaver](Screensaver.md),
  [Profile Setup](Profile-Setup.md), [Settings](Settings.md),
  [Contacts](Contacts.md), [Music Player](Music-Player.md),
  [Cassette Animation](Cassette-Animation.md), [Radio Chat](Radio-Chat.md),
  [Morse Mode](Morse-Mode.md), [PeerDrop](PeerDrop.md),
  [Ship Battle](Ship-Battle.md), [Challenge Framework](Challenge-Framework.md),
  [Storage System](Storage-System.md), [Audio System](Audio-System.md),
  [LED System](LED-System.md), [Battery Monitoring](Battery-Monitoring.md),
  [LoRa Subsystem](LoRa-Subsystem.md), [BLE Subsystem](BLE-Subsystem.md),
  [Mission Completion System](Mission-Completion-System.md),
  [Notification System](Notification-System.md)

For build-quality documentation (architecture deep-dives, protocol specs,
challenge design, deployment), see [`docs/`](../docs/) in the repository
root.

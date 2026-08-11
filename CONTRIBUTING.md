# Contributing

## Coding standards

See [`DEVELOPMENT.md`](DEVELOPMENT.md#coding-standards). In short:
comments explain WHY, not WHAT; no unannounced behavioral changes; follow
the existing `AppState`/subsystem-manager conventions described in
[`ARCHITECTURE.md`](ARCHITECTURE.md).

## Formatting standards

Match the existing style in the file you're editing (brace placement,
naming, indentation). This codebase does not currently enforce an
automated formatter — consistency with surrounding code is the standard.

## Naming conventions

- Namespaces are lowercase (`led::`, `audio::`, `challenges::`).
- Constants are `kCamelCase` or `UPPER_SNAKE_CASE` depending on scope,
  matching the surrounding file.
- File-static state is prefixed `s_` (e.g. `s_phase`, `s_selected`).

## Commit message standards

- Describe the change's purpose, not just its mechanics.
- Keep firmware-behavior commits separate from pure documentation/cleanup
  commits where practical.
- Reference the affected subsystem in the summary line where helpful
  (e.g. "Radio Chat: ...", "PeerDrop: ...").

## Pull request process

1. Open a PR against `main` describing what changed and why.
2. Note any hardware-dependent behavior and whether it has been verified
   on physical hardware.
3. Confirm `./tools/build.sh` succeeds.

## Review process

Changes to shared infrastructure (the `AppState` machine, subsystem
managers, the challenge engine, the LoRa/BLE link layers) warrant closer
review than an isolated screen or content change, since they affect every
feature built on top of them.

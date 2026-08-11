# Development

## Development workflow

The firmware builds and flashes entirely inside the official Espressif
Docker image via the scripts in [`tools/`](tools/) — no local ESP-IDF
install is required for day-to-day work.

```bash
./tools/build.sh
./tools/flash.sh /dev/ttyACMx
./tools/monitor.sh /dev/ttyACMx
./tools/clean.sh
```

## Adding features

Follow the existing `AppState` convention (see [`ARCHITECTURE.md`](ARCHITECTURE.md)):
a new screen implements `enter()`/`frame()`, is added to
`firmware/include/state.h`'s `AppState` enum, and is dispatched from
`main.cpp`. Screens should go through the shared `ui::` renderer/widgets
and subsystem managers (`led::`, `audio::`, `power::`) rather than
touching hardware drivers directly.

## Adding components

Third-party components are vendored under `firmware/components/` (the
Adafruit display stack) rather than pulled as ESP-IDF managed components,
so the release tree is self-contained. First-party modules live under
`firmware/main/<module>/` with their own `.h`/`.cpp` pair and are wired
into `firmware/main/CMakeLists.txt`'s source list.

## Adding a challenge level

Edit `firmware/main/challenges/challenge_data.h` only, for content that
reuses an existing `StageKind`: add a registry entry with a unique `id`,
a `requiresId` for the unlock chain, and a `flagSha256` digest. A
genuinely new on-device *behavior* needs a new `StageKind`, a module, and
a dispatch case in `challenges.cpp`. See
[`docs/architecture/challenge-engine.md`](docs/architecture/challenge-engine.md).

## Debugging workflow

- `./tools/monitor.sh /dev/ttyACMx` for live serial output.
- Debug-only scoped timing is available via `firmware/main/perf/perf.h`,
  compiled in only when `PERF_DEBUG` is defined in the component's build
  flags — it measures actual frame/redraw/SPI timing rather than
  assuming a change improved it.
- See [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md) for build/flash/hardware
  failure modes.

## Common pitfalls

- **CMake source globbing is configure-time**, and ESP-IDF forbids
  `CONFIGURE_DEPENDS` — a newly added `.cpp` needs a touched
  `CMakeLists.txt` or a clean build to be picked up.
- **`loop()` must never block** — no `delay()` in the main path; ESP-IDF's
  task watchdog monitors idle-task starvation. Use `millis()`-gated
  timing instead.
- **The LoRa module has no firmware-accessible RESET line** — code that
  assumes it can reset the radio in software is wrong; see
  [`docs/architecture/lora-hardware.md`](docs/architecture/lora-hardware.md).
- **Screens should never touch hardware drivers directly** — go through
  the subsystem managers and `ui::` renderer.

## ESP-IDF considerations

Built with Arduino integrated as an ESP-IDF component, not standalone
Arduino/PlatformIO. `firmware/CMakeLists.txt` documents the
integration/build-flag glue this requires.

## Coding standards

- Comments explain WHY (hardware constraints, protocol rules, security
  rationale), not WHAT — the code itself should read clearly enough that
  restating its behavior in a comment is unnecessary.
- No functional/behavioral changes should ship "quietly" alongside a
  documentation or cleanup pass — if a change affects behavior, say so
  explicitly in the commit message.
- Prefer static allocation over heap allocation after boot, consistent
  with the rest of the firmware.

## Pull request guidelines

- Keep firmware behavior changes and documentation/cleanup changes in
  separate commits where practical.
- Describe hardware-dependent changes explicitly — note what has and has
  not been verified on physical hardware (see
  [`docs/release/RELEASE-1.0-CERTIFICATION.md`](docs/release/RELEASE-1.0-CERTIFICATION.md)
  for the verification-status convention used in this repository).
- See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the full contribution
  process.

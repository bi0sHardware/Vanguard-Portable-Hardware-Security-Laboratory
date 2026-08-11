# Challenge Framework

Vanguard's on-device CTF is a sequential, four-level arc implemented as a
data-driven registry, not four hand-written screens with duplicated
plumbing.

## Registry-driven design

Every stage — the four graded levels plus the utility "Submit Flag" desk —
is one entry in a single array (`firmware/main/challenges/challenge_data.h`).
Each entry carries:

- a stable string `id` (used both as the NVS completion key and as the
  unlock dependency target for later stages)
- display metadata (name, category, description, hint, difficulty)
- a `StageKind` enum value selecting which on-device behavior renders the
  stage (a static info panel, the UART leak, the LoRa telemetry screen,
  the authenticated uplink screen, or the payload receiver)
- a `requiresId` unlock dependency (`nullptr` means always unlocked)
- a SHA-256 digest of the correct flag string — never the flag itself

Adding a new stage that reuses an existing `StageKind` is a pure data
change: one new entry in the registry, no new code. A stage that needs
genuinely new on-device *behavior* gets a new `StageKind` value, a
dispatch case in `challenges.cpp`, and its own module.

## Unlock chain and persistence

`challenges::isUnlocked()` walks the `requiresId` chain: a stage is
selectable once its prerequisite (if any) has been completed. Completion
state lives in NVS, keyed by each stage's `id`, so progress survives
reboots. `challenges::completeChallenge()` is idempotent — completing an
already-completed stage is a silent no-op — and `resetAll()` clears every
stage's state for re-provisioning a badge between events.

## Flag verification

The "Submit Flag" utility stage (`flagdesk.cpp`) accepts a candidate flag
over the serial console, hashes it with SHA-256, and compares the digest
against the stage's stored hash. The firmware never holds a flag in
plaintext at rest, so a firmware or flash dump does not hand over a
solvable stage's answer directly — recovering the flag still requires
completing whatever the stage's actual mechanism is (see the per-level
pages in this directory). A lightweight, per-level XOR-obfuscated
plaintext is held briefly in RAM only at the moment a stage reveals its
own flag on success (see `flag_reveal.h`), which is a UX confirmation
step, not the verification path itself.

## Background vs. screen-driven stages

Some stages run detection logic continuously in the background regardless
of which screen is on-display (`challenges::update()` is ticked every
`loop()` iteration independent of the active `AppState`); others are only
active while their own screen is showing. See each level's page for which
model it uses.

## Mission-complete event

`challenges::consumeMissionCompleteEvent()` fires exactly once, the tick
all four graded levels become complete, and `main.cpp` polls it to force a
transition into a dedicated Mission Complete screen.

## Per-level documentation

- [Level 1 — UART Recon](challenge-1-uart-recon.md)
- [Level 2 — Satellite Recon](challenge-2-satellite-recon.md)
- [Level 3 — Establishing the Uplink](challenge-3-uplink.md)
- [Level 4 — Operation Vanguard](challenge-4-payload.md)

These pages describe mechanism, progression, and validation — not
solutions. Flags and walkthroughs are organizer-only material and are
intentionally not part of this repository.

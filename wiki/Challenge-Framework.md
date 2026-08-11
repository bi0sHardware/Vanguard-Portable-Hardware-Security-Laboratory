# Challenge Framework

**Module:** `firmware/main/challenges/`

## Purpose

The on-device, four-level embedded-security CTF built into the badge.

## User flow

See [`docs/challenges/challenge-framework.md`](../docs/challenges/challenge-framework.md)
and the per-level pages linked from it. This wiki page intentionally does
not duplicate that content — challenge documentation is spoiler-free and
lives under `docs/challenges/` as the canonical source.

## Technical design

Registry-driven: one array entry per stage
(`firmware/main/challenges/challenge_data.h`), dispatched generically by
`StageKind`, unlocked sequentially via a `requiresId` chain. See
[`docs/architecture/challenge-engine.md`](../docs/architecture/challenge-engine.md).

## Dependencies

`storage::` (NVS-backed progress), `console::` (serial UI), `led::`/
`audio::` for completion feedback.

## Storage usage

Completion state per stage, keyed by stage `id`, in the `challenges` NVS
namespace.

## Known limitations

See [`docs/release/RELEASE-1.0-CERTIFICATION.md`](../docs/release/RELEASE-1.0-CERTIFICATION.md)
for what has and has not been hardware-verified.

## Future extension points

Adding a Level 5 that reuses an existing `StageKind` is a pure data
change in `challenge_data.h`; a genuinely new mechanism needs a new
`StageKind` and module.

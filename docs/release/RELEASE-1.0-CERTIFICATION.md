# Release 1.0 Certification

## Status legend

Every claim in this document is labeled with exactly one of:

- **Verified (code review)** — confirmed by reading the implementation.
- **Verified (hardware test)** — confirmed by running the described
  scenario on physical Vanguard hardware.
- **Reasoned analysis** — a conclusion inferred from the code and design,
  not from a direct test.
- **Pending** — not yet checked by any of the above; no claim is made
  either way.

No section below asserts a hardware-tested result that has not actually
been run. Sections awaiting real test data are marked **Pending** rather
than assumed passing.

## Firmware audit summary

- Repository structure, source migration, and documentation
  cross-references: **Verified (code review)**.
- No AI-development artifacts (tool references, session-narrative
  comments) remain in source, docs, or git history: **Verified (code
  review)** — confirmed via full-repository grep sweep; see the cleanup
  report accompanying this release.
- No spoiler content (flags, solutions, walkthroughs) present in
  participant-facing documentation: **Verified (code review)**.
- Clean-clone build: **Pending** — requires a Docker/ESP-IDF build run
  against this exact tree.

## Security review summary

- Challenge flags are stored as SHA-256 digests, never as plaintext
  constants, in the firmware source: **Verified (code review)**.
- Each challenge level uses a distinct multi-byte obfuscation key for its
  on-device flag-reveal string, so recovering one level's key does not
  expose another level's flag: **Verified (code review)**.
- Resistance to a full firmware/flash dump plus unlimited offline
  analysis: **Reasoned analysis** — no purely on-device, statically
  embedded secret can be made unrecoverable against that threat model;
  the design goal is raising the cost of casual extraction, not
  providing a cryptographic guarantee.

## Challenge validation summary

- Registry-driven unlock chain and NVS persistence logic: **Verified
  (code review)**.
- Level 1 (UART Recon) observable with a logic analyzer on production
  hardware: **Pending**.
- Level 2 (Satellite Recon) badge-side telemetry receive against a
  physical satellite simulator: **Pending**.
- Level 3 (Uplink) full authenticated round-trip, including rejection
  paths, against a physical satellite simulator: **Pending**.
- Level 4 (Payload) chunked transfer, download, and extraction round-trip
  on physical hardware: **Pending**.

## Communication validation summary

- LoRa frame codec and link-layer implementation: **Verified (code
  review)**.
- LoRa bidirectional communication between two physical badges: **Pending**.
- BLE (NimBLE) PeerDrop exchange on physical hardware, both directions:
  **Pending**.
- Radio Chat / Ship Battle discovery and reliable delivery on physical
  hardware: **Pending**.

## Hardware validation summary

- Pin map and documented hardware constraints (backlight wiring, LoRa
  reset-line absence, SPI bus sharing, VDD_SPI eFuse requirement):
  **Verified (code review)** — as described in source comments and
  `firmware/include/pins.h`; not independently re-derived from schematics
  in this repository.
- Physical badge boot, display, input, and battery-monitoring behavior on
  production PCBs: **Pending**.

## Deployment readiness summary

- Build/flash/provisioning tooling is present, documented, and reviewed
  for correctness: **Verified (code review)**.
- End-to-end badge provisioning on a batch of physical boards: **Pending**.
- Full competition deployment checklist walkthrough on physical hardware:
  **Pending** — see [`docs/deployment/event-checklist.md`](../deployment/event-checklist.md).

## Known limitations

- No hardware-in-the-loop testing has been performed as part of this
  documentation and repository-cleanup pass. Every "Pending" item above
  requires a physical-hardware test pass before this release can be
  considered fully certified.
- This document reflects the state of the source tree at the time it was
  written and should be re-run/updated whenever firmware behavior
  changes.

## Final recommendation

**Conditional.** The repository, documentation, and source-level security
properties described above are verified by code review. Hardware-in-the-
loop validation across the challenge arc, communications subsystems, and
provisioning tooling is required before this release can be certified
ready for competition deployment. Update the "Pending" items above with
real test results, then re-issue this document's final recommendation.

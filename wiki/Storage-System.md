# Storage System

**Module:** `firmware/main/storage/storage.*`

## Purpose

Thin wrapper over ESP-IDF NVS (Preferences) for everything the badge
persists across reboots.

## User flow

Not user-facing directly — every feature that needs persistence (Profile
Setup, Contacts, Settings, Challenge progress) goes through this layer.

## Technical design

Data is organized into separate NVS namespaces per concern
(`my_id`, `contacts`, `settings`, `challenges`), keeping unrelated
features from colliding on keys. See
[`docs/architecture/storage-nvs.md`](../docs/architecture/storage-nvs.md).

## Dependencies

ESP-IDF NVS.

## Storage usage

Namespaces: `my_id` (player identity), `contacts` (up to
`NVS_MAX_CONTACTS` entries), `settings`, `challenges` (per-stage
completion flags keyed by stage ID).

## Known limitations

See [`docs/architecture/storage-nvs.md`](../docs/architecture/storage-nvs.md)
for any documented concurrency/open-mode considerations.

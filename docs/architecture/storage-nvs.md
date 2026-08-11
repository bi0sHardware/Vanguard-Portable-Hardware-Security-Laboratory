# Storage / NVS

Vanguard persists all of its non-volatile state through the ESP-IDF NVS
(Non-Volatile Storage) partition, accessed via the Arduino `Preferences`
API. Most of it is centralized behind a small wrapper,
`firmware/main/storage/storage.h`/`.cpp`; Challenge progress uses its own
NVS namespace but the same underlying mechanism (see
`docs/architecture/challenge-engine.md`).

## NVS namespaces

`firmware/include/config.h` defines the fixed namespace names the firmware
uses:

| Namespace | Constant | Contents |
|---|---|---|
| `my_id` | `NVS_NS_MY_ID` | this badge's own identity: name, email, phone, organization |
| `settings` | `NVS_NS_SETTINGS` | display brightness, sound-enabled flag, and Badge Attack state (`hasAttackedSomeone`, `isSecureBadgeEnabled`) |
| `contacts` | `NVS_NS_CONTACTS` | the PeerDrop contact list (see below) |
| `challenges` | `NVS_NS_CHALLENGES` | per-stage completion flags, keyed by each `ChallengeStage`'s `id` |

## What's persisted

- **Identity** (`storage::Identity`: name/email/phone/org) — the profile a
  user fills in via Settings/WiFi Setup. This is what PeerDrop exchanges
  with other badges and what `radiolink` peers can be matched against by
  MAC (see `docs/protocols/peerdrop.md`, `docs/protocols/radio-chat.md`).
- **Settings** — display brightness (stored value only; see
  `docs/architecture/display-rendering.md` for why this does not control
  actual backlight hardware) and a sound-enabled toggle, plus two Badge
  Attack-related flags: whether this badge has ever landed a confirmed
  (acknowledged) attack on another badge, and whether this badge's own
  "Secure Badge" defense is currently enabled.
- **Contacts** — a list of PeerDrop exchange results, each a BLE MAC address
  plus the peer's identity fields, capped at `cfg::NVS_MAX_CONTACTS` (32)
  entries.
- **Challenge progress** — per-stage completion state, persisted so progress
  survives a reboot; see `docs/architecture/challenge-engine.md` for the
  registry this keys against.

## Read-write opens, not read-only

Every load-style function in `storage.cpp` — including ones that never write
anything, like `loadMyIdentity()` or `contactCount()` — opens its
`Preferences` namespace read-write (`prefs.begin(ns, /*readOnly=*/false)`),
not read-only. The source comment explains why: opening a namespace read-only
fails with `ESP_ERR_NVS_NOT_FOUND` (logged as an alarming
`nvs_open failed: NOT_FOUND` line) whenever that namespace has never been
written to yet — which is exactly the state of a freshly-flashed or just-
erased badge before its first Settings change, saved contact, or completed
challenge stage. A read-write open auto-creates the namespace instead, so a
plain read works cleanly on first boot with no functional difference from a
true read-only open once the namespace exists.

## Contact list layout and concurrency

The contact list is stored as a flat, manually-indexed structure rather than
a single serialized blob: a `count` key, plus `mac<i>`/`data<i>` key pairs
for each entry (`macKey()`/`dataKey()` in `storage.cpp`). Each contact's
identity fields are packed into one string value, joined by the `0x1F`
(unit-separator) control character — chosen specifically because it cannot
appear in normal user-entered text, unlike a comma (which PeerDrop's own
identity-characteristic payload uses, and which a name or organization field
could plausibly contain). `unpackIdentity()` is defensive about a blob
missing one or more delimiters (a corrupted or legacy entry, or a read of an
index whose keys were never written, which returns empty strings from
`Preferences`): it bails out to an all-empty `Identity` the moment an
expected delimiter isn't found, rather than relying on `String::substring()`
being handed a `-1` bound.

`deleteContact()` removes an entry by shifting every subsequent entry's
`mac<i>`/`data<i>` pair down by one index, then removing the last (now
duplicate) pair and decrementing `count` — a re-pack-and-resave approach
rather than leaving a gap. There is no locking or transactional guarantee
around this multi-key update sequence; the firmware's single-threaded,
cooperative main loop is what keeps two contact-list mutations from
interleaving; nothing in `storage.cpp` itself is designed to be concurrency-
safe against, for example, an interrupt-driven write.

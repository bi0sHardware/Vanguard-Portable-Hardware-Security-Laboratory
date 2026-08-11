# Settings

**Module:** `firmware/main/settings/settings.*`

## Purpose

Central place for badge preferences and diagnostics: sound toggle,
clearing the contacts database, launching the WiFi Setup portal, and a
hardware test suite.

## User flow

Reached from the Main Menu. A list screen with the options above; each
selection either toggles a value immediately or transitions into its own
sub-flow (e.g. WiFi Setup — see [Profile Setup](Profile-Setup.md)).

## Technical design

`settings::enter(bool startAtWifiSetup)` can skip straight to the WiFi
Setup sub-flow, used by the Screensaver's Profile shortcut, so that path
returns to the Screensaver on Back instead of to the Settings list.
`stopWifiPortalIfActive()` is idempotent and is also called from
`main.cpp`'s forced-transition paths, so the softAP never leaks running
if a forced state change bypasses this screen's own cleanup.

## Dependencies

`storage::`, `badge_id::`, `audio::` (sound toggle).

## Storage usage

Reads/writes the `settings` NVS namespace; the "Clear Contacts" action
writes to the `contacts` namespace.

## Known limitations

The "Secure Badge" defense toggle referenced by the Badge Attack
minigame is only unlockable after landing an attack on another badge at
least once — see [Ship Battle](Ship-Battle.md) / Radio Chat for that
mechanic.

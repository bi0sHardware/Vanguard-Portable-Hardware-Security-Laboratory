# Profile Setup

**Module:** `firmware/main/settings/settings.*` (WiFi Setup portal path)

## Purpose

Lets a player set their identity (name, organization, contact info) used
elsewhere in the badge (Contacts, PeerDrop) via a WiFi captive-portal
form rather than on-device text entry.

## User flow

Entered directly from the Screensaver's Ok shortcut, or via Settings.
The badge starts a WPA2 access point (`ID:[BADGE_ID]`) and a local HTTP
server at a fixed IP; a phone or laptop connects to the AP and fills in a
profile form. Backing out from this path returns to the Screensaver
directly (not to the Settings list), matching how it was entered.

## Technical design

The AP SSID and password are both derived deterministically from the
badge's MAC via SHA-256 with distinct per-purpose salts — not raw MAC
substrings, since the BSSID is broadcast in cleartext over 802.11 and a
MAC-derived password could otherwise be read off the air. The softAP and
HTTP server are stopped idempotently, including from `main.cpp`'s forced
state-transition paths that bypass this screen's own cleanup, so the AP
never leaks running.

## Dependencies

`settings::`, `badge_id::`, `storage::`.

## Storage usage

Writes the player's identity to the `my_id` NVS namespace.

## Known limitations

Requires a WiFi-capable device (phone/laptop) to complete profile setup —
there is no on-device text-entry fallback for this specific flow.

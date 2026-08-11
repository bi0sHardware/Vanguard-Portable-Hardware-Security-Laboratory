#pragma once
#include <Arduino.h>

// Badge ID: 6 hex digits derived deterministically from the chip's MAC.
// AP SSID is "ID:[BADGE_ID]".
namespace badge_id {

// Not the raw MAC's low 24 bits: that's the OUI (manufacturer prefix), identical across
// a batch, so many badges would share the same WiFi Setup SSID. Derived via
// SHA-256(station MAC + salt), truncated to 6 hex chars: unique per device.
String getBadgeId();
// SHA-256(MAC + a different salt), truncated to 8 hex chars. Not a raw MAC substring:
// the BSSID is broadcast in cleartext over 802.11, so a raw-MAC-derived password could
// be read off the air with a WiFi scanner. Doesn't defend against physical flash access.
String getBadgePassword();

// Last 3 octets of the BLUETOOTH MAC (not the station MAC above), independently derived
// so it matches storage::Contact::mac (from NimBLEAddress::toString()) for PeerDrop lookup.
String getLinkId();

} // namespace badge_id

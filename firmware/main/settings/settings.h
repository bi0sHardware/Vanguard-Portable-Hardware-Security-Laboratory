#pragma once
#include "../../include/state.h"

// Settings & Diagnostics: Sound toggle, Clear Contacts DB, WiFi Setup portal
// (WPA2 AP "ID:[BADGE_ID]" @ 192.168.4.1), Hardware Test Suite.
namespace settings {

// startAtWifiSetup: skip the List screen, go straight to WiFi/Profile Setup — used by
// the Screensaver's PROFILE shortcut. Back from that path returns to Screensaver.
void enter(bool startAtWifiSetup = false);
AppState frame();

// Idempotent; stops the softAP + HTTP server. Needed for main.cpp's forced-transition
// paths that skip this screen's own frame()-gated cleanup, so the AP doesn't leak running.
void stopWifiPortalIfActive();

} // namespace settings

#pragma once

namespace cfg {

constexpr const char* FW_VERSION = "1.0.0";

// ---- Display ----
// Panel is 240x320 native, run in landscape -> 320x240 active area.
constexpr int DISPLAY_WIDTH  = 320;
constexpr int DISPLAY_HEIGHT = 240;
// Boot splash duration = boot animation length (see boot.cpp / display/boot_anim_data.h).

// ---- Input ----
constexpr unsigned long DEBOUNCE_MS     = 25; // software debounce on top of hardware RC filter
constexpr unsigned long REPEAT_DELAY_MS = 400;
constexpr unsigned long REPEAT_RATE_MS  = 120;

// ---- LEDs ----
constexpr int LED_CHAIN_COUNT = 14;

// ---- Battery sampling ----
// GPIO1/VBATT_SENSE: dedicated ADC input (100k/100k divider + 10nF filter cap).
constexpr unsigned long BATT_SAMPLE_INTERVAL_MS = 5000;
constexpr float ADC_REF_VOLTAGE     = 3.3f;
constexpr float BATT_DIVIDER_RATIO  = 2.0f; // 100k/100k -> voltage = adc_voltage * 2
constexpr float BATT_EMPTY_V = 3.3f;
constexpr float BATT_FULL_V  = 4.2f;

// ---- PeerDrop (BLE) ----
constexpr const char* BLE_DEVICE_NAME_PREFIX = "VAJRA-";
// Spec calls for -35 but devkit testing never saw better than -51 dBm; loosened to -70 to unblock testing.
constexpr int PEERDROP_RSSI_THRESHOLD_DBM = -70; // gates whether a badge is selectable
// Auto-share threshold for near-contact hold; must stay stronger than the scan threshold above.
constexpr int PEERDROP_AUTO_CONNECT_RSSI_DBM = -45;
// True physical contact: skips even the auto-connect hold timer, shares instantly.
constexpr int PEERDROP_TOUCH_RSSI_DBM = -20;
// RSSI must hold above auto-connect threshold this long to filter noise/multipath spikes.
constexpr unsigned long PEERDROP_AUTO_CONNECT_HOLD_MS = 600;
constexpr unsigned long PEERDROP_CONSENT_TIMEOUT_MS = 30000; // waiting-for-accept timeout
// NimBLEClient::connect() blocks the whole badge (nothing else runs, not even input) while connecting; bounds that freeze.
constexpr unsigned long PEERDROP_CONNECT_TIMEOUT_MS = 5000;

// ---- WiFi setup portal ----
constexpr const char* WIFI_AP_IP = "192.168.4.1";

// ---- NVS namespaces ----
constexpr const char* NVS_NS_CONTACTS = "contacts";
constexpr const char* NVS_NS_MY_ID    = "my_id";
constexpr const char* NVS_NS_SETTINGS = "settings";
constexpr const char* NVS_NS_CHALLENGES = "challenges"; // keyed by ChallengeStage.id, bool completion flags
constexpr int NVS_MAX_CONTACTS = 32;

// ---- LoRa (RA-01SC / LLCC68) ----
constexpr float LORA_FREQUENCY_MHZ = 433.0f; // Missions 02-04 + satellite -- do not repurpose

// Radio Chat/Ship Battle channel: 700kHz off + different spreading factor, so it's
// physically unable to collide with mission/satellite traffic. Within 433.05-434.79 MHz ISM band.
constexpr float LORA_BADGELINK_FREQUENCY_MHZ = 433.7f;

} // namespace cfg

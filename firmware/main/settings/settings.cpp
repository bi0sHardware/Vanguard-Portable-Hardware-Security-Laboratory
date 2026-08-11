#include "settings.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../audio/buzzer.h"
#include "../audio/audio_manager.h"
#include "../leds/led_chain.h"
#include "../power/battery.h"
#include "../storage/storage.h"
#include "../rf/lora.h"
#include "../ui/renderer.h"
#include "../ui/widgets.h"
#include "../ui/theme.h"
#include "../challenges/challenge_engine.h"
#include "../ui/menu.h"
#include "badge_id.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <qrcode.h>
#include <cstdio>

namespace settings {

enum class Screen { List, WifiSetup, HardwareTest, ButtonTester, ConfirmResetChallenges };

// Brightness control removed: backlight has no GPIO control at all (K/A wired to
// GND/3.3V), so it never worked. display::setBrightness() kept as inert storage only.
// "Secure Badge" is a 6th row appended only once storage::hasAttackedSomeone() is true.
// kItemCount is the max array size; s_activeItemCount is the actual visible row count,
// recomputed once on enter().
static const char* kItems[] = {
    "Sound", "WiFi Setup", "Clear Contacts DB", "Hardware Test Suite", "Reset Challenges", "Secure Badge"
};
static const int kItemCount = 6;
static int s_activeItemCount = 5;

static int s_selected = 0;
static Screen s_screen = Screen::List;
static bool s_dirty = true;
static uint8_t s_brightness;
static bool s_soundEnabled;
static bool s_secureBadgeEnabled; // "Secure Badge" row state
// Cleared once consumed so a later normal List -> WiFi Setup -> Back still goes to List.
static bool s_wifiSetupIsShortcut = false;

static WebServer* s_webServer = nullptr;
static bool s_apActive = false;
// Captive-portal DNS: resolves every hostname to this badge's AP IP, so OS captive-portal
// probes (Apple/Android/Windows) land here and the setup page opens automatically.
static DNSServer s_dnsServer;

// ---- WiFi setup portal ----
// Inline CSS only — this AP has no internet route out. Loosely echoes the badge's own palette.
static const char* kPortalStyle =
    "body{background:#0c1410;color:#eaf5ec;font-family:'Courier New',monospace;"
    "margin:0;padding:24px 16px;display:flex;justify-content:center}"
    ".card{background:#16241c;border:1px solid #3a5a44;padding:20px;max-width:340px;width:100%}"
    "h2{margin:0 0 4px;font-size:1.1em;color:#b9e6c4}"
    ".sub{margin:0 0 18px;font-size:.8em;color:#7fa88c}"
    "label{display:block;font-size:.78em;color:#9fcaab;margin:14px 0 4px}"
    "input[type=text],input[type=email],input[type=tel]{width:100%;box-sizing:border-box;"
    "background:#0c1410;border:1px solid #3a5a44;color:#eaf5ec;padding:9px 10px;font:inherit;font-size:1em}"
    "input:focus{outline:none;border-color:#7fd996}"
    "button{width:100%;margin-top:20px;padding:11px;border:1px solid #7fd996;"
    "background:#16241c;color:#7fd996;font:inherit;font-size:1em;cursor:pointer}"
    "button:active{background:#7fd996;color:#0c1410}"
    ".hint{margin-top:16px;font-size:.72em;color:#5f8a6c;text-align:center;line-height:1.5}";

static void handleRoot() {
    String html =
        String("<html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
               "<title>Vanguard Setup</title><style>") + kPortalStyle +
        "</style></head><body><div class='card'>"
        "<h2>VANGUARD // IDENTITY</h2>"
        "<p class='sub'>Shared with badges via PeerDrop &amp; Radio Chat</p>"
        "<form method='POST' action='/save'>"
        "<label>Name</label><input type='text' name='name' maxlength='64' autocomplete='name' required>"
        "<label>Email</label><input type='email' name='email' maxlength='64' autocomplete='email'>"
        "<label>Phone</label><input type='tel' name='phone' maxlength='64' autocomplete='tel'>"
        "<label>Organisation</label><input type='text' name='org' maxlength='64'>"
        "<button type='submit'>SAVE IDENTITY</button>"
        "</form>"
        "<p class='hint'>This only leaves your badge when you choose to<br>"
        "share it with another badge nearby.</p>"
        "</div></body></html>";
    s_webServer->send(200, "text/html", html);
}

// Fixes two issues in raw form fields: (1) storage::packIdentity() joins fields with an
// unescaped '\x1f' delimiter, so a field containing that byte would corrupt the stored
// blob; (2) unbounded length could push arbitrarily large strings into NVS.
static String sanitizeIdentityField(const String& raw, size_t maxLen) {
    String out;
    out.reserve(raw.length());
    for (size_t i = 0; i < raw.length() && out.length() < maxLen; i++) {
        char c = raw[i];
        if ((unsigned char)c >= 0x20 && c != '\x7F') out += c; // drop all control bytes, not just \x1f
    }
    return out;
}

static void handleSave() {
    constexpr size_t kMaxFieldLen = 64;
    storage::Identity id;
    id.name = sanitizeIdentityField(s_webServer->arg("name"), kMaxFieldLen);
    id.email = sanitizeIdentityField(s_webServer->arg("email"), kMaxFieldLen);
    id.phone = sanitizeIdentityField(s_webServer->arg("phone"), kMaxFieldLen);
    id.org = sanitizeIdentityField(s_webServer->arg("org"), kMaxFieldLen);
    storage::saveMyIdentity(id);
    String html =
        String("<html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
               "<title>Vanguard Setup</title><style>") + kPortalStyle +
        "</style></head><body><div class='card' style='text-align:center'>"
        "<h2 style='color:#7fd996'>&#10003; SAVED</h2>"
        "<p class='sub'>Your identity is stored on the badge.</p>"
        "<p class='hint'>You can close this page and disconnect --<br>"
        "the badge keeps everything you entered.</p>"
        "</div></body></html>";
    s_webServer->send(200, "text/html", html);
}

// Serves the real page directly at every captive-portal probe URL, rather than a
// redirect — iOS specifically expects real content at the exact probe URL it requested.
static void handleCaptivePortalProbe() {
    handleRoot();
}

static void startWifiPortal() {
    String ssid = "ID:" + badge_id::getBadgeId();
    String password = badge_id::getBadgePassword();
    WiFi.softAP(ssid.c_str(), password.c_str());

    s_dnsServer.start(53, "*", WiFi.softAPIP());

    if (!s_webServer) s_webServer = new WebServer(80);
    s_webServer->on("/", handleRoot);
    s_webServer->on("/save", HTTP_POST, handleSave);
    // Android
    s_webServer->on("/generate_204", handleCaptivePortalProbe);
    s_webServer->on("/gen_204", handleCaptivePortalProbe);
    // Apple (iOS/macOS)
    s_webServer->on("/hotspot-detect.html", handleCaptivePortalProbe);
    s_webServer->on("/library/test/success.html", handleCaptivePortalProbe);
    // Windows
    s_webServer->on("/connecttest.txt", handleCaptivePortalProbe);
    s_webServer->on("/ncsi.txt", handleCaptivePortalProbe);
    s_webServer->onNotFound(handleCaptivePortalProbe); // route anything else to the real page
    s_webServer->begin();
    s_apActive = true;
}

static void stopWifiPortal() {
    if (s_webServer) s_webServer->stop();
    s_dnsServer.stop();
    WiFi.softAPdisconnect(true);
    s_apActive = false;
}

void stopWifiPortalIfActive() {
    if (s_apActive) stopWifiPortal();
}

// ---- List screen ----
// One row = one Region (packed state: row index | selected<<7 | sound<<6); Renderer
// only redraws rows whose selection actually changed. See ui/menu.cpp for the pattern.
static uint8_t s_listRowState[kItemCount];
static uint32_t s_footerState = 0; // battery percent only — footer text is otherwise static

static void drawListRow(Adafruit_ST7789& tft, ui::Rect bounds, const void* state) {
    uint8_t v = *(const uint8_t*)state;
    int idx = v & 0x07;
    bool selected = v & 0x80;
    bool soundEnabled = v & 0x08;
    bool secureBadgeEnabled = v & 0x10;
    const char* value = nullptr;
    if (idx == 0) value = soundEnabled ? "ON" : "OFF";
    else if (idx == 5) value = secureBadgeEnabled ? "ON" : "OFF";
    ui::widgets::listItem(tft, bounds, idx, kItems[idx], selected, value);
}

// Uptime packed into upper 24 bits of the same state word as battPercent (bits 0-7);
// Region redraws only when the packed value changes.
static void drawFooter(Adafruit_ST7789& tft, ui::Rect bounds, const void* state) {
    uint32_t v = *(const uint32_t*)state;
    uint8_t battPercent = v & 0xFF;
    uint32_t uptimeSec = v >> 8;
    tft.setTextColor(theme::COLOR_TEXT);
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    int y = bounds.y;
    tft.setCursor(bounds.x, y);
    tft.print("Badge ID: ");
    tft.print(badge_id::getBadgeId());
    y += theme::BODY_LINE_HEIGHT;
    tft.setCursor(bounds.x, y);
    tft.print("Uptime: ");
    char buf[16];
    unsigned h = uptimeSec / 3600, m = (uptimeSec / 60) % 60, sec = uptimeSec % 60;
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", h, m, sec);
    tft.print(buf);
    y += theme::BODY_LINE_HEIGHT;
    tft.setCursor(bounds.x, y);
    tft.print("Battery: ");
    tft.print(battPercent);
    tft.print("%");
}

UI_ASSERT_REGION_STATE_FITS(uint8_t);  // s_listRowState
UI_ASSERT_REGION_STATE_FITS(uint32_t); // s_footerState

static ui::Region s_listRegions[kItemCount + 1];
static bool s_listRegionsInit = false;
static int s_listRegionsBuiltForCount = -1; // footer's Y depends on row count -- rebuild if changed

static ui::Region* listRegions(int* count) {
    if (!s_listRegionsInit || s_listRegionsBuiltForCount != s_activeItemCount) {
        for (int i = 0; i < kItemCount; i++) {
            s_listRegions[i] = ui::Region{
                ui::Rect{0, (int16_t)(theme::LIST_START_Y + i * theme::LIST_ITEM_HEIGHT),
                         (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_ITEM_HEIGHT},
                drawListRow, nullptr, &s_listRowState[i], sizeof(uint8_t), theme::COLOR_BG, {}, false};
        }
        // Footer goes at slot s_activeItemCount (last visible row), not fixed slot
        // kItemCount, since the caller only walks [0, s_activeItemCount].
        s_listRegions[s_activeItemCount] = ui::Region{
            ui::Rect{0, (int16_t)(theme::LIST_START_Y + s_activeItemCount * theme::LIST_ITEM_HEIGHT + theme::BODY_LINE_HEIGHT),
                     (int16_t)cfg::DISPLAY_WIDTH, (int16_t)(theme::BODY_LINE_HEIGHT * 3)},
            drawFooter, nullptr, &s_footerState, sizeof(uint32_t), theme::COLOR_BG, {}, false};
        s_listRegionsInit = true;
        s_listRegionsBuiltForCount = s_activeItemCount;
    }
    for (int i = 0; i < s_activeItemCount; i++) {
        s_listRowState[i] = (uint8_t)i | (i == s_selected ? 0x80 : 0) |
                            (s_soundEnabled ? 0x08 : 0) |
                            (s_secureBadgeEnabled ? 0x10 : 0);
    }
    s_footerState = (uint32_t)power::batteryPercent() | ((millis() / 1000) << 8);
    *count = s_activeItemCount + 1;
    return s_listRegions;
}

static ui::Screen kListScreen{nullptr, listRegions, "Settings & Diagnostics"};

static void listFrame(AppState* next) {
    if (input::wasPressed(input::Button::JoyUp)) {
        s_selected = (s_selected - 1 + s_activeItemCount) % s_activeItemCount;
        ui::widgets::navSfx();
    }
    if (input::wasPressed(input::Button::JoyDown)) {
        s_selected = (s_selected + 1) % s_activeItemCount;
        ui::widgets::navSfx();
    }

    bool confirm = input::wasPressed(input::Button::Ok) ||
                   input::wasPressed(input::Button::JoySelect);
    if (confirm) {
        switch (s_selected) {
            case 0:
                s_soundEnabled = !s_soundEnabled;
                storage::saveSoundEnabled(s_soundEnabled);
                audio::setSoundEnabled(s_soundEnabled);
                break;
            case 1:
                startWifiPortal();
                s_screen = Screen::WifiSetup;
                s_dirty = true;
                break;
            case 2:
                storage::clearContacts();
                break;
            case 3:
                s_screen = Screen::HardwareTest;
                s_dirty = true;
                break;
            case 4:
                // Destructive (erases solved flags), so goes through a confirm dialog.
                s_screen = Screen::ConfirmResetChallenges;
                s_dirty = true;
                break;
            case 5:
                // Only reachable once s_activeItemCount==6 (see enter()).
                s_secureBadgeEnabled = !s_secureBadgeEnabled;
                storage::setSecureBadgeEnabled(s_secureBadgeEnabled);
                break;
        }
    }

    if (input::wasPressed(input::Button::Back)) {
        *next = AppState::MainMenu;
        return;
    }

    ui::frame(kListScreen);
}

// ---- WiFi setup screen ----

// esp_qrcode_generate()'s display_func is a plain C function pointer (no captures), so
// state is passed via file-statics instead, set/cleared around the call.
static Adafruit_ST7789* s_qrTft = nullptr;
static int s_qrBoxX = 0, s_qrBoxY = 0, s_qrBoxSize = 0;

static void qrDisplayCallback(esp_qrcode_handle_t qrcode) {
    if (!s_qrTft) return;
    int modules = esp_qrcode_get_size(qrcode);
    if (modules <= 0) return;
    int scale = s_qrBoxSize / modules;
    if (scale < 1) scale = 1;
    int renderSize = modules * scale;
    // Centered so the quiet-zone border comes out even on all sides — needed for camera lock-on.
    int x0 = s_qrBoxX + (s_qrBoxSize - renderSize) / 2;
    int y0 = s_qrBoxY + (s_qrBoxSize - renderSize) / 2;
    for (int y = 0; y < modules; y++) {
        for (int x = 0; x < modules; x++) {
            if (esp_qrcode_get_module(qrcode, x, y)) {
                s_qrTft->fillRect(x0 + x * scale, y0 + y * scale, scale, scale, ST77XX_BLACK);
            }
        }
    }
}

static void drawWifiQr(Adafruit_ST7789& tft, const String& text, int x, int y, int boxSize) {
    tft.fillRect(x, y, boxSize, boxSize, ST77XX_WHITE); // background doubles as the quiet zone
    s_qrTft = &tft;
    s_qrBoxX = x;
    s_qrBoxY = y;
    s_qrBoxSize = boxSize;
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    cfg.display_func = qrDisplayCallback;
    cfg.max_qrcode_version = 5; // comfortably covers the ~35-char WIFI: string below
    cfg.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
    esp_qrcode_generate(&cfg, text.c_str());
    s_qrTft = nullptr;
}

static void drawWifiSetup() {
    auto& tft = display::tft();
    theme::drawHeader(tft, "WiFi Setup");

    // Standard "WIFI:" QR format; SSID's literal colon must be backslash-escaped per the
    // format's field-separator rules, or a scanner reads the SSID as ending at "ID".
    String ssid = "ID:" + badge_id::getBadgeId();
    String wifiQr = "WIFI:T:WPA;S:ID\\:" + badge_id::getBadgeId() +
                     ";P:" + badge_id::getBadgePassword() + ";;";

    constexpr int kQrBox = 118;
    int qrX = theme::MARGIN_X;
    int qrY = theme::LIST_START_Y;
    drawWifiQr(tft, wifiQr, qrX, qrY, kQrBox);

    int textX = qrX + kQrBox + 14;
    int y = qrY;
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    tft.setTextColor(theme::COLOR_TEXT);
    tft.setCursor(textX, y); tft.print("Scan to join WiFi,");
    y += theme::BODY_LINE_HEIGHT;
    tft.setCursor(textX, y); tft.print("or connect manually:");
    y += theme::BODY_LINE_HEIGHT * 2;
    tft.setCursor(textX, y); tft.print("SSID:"); y += theme::BODY_LINE_HEIGHT;
    tft.setCursor(textX, y); tft.print(ssid);
    y += theme::BODY_LINE_HEIGHT * 2;
    tft.setCursor(textX, y); tft.print("Password:"); y += theme::BODY_LINE_HEIGHT;
    tft.setCursor(textX, y); tft.print(badge_id::getBadgePassword());

    y = qrY + kQrBox + 10;
    tft.setCursor(theme::MARGIN_X, y); tft.print("Then browse to: "); tft.print(cfg::WIFI_AP_IP);
    y += theme::BODY_LINE_HEIGHT * 2;
    tft.setCursor(theme::MARGIN_X, y); tft.print("BACK to stop portal");
}

static void wifiSetupFrame(AppState* next) {
    if (s_apActive) { s_webServer->handleClient(); s_dnsServer.processNextRequest(); }
    if (s_dirty) { drawWifiSetup(); s_dirty = false; }
    if (input::wasPressed(input::Button::Back)) {
        stopWifiPortal();
        if (s_wifiSetupIsShortcut) {
            // Entered via Screensaver's PROFILE shortcut; go all the way home.
            s_wifiSetupIsShortcut = false;
            *next = AppState::Screensaver;
            return;
        }
        s_screen = Screen::List;
        s_dirty = true;
        ui::invalidate(); // force full redraw on return to List
    }
}

// ---- Hardware Test Suite ----
static void drawHardwareTest() {
    auto& tft = display::tft();
    theme::drawHeader(tft, "Hardware Test Suite");
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    int y = theme::LIST_START_Y;
    tft.setCursor(theme::MARGIN_X, y); tft.print("LED_BUILTIN: cycling"); y += theme::BODY_LINE_HEIGHT;
    tft.setCursor(theme::MARGIN_X, y); tft.print("Shift regs: walking bit"); y += theme::BODY_LINE_HEIGHT;
    tft.setCursor(theme::MARGIN_X, y); tft.print("Buzzer: tone sweep"); y += theme::BODY_LINE_HEIGHT;
    tft.setCursor(theme::MARGIN_X, y); tft.print("Battery ADC: ");
    tft.print(power::batteryVoltage()); tft.print("V"); y += theme::BODY_LINE_HEIGHT;
    tft.setCursor(theme::MARGIN_X, y); tft.print("LoRa SPI: ");
    tft.print(rf::selfTest() ? "OK" : "FAIL"); y += theme::BODY_LINE_HEIGHT;
    tft.setCursor(theme::MARGIN_X, y); tft.print("OK: live button tester"); y += theme::BODY_LINE_HEIGHT;
    tft.setCursor(theme::MARGIN_X, y + theme::BODY_LINE_HEIGHT); tft.print("BACK to return");
}

static bool s_buttonTesterChromeDrawn = false;
static int8_t s_buttonTesterLastDown[(int)input::Button::Count];

static void hardwareTestFrame(AppState* next) {
    (void)next;
    static unsigned long lastMs = 0;
    static int walkBit = 0;
    unsigned long now = millis();
    if (now - lastMs > 200) {
        lastMs = now;
        leds::setBuiltin((now / 200) % 2 == 0);
        leds::clearAll();
        leds::setChainLed((walkBit % 14) + 1, true);
        walkBit++;
        buzzer::toneAsync(500 + (walkBit % 5) * 200); // non-blocking; blocking tone() would freeze loop()
    }
    if (s_dirty) { drawHardwareTest(); s_dirty = false; }

    if (input::wasPressed(input::Button::Ok)) {
        buzzer::stopAsync();
        s_screen = Screen::ButtonTester;
        s_buttonTesterChromeDrawn = false;
        s_dirty = true;
    }
    if (input::wasPressed(input::Button::Back)) {
        buzzer::stopAsync();
        leds::clearAll();
        s_screen = Screen::List;
        s_dirty = true;
        ui::invalidate(); // force full redraw on return to List
    }
}

// ---- Live button tester ----
static const char* kButtonNames[] = {
    "UP", "DOWN", "LEFT", "RIGHT", "SELECT", "PAUSE", "DISP", "START", "BACK", "OK"
};

// Chrome draws once on entry; each button's line only repaints when its own
// down/up state changes (avoids the full-screen-flicker bug from redrawing every tick).
static void drawButtonTesterRow(Adafruit_ST7789& tft, int i, int y, bool down) {
    tft.fillRect(theme::MARGIN_X, y, cfg::DISPLAY_WIDTH - 2 * theme::MARGIN_X, theme::BODY_LINE_HEIGHT, theme::COLOR_BG);
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    tft.setTextColor(down ? theme::COLOR_SUCCESS : theme::COLOR_TEXT);
    tft.setCursor(theme::MARGIN_X, y);
    tft.print(kButtonNames[i]);
    tft.print(down ? ": DOWN" : ": up");
}

static void drawButtonTester() {
    auto& tft = display::tft();
    int y = theme::LIST_START_Y;
    if (!s_buttonTesterChromeDrawn) {
        theme::drawHeader(tft, "Button Tester");
        int footerY = y + (int)input::Button::Count * theme::BODY_LINE_HEIGHT + theme::BODY_LINE_HEIGHT;
        tft.setTextSize(theme::BODY_TEXT_SIZE);
        tft.setTextColor(theme::COLOR_TEXT);
        tft.setCursor(theme::MARGIN_X, footerY);
        tft.print("BACK to return");
        for (int i = 0; i < (int)input::Button::Count; i++) s_buttonTesterLastDown[i] = -1;
        s_buttonTesterChromeDrawn = true;
    }
    for (int i = 0; i < (int)input::Button::Count; i++) {
        bool down = input::isDown((input::Button)i);
        if ((int8_t)down != s_buttonTesterLastDown[i]) {
            drawButtonTesterRow(tft, i, y, down);
            s_buttonTesterLastDown[i] = (int8_t)down;
        }
        y += theme::BODY_LINE_HEIGHT;
    }
}

static void buttonTesterFrame(AppState* next) {
    (void)next;
    drawButtonTester(); // delta-tracked per row, see above
    if (input::wasPressed(input::Button::Back)) {
        s_screen = Screen::HardwareTest;
        s_dirty = true;
    }
}

// ---- Reset Challenges confirmation ----
static void drawConfirmResetChallenges() {
    auto& tft = display::tft();
    ui::widgets::clearScreen(tft);
    ui::Rect box{20, 60, (int16_t)(cfg::DISPLAY_WIDTH - 40), 120};
    ui::widgets::dialog(tft, box, "RESET CHALLENGES?",
                        "All 4 flags will be cleared", "OK: Confirm   BACK: Cancel");
}

static void confirmResetChallengesFrame(AppState* next) {
    (void)next;
    if (s_dirty) { drawConfirmResetChallenges(); s_dirty = false; }
    bool confirm = input::wasPressed(input::Button::Ok) ||
                   input::wasPressed(input::Button::JoySelect);
    if (confirm) {
        challenges::resetAll();
        s_screen = Screen::List;
        s_dirty = true;
        ui::invalidate(); // force full redraw on return to List
    } else if (input::wasPressed(input::Button::Back)) {
        s_screen = Screen::List;
        s_dirty = true;
        ui::invalidate(); // force full redraw on return to List
    }
}

void enter(bool startAtWifiSetup) {
    s_selected = 0;
    s_dirty = true;
    s_brightness = storage::loadBrightness();
    s_soundEnabled = storage::loadSoundEnabled();
    s_secureBadgeEnabled = storage::isSecureBadgeEnabled();
    // "Secure Badge" row only appears once earned; recomputed here, not every frame.
    s_activeItemCount = storage::hasAttackedSomeone() ? kItemCount : kItemCount - 1;
    if (s_selected >= s_activeItemCount) s_selected = 0;
    display::setBrightness(s_brightness);
    if (startAtWifiSetup) {
        startWifiPortal();
        s_screen = Screen::WifiSetup;
        s_wifiSetupIsShortcut = true;
    } else {
        s_screen = Screen::List;
    }
    ui::invalidate();
}

AppState frame() {
    // Must match whichever identity AppState this screen was entered under, or
    // main.cpp's enterState() would continuously re-enter (see state.h's ProfileSetup).
    AppState next = s_wifiSetupIsShortcut ? AppState::ProfileSetup : AppState::Settings;
    switch (s_screen) {
        case Screen::List: listFrame(&next); break;
        case Screen::WifiSetup: wifiSetupFrame(&next); break;
        case Screen::HardwareTest: hardwareTestFrame(&next); break;
        case Screen::ButtonTester: buttonTesterFrame(&next); break;
        case Screen::ConfirmResetChallenges: confirmResetChallengesFrame(&next); break;
    }
    bool leavingSettings = next != AppState::Settings && next != AppState::ProfileSetup;
    if (leavingSettings && s_apActive) stopWifiPortal();
    return next;
}

} // namespace settings

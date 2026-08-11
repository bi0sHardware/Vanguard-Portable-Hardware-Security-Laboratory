#include "glitch.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../leds/led_manager.h"
#include "../leds/led_chain.h"
#include "../audio/audio_manager.h"
#include "../ui/theme.h"
#include <Arduino.h>
#include <esp_random.h>

namespace glitch {

static bool s_pending = false;
static bool s_active = false;
static unsigned long s_startAtMs = 0;
static AppState s_returnTo = AppState::MainMenu;

constexpr unsigned long kDurationMs = 6000;

void trigger() {
    if (s_active) return; // already glitching this victim -- a second hit doesn't restart/extend it
    s_pending = true;
}

bool consumeTriggerEvent() {
    if (!s_pending) return false;
    s_pending = false;
    return true;
}

// Approximate chiptune arrangement of the "Never Gonna Give You Up" chorus hook, transposed for buzzer register.
// Follows the real melodic shape (scale degrees 1-1-4-5-6 back to 4, lower on "down") to stay recognizable.
static const audio::Note kRickrollRiff[] = {
    { 587, 150 }, { 587, 150 }, { 784, 150 }, { 880, 150 }, { 988, 180 }, { 880, 150 }, { 784, 450 },
    { 587, 150 }, { 587, 150 }, { 784, 150 }, { 880, 150 }, { 784, 150 }, { 740, 150 }, { 659, 500 },
};
constexpr uint16_t kRickrollRiffCount = sizeof(kRickrollRiff) / sizeof(kRickrollRiff[0]);

static const char* kGlitchLines[] = {
    "SYSTEM COMPROMISED",
    "BADGE HIJACKED",
    "NEVER GONNA GIVE YOU UP",
    "NEVER GONNA LET YOU DOWN",
    "YOU'VE BEEN PWNED",
    "UNAUTHORIZED ACCESS",
};
constexpr int kGlitchLineCount = sizeof(kGlitchLines) / sizeof(kGlitchLines[0]);

void enter(AppState returnTo) {
    s_active = true;
    s_returnTo = returnTo;
    s_startAtMs = millis();
    led::stop(); // quiesce the effect system -- this screen drives the LED chain directly, same reasoning as Morse keying
    if (audio::isSoundEnabled()) audio::playMelody(kRickrollRiff, kRickrollRiffCount, /*loop=*/true);
}

static unsigned long s_lastVisualMs = 0;
constexpr unsigned long kVisualIntervalMs = 90;
static unsigned long s_lastLedMs = 0;
constexpr unsigned long kLedIntervalMs = 60;

static void drawGlitchFrame() {
    auto& tft = display::tft();
    // Redrawn (not accumulated) each tick -- fill base color first so old noise never lingers.
    uint16_t base = (esp_random() % 2) ? ST77XX_BLACK : theme::COLOR_DANGER;
    tft.fillScreen(base);
    for (int i = 0; i < 14; i++) {
        int16_t x = (int16_t)(esp_random() % cfg::DISPLAY_WIDTH);
        int16_t y = (int16_t)(esp_random() % cfg::DISPLAY_HEIGHT);
        int16_t w = (int16_t)(10 + esp_random() % 60);
        int16_t h = (int16_t)(4 + esp_random() % 18);
        uint16_t colors[] = { ST77XX_WHITE, ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE,
                              ST77XX_YELLOW, ST77XX_MAGENTA, ST77XX_CYAN, ST77XX_BLACK };
        uint16_t c = colors[esp_random() % (sizeof(colors) / sizeof(colors[0]))];
        tft.fillRect(x, y, w, h, c);
    }
    int line = (int)(esp_random() % kGlitchLineCount);
    tft.setTextSize(theme::HEADER_TEXT_SIZE);
    theme::drawCentered(tft, kGlitchLines[line], cfg::DISPLAY_HEIGHT / 2 - 8, theme::HEADER_TEXT_SIZE,
                        (esp_random() % 2) ? ST77XX_WHITE : ST77XX_RED);
}

AppState frame() {
    unsigned long now = millis();
    unsigned long elapsed = now - s_startAtMs;

    // Buttons "not working": input::wasPressed() is deliberately never read here for the whole duration.

    if (elapsed >= kDurationMs) {
        s_active = false;
        led::stop();
        audio::stop();
        AppState back = s_returnTo;
        return back;
    }

    if (now - s_lastVisualMs >= kVisualIntervalMs) {
        s_lastVisualMs = now;
        drawGlitchFrame();
    }
    if (now - s_lastLedMs >= kLedIntervalMs) {
        s_lastLedMs = now;
        // Direct writes (not led::playEffect()) since the effect system is quiescent for this screen (see enter()).
        leds::writeChainRaw((uint16_t)esp_random());
    }
    return AppState::Glitched;
}

} // namespace glitch

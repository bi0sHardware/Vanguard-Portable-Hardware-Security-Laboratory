#include "mission_complete.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../ui/theme.h"
#include "../leds/led_manager.h"
#include "../audio/audio_manager.h"
#include <Arduino.h>

namespace mission_complete {

// Longer than the per-level jingle since this plays once for the whole campaign.
static const audio::Note kVictoryFanfare[] = {
    { 784,  110 }, { 988,  110 }, { 1175, 110 }, { 1568, 110 },
    { 1175,  90 }, { 1568,  90 }, { 1976, 110 }, { 2637, 380 },
};
static constexpr uint8_t kVictoryFanfareCount =
    sizeof(kVictoryFanfare) / sizeof(kVictoryFanfare[0]);

// 5 flashes * 200ms * 2 (on+off) = 2000ms of every LED blinking.
static constexpr uint8_t  kBlinkFlashes = 5;
static constexpr uint16_t kBlinkPeriodMs = 200;

void enter() {
    Adafruit_ST7789& tft = display::tft();
    tft.fillScreen(ST77XX_BLACK);

    int y = 60;
    theme::drawCentered(tft, "MISSION COMPLETE", y, 2, theme::COLOR_SUCCESS);
    y += 36;
    theme::drawCentered(tft, "VANGUARD", y, 3, ST77XX_WHITE);
    y += 44;
    theme::drawCentered(tft, "IS UNDER YOUR", y, 1, ST77XX_WHITE);
    y += theme::BODY_LINE_HEIGHT;
    theme::drawCentered(tft, "FULL CONTROL", y, 1, ST77XX_WHITE);

    theme::drawCentered(tft, "Press any button to continue", cfg::DISPLAY_HEIGHT - 20,
                        1, theme::COLOR_ACCENT);

    led::playEffect(led::EffectId::ChallengeComplete,
                    led::EffectParams{0, kBlinkFlashes, kBlinkPeriodMs, led::kMaskAll});
    audio::playMelody(kVictoryFanfare, kVictoryFanfareCount, /*loop=*/false);
}

AppState frame() {
    if (input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::Back) ||
        input::wasPressed(input::Button::JoySelect)) {
        return AppState::MainMenu;
    }
    return AppState::MissionComplete;
}

} // namespace mission_complete

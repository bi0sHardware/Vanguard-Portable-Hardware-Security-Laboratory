#include "game_ui.h"
#include "../../include/config.h"
#include "../ui/widgets.h"
#include "../ui/theme.h"
#include <cstdio>

namespace games {

void drawPauseDialog(Adafruit_ST7789& tft) {
    ui::Rect box{(int16_t)(cfg::DISPLAY_WIDTH / 2 - 70), (int16_t)(cfg::DISPLAY_HEIGHT / 2 - 35), 140, 70};
    ui::widgets::dialog(tft, box, "PAUSED", nullptr, "PAUSE to resume");
}

void drawGameOverDialog(Adafruit_ST7789& tft, int score) {
    tft.fillScreen(ST77XX_BLACK);
    char scoreLine[24];
    snprintf(scoreLine, sizeof(scoreLine), "Score: %d", score);
    ui::Rect box{(int16_t)(cfg::DISPLAY_WIDTH / 2 - 100), (int16_t)(cfg::DISPLAY_HEIGHT / 2 - 50), 200, 100};
    ui::widgets::dialog(tft, box, "GAME OVER", scoreLine, "Press START to retry");
}

} // namespace games

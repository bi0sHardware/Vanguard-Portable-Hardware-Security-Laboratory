#pragma once
#include <Adafruit_ST7789.h>

// Shared pause/game-over dialogs for all games, built on ui::widgets::dialog().
namespace games {

// Overlay on the frozen board — does not clear the screen.
void drawPauseDialog(Adafruit_ST7789& tft);

// Clears to black, then draws the dialog box.
void drawGameOverDialog(Adafruit_ST7789& tft, int score);

} // namespace games

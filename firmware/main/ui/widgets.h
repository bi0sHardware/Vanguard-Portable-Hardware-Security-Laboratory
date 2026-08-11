#pragma once
#include "renderer.h"
#include <Adafruit_ST7789.h>

// Shared drawing primitives every screen composes instead of hand-rolling its own
// fillScreen()/setCursor()/print(). Colors/layout come from theme.h; only widgets.cpp reads
// theme colors directly, so swapping the palette is a single-file change.
namespace ui::widgets {

int screenWidth();
int headerHeight(); // y-extent reserved for header() — used by Renderer to clear on screen-enter

// Full-screen clear in the theme background color. Renderer calls this once per screen switch,
// since clearing only the new screen's region bounds can leave the previous screen's larger
// content visible underneath.
void clearScreen(Adafruit_ST7789& tft);

// Screen title + separator rule. Drawn once per screen-enter by the Renderer, not every redraw.
// `clearFirst` (default true) fills `bounds` first; pass false when the caller already cleared
// that area (avoids a redundant fillRect / SPI transfer on screen transitions).
void header(Adafruit_ST7789& tft, Rect bounds, const char* title, bool clearFirst = true);

// Small persistent status readout (e.g. Main Menu's battery %) — own Region,
// redrawn only when its text/color actually changes.
void statusBar(Adafruit_ST7789& tft, Rect bounds, const char* text, uint16_t color);

// One row of a vertical list, with the standard highlight when selected.
void listItem(Adafruit_ST7789& tft, Rect bounds, int index, const char* label,
              bool selected, const char* value = nullptr, uint8_t textSize = 0 /*0 = theme default*/);

void progressBar(Adafruit_ST7789& tft, Rect bounds, uint8_t percent0to100);

// Base for pause/confirm/game-over/notification overlays.
void dialog(Adafruit_ST7789& tft, Rect bounds, const char* title, const char* body, const char* hint);

// Transient toast/popup.
void popup(Adafruit_ST7789& tft, Rect bounds, const char* text, uint16_t bg, uint16_t fg);

// Y coordinate of the Nth list row (0-indexed).
int listItemY(int index);

// Shared navigation click, called on JoyUp/JoyDown so selection always gives audible feedback.
void navSfx();

} // namespace ui::widgets

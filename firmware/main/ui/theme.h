#pragma once
#include <Adafruit_ST7789.h>

// Shared UI style constants + drawing helpers so every screen uses the same margins, header layout,
// list-item layout, and type scale instead of picking its own coordinates.
namespace theme {

constexpr int MARGIN_X          = 14; // left margin for header title AND list items (same column)
constexpr int HEADER_Y          = 8;
constexpr int HEADER_RULE_Y      = 30;
constexpr int HEADER_TEXT_SIZE   = 2;
constexpr int LIST_START_Y       = 44;
constexpr int LIST_ITEM_HEIGHT   = 26;
constexpr int LIST_TEXT_SIZE     = 2;
constexpr int BODY_TEXT_SIZE     = 1;
constexpr int BODY_LINE_HEIGHT   = 14;

// Game Boy-style UI theme, applied via these shared constants to every menu-system screen.
// Gameplay sprite colors inside the games are left alone — functional color-coding, not UI chrome.
constexpr uint16_t COLOR_BG            = 0xB675; // GB retro background tint
constexpr uint16_t COLOR_TEXT          = 0x0821; // GB near-black green
constexpr uint16_t COLOR_HIGHLIGHT     = 0x0821; // selection background (dark, inverted-text row)
constexpr uint16_t COLOR_HIGHLIGHT_TEXT = 0xB675; // text color on a selected/highlighted row
constexpr uint16_t COLOR_ACCENT        = 0x3286; // GB dark green-grey, header rule / borders
constexpr uint16_t COLOR_ACCENT_DARK   = 0x0821; // header title text, deep accents
constexpr uint16_t COLOR_DANGER        = ST77XX_RED;
// Deliberately NOT ST77XX_GREEN — neon saturation clashed with the muted GB sage palette
// ("green looks bad" report). Mid-tone sage green, same hue family as COLOR_ACCENT/COLOR_BG.
constexpr uint16_t COLOR_SUCCESS       = 0x5D4B;

// Clears the screen and draws the standard title + separator rule, left-aligned at MARGIN_X.
void drawHeader(Adafruit_ST7789& tft, const char* title);

// Horizontally centers a single line of text at the given y/size/color.
void drawCentered(Adafruit_ST7789& tft, const char* text, int16_t y,
                   uint8_t textSize, uint16_t color);

// Y coordinate of the Nth list row (0-indexed).
int listItemY(int index);

} // namespace theme

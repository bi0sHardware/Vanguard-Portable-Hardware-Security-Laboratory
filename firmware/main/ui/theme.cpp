#include "theme.h"
#include "../../include/config.h"

namespace theme {

void drawHeader(Adafruit_ST7789& tft, const char* title) {
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_ACCENT_DARK);
    tft.setTextSize(HEADER_TEXT_SIZE);
    tft.setCursor(MARGIN_X, HEADER_Y);
    tft.print(title);
    tft.drawFastHLine(0, HEADER_RULE_Y, cfg::DISPLAY_WIDTH, COLOR_ACCENT);
}

void drawCentered(Adafruit_ST7789& tft, const char* text, int16_t y,
                   uint8_t textSize, uint16_t color) {
    int16_t x1, y1;
    uint16_t w, h;
    tft.setTextSize(textSize);
    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int16_t x = (cfg::DISPLAY_WIDTH - (int)w) / 2;
    if (x < 0) x = 0;
    tft.setTextColor(color);
    tft.setCursor(x, y);
    tft.print(text);
}

int listItemY(int index) {
    return LIST_START_Y + index * LIST_ITEM_HEIGHT;
}

} // namespace theme

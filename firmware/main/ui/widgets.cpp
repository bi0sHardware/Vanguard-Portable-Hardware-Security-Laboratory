#include "widgets.h"
#include "theme.h"
#include "../../include/config.h"
#include "../audio/audio_manager.h"
#include <Arduino.h>

namespace ui::widgets {

int screenWidth() { return cfg::DISPLAY_WIDTH; }
int headerHeight() { return theme::LIST_START_Y; } // chrome occupies [0, LIST_START_Y)

void clearScreen(Adafruit_ST7789& tft) {
    tft.fillScreen(theme::COLOR_BG);
}

void header(Adafruit_ST7789& tft, Rect bounds, const char* title, bool clearFirst) {
    if (clearFirst) tft.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, theme::COLOR_BG);
    tft.setTextColor(theme::COLOR_ACCENT_DARK);
    tft.setTextSize(theme::HEADER_TEXT_SIZE);
    tft.setTextWrap(false);
    tft.setCursor(theme::MARGIN_X, theme::HEADER_Y);

    // Clip to width: unclipped, a long data-driven title (e.g. challenges.cpp's stage.name)
    // wraps onto a second line on top of the accent rule below. Same '~'-suffix convention as listItem.
    int maxChars = (cfg::DISPLAY_WIDTH - theme::MARGIN_X) / (6 * theme::HEADER_TEXT_SIZE);
    if (maxChars < 1) maxChars = 1;
    String text = title;
    if ((int)text.length() > maxChars) {
        int keep = maxChars > 1 ? maxChars - 1 : 1;
        text = text.substring(0, keep) + "\x7E";
    }
    tft.print(text);
    tft.drawFastHLine(0, theme::HEADER_RULE_Y, cfg::DISPLAY_WIDTH, theme::COLOR_ACCENT);
}

void statusBar(Adafruit_ST7789& tft, Rect bounds, const char* text, uint16_t color) {
    tft.setTextColor(color);
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    tft.setCursor(bounds.x, bounds.y);
    tft.print(text);
}

void listItem(Adafruit_ST7789& tft, Rect bounds, int index, const char* label,
              bool selected, const char* value, uint8_t textSize) {
    (void)index;
    uint8_t size = textSize ? textSize : theme::LIST_TEXT_SIZE;
    if (selected) {
        tft.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, theme::COLOR_HIGHLIGHT);
    }
    tft.setTextColor(selected ? theme::COLOR_HIGHLIGHT_TEXT : theme::COLOR_TEXT);
    tft.setTextSize(size);
    // Default text wrap breaks at the screen edge, not this row's bounds — would corrupt the row below.
    tft.setTextWrap(false);
    tft.setCursor(bounds.x + 10, bounds.y + 4);

    String text = label;
    if (value) { text += "  "; text += value; }
    int maxChars = (bounds.w - 14) / (6 * size);
    if (maxChars < 1) maxChars = 1;
    if ((int)text.length() > maxChars) {
        int keep = maxChars > 1 ? maxChars - 1 : 1;
        text = text.substring(0, keep) + "\x7E"; // '~' truncation marker
    }
    tft.print(text);
}

void progressBar(Adafruit_ST7789& tft, Rect bounds, uint8_t percent0to100) {
    uint8_t pct = percent0to100 > 100 ? 100 : percent0to100;
    tft.drawRect(bounds.x, bounds.y, bounds.w, bounds.h, theme::COLOR_ACCENT_DARK);
    int fillW = ((bounds.w - 2) * pct) / 100;
    if (fillW > 0) {
        tft.fillRect(bounds.x + 1, bounds.y + 1, fillW, bounds.h - 2, theme::COLOR_ACCENT);
    }
}

void dialog(Adafruit_ST7789& tft, Rect bounds, const char* title, const char* body, const char* hint) {
    tft.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, theme::COLOR_BG);
    tft.drawRect(bounds.x, bounds.y, bounds.w, bounds.h, theme::COLOR_ACCENT_DARK);
    tft.drawRect(bounds.x + 2, bounds.y + 2, bounds.w - 4, bounds.h - 4, theme::COLOR_ACCENT);

    int16_t x1, y1;
    uint16_t w, h;

    tft.setTextSize(theme::HEADER_TEXT_SIZE);
    tft.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    tft.setTextColor(theme::COLOR_ACCENT_DARK);
    tft.setCursor(bounds.x + (bounds.w - (int)w) / 2, bounds.y + 14);
    tft.print(title);

    if (body) {
        tft.setTextSize(theme::BODY_TEXT_SIZE);
        tft.getTextBounds(body, 0, 0, &x1, &y1, &w, &h);
        tft.setTextColor(theme::COLOR_TEXT);
        tft.setCursor(bounds.x + (bounds.w - (int)w) / 2, bounds.y + bounds.h / 2);
        tft.print(body);
    }

    if (hint) {
        tft.setTextSize(theme::BODY_TEXT_SIZE);
        tft.getTextBounds(hint, 0, 0, &x1, &y1, &w, &h);
        tft.setTextColor(theme::COLOR_ACCENT);
        tft.setCursor(bounds.x + (bounds.w - (int)w) / 2, bounds.y + bounds.h - 18);
        tft.print(hint);
    }
}

void popup(Adafruit_ST7789& tft, Rect bounds, const char* text, uint16_t bg, uint16_t fg) {
    tft.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, bg);
    tft.drawRect(bounds.x, bounds.y, bounds.w, bounds.h, fg);
    int16_t x1, y1;
    uint16_t w, h;
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    tft.setTextColor(fg);
    tft.setCursor(bounds.x + (bounds.w - (int)w) / 2, bounds.y + (bounds.h - (int)h) / 2);
    tft.print(text);
}

int listItemY(int index) {
    return theme::LIST_START_Y + index * theme::LIST_ITEM_HEIGHT;
}

static const audio::Note kNavClick[] = {{ 700, 12 }};

void navSfx() {
    audio::playSfx(kNavClick, 1);
}

} // namespace ui::widgets

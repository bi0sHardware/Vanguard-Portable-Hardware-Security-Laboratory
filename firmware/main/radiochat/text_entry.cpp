#include "text_entry.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"
#include "../audio/audio_manager.h"
#include <Arduino.h>
#include <cstring>
#include <cstdio>

namespace text_entry {

// Charset: space (blank default), A-Z, digits, small punctuation set.
static const char kCharset[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?";
constexpr int kCharsetLen = sizeof(kCharset) - 1; // exclude trailing NUL

constexpr size_t kBufCap = 32; // hard ceiling; callers pass their own (smaller) maxLen

static char   s_buf[kBufCap + 1];
static size_t s_len = 0;
static size_t s_maxLen = 24;
static size_t s_cursor = 0; // == s_len means "append position"
static String s_title;
static bool   s_dirty = true;
static bool   s_done = false;
static bool   s_confirmed = false;

static const audio::Note kSfxTick[] = { { 900, 10 } };

static int charsetIndexOf(char c) {
    for (int i = 0; i < kCharsetLen; i++) if (kCharset[i] == c) return i;
    return 0; // unknown char -> space
}

void enter(const char* title, const char* initial, size_t maxLen) {
    s_title = title ? title : "";
    s_maxLen = (maxLen == 0 || maxLen > kBufCap) ? kBufCap : maxLen;
    size_t n = initial ? strlen(initial) : 0;
    if (n > s_maxLen) n = s_maxLen;
    memcpy(s_buf, initial ? initial : "", n);
    s_buf[n] = 0;
    s_len = n;
    s_cursor = s_len; // start ready to append
    s_dirty = true;
    s_done = false;
    s_confirmed = false;
}

static void draw() {
    auto& tft = display::tft();
    ui::widgets::clearScreen(tft);
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y},
                        s_title.c_str());

    // Text field box.
    constexpr int boxX = theme::MARGIN_X, boxY = 70, boxW = cfg::DISPLAY_WIDTH - 2 * theme::MARGIN_X, boxH = 34;
    tft.drawRoundRect(boxX, boxY, boxW, boxH, 4, theme::COLOR_ACCENT_DARK);

    // Trailing '_' shown at the append position when cursor is there.
    char shown[kBufCap + 2];
    memcpy(shown, s_buf, s_len);
    size_t shownLen = s_len;
    if (s_cursor == s_len && shownLen < s_maxLen) shown[shownLen++] = '_';
    shown[shownLen] = 0;

    tft.setTextSize(theme::LIST_TEXT_SIZE);
    tft.setTextColor(theme::COLOR_TEXT);
    tft.setTextWrap(false);
    tft.setCursor(boxX + 8, boxY + 10);
    tft.print(shown);

    // Highlight the character under the cursor with up/down arrow markers.
    int charX = boxX + 8 + (int)s_cursor * (6 * theme::LIST_TEXT_SIZE);
    int markY = boxY + boxH + 6;
    tft.fillTriangle(charX + 2, markY, charX + 8, markY, charX + 5, markY - 5, theme::COLOR_ACCENT_DARK);
    tft.fillTriangle(charX + 2, markY + 12, charX + 8, markY + 12, charX + 5, markY + 17, theme::COLOR_ACCENT_DARK);

    // Length counter.
    char counter[12];
    snprintf(counter, sizeof(counter), "%u/%u", (unsigned)s_len, (unsigned)s_maxLen);
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    tft.setTextColor(theme::COLOR_ACCENT_DARK);
    int16_t x1, y1; uint16_t w, h;
    tft.getTextBounds(counter, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(cfg::DISPLAY_WIDTH - theme::MARGIN_X - (int16_t)w, markY + 3);
    tft.print(counter);

    theme::drawCentered(tft, "UP/DN=char  L/R=move", cfg::DISPLAY_HEIGHT - 32,
                        theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
    theme::drawCentered(tft, "OK=confirm  PAUSE=del  BACK=cancel", cfg::DISPLAY_HEIGHT - 16,
                        theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
}

bool frame(bool* confirmed) {
    if (input::wasPressed(input::Button::Back)) {
        s_done = true;
        s_confirmed = false;
    } else if (input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::JoySelect)) {
        // Trim trailing space left over from an abandoned cycle-in-progress.
        while (s_len > 0 && s_buf[s_len - 1] == ' ') s_len--;
        s_buf[s_len] = 0;
        s_done = true;
        s_confirmed = true;
    } else {
        if (input::wasPressed(input::Button::JoyLeft) && s_cursor > 0) {
            s_cursor--;
            s_dirty = true;
        }
        if (input::wasPressed(input::Button::JoyRight) && s_cursor < s_len) {
            s_cursor++;
            s_dirty = true;
        }
        if (input::wasPressed(input::Button::Pause) && s_len > 0) {
            // Delete at cursor (or last char if cursor is at append position).
            size_t delAt = (s_cursor < s_len) ? s_cursor : s_len - 1;
            for (size_t i = delAt; i < s_len - 1; i++) s_buf[i] = s_buf[i + 1];
            s_len--;
            s_buf[s_len] = 0;
            if (s_cursor > s_len) s_cursor = s_len;
            audio::playSfx(kSfxTick, 1);
            s_dirty = true;
        }
        bool up = input::wasPressed(input::Button::JoyUp);
        bool down = input::wasPressed(input::Button::JoyDown);
        if ((up || down) && s_len < kBufCap) {
            if (s_cursor == s_len) {
                // Extending the buffer -- capped at maxLen.
                if (s_len < s_maxLen) {
                    s_buf[s_len] = kCharset[up ? 0 : kCharsetLen - 1];
                    s_len++;
                    s_buf[s_len] = 0;
                    audio::playSfx(kSfxTick, 1);
                    s_dirty = true;
                }
            } else {
                int idx = charsetIndexOf(s_buf[s_cursor]);
                idx = (idx + (up ? 1 : -1) + kCharsetLen) % kCharsetLen;
                s_buf[s_cursor] = kCharset[idx];
                audio::playSfx(kSfxTick, 1);
                s_dirty = true;
            }
        }
    }

    if (s_dirty || s_done) {
        draw();
        s_dirty = false;
    }

    if (s_done) {
        *confirmed = s_confirmed;
        return true;
    }
    return false;
}

const char* result() { return s_buf; }

} // namespace text_entry

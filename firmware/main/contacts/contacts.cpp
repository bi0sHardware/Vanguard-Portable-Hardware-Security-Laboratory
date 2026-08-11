#include "contacts.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../storage/storage.h"
#include "../ui/renderer.h"
#include "../ui/widgets.h"
#include "../ui/theme.h"
#include <Arduino.h>
#include <cstdio>

namespace contacts {

static int s_selected = 0;

// Rows that actually fit on screen between header and footer (NVS cap is 32 contacts,
// far more than fit without scrolling).
constexpr int kVisibleRows = (cfg::DISPLAY_HEIGHT - theme::BODY_LINE_HEIGHT - theme::LIST_START_Y) / theme::LIST_ITEM_HEIGHT;
static int s_scrollOffset = 0;

// Keeps s_selected inside the visible window, scrolling the minimum amount needed.
static void clampScroll(int count) {
    if (s_selected < s_scrollOffset) s_scrollOffset = s_selected;
    if (s_selected >= s_scrollOffset + kVisibleRows) s_scrollOffset = s_selected - kVisibleRows + 1;
    int maxOffset = count > kVisibleRows ? count - kVisibleRows : 0;
    if (s_scrollOffset > maxOffset) s_scrollOffset = maxOffset;
    if (s_scrollOffset < 0) s_scrollOffset = 0;
}

// storage::contactCount() opens a fresh NVS handle each call; cache it and refresh
// only on enter()/append/delete, not every tick, to avoid NVS-latency freezes.
static int s_cachedCount = 0;

static void refreshCount() {
    s_cachedCount = storage::contactCount();
}

static const int kListWidth = 150;
static const int kRowWidth = theme::MARGIN_X + kListWidth + 16; // aligns exactly with the preview column start
static const int kPreviewX = theme::MARGIN_X + kListWidth + 16;

// One row = one Region (packed state: index | selected<<7 | valid<<6); only changed
// rows redraw. Sized to kVisibleRows, not the NVS cap — row-to-contact mapping shifts via s_scrollOffset.
UI_ASSERT_REGION_STATE_FITS(uint8_t); // s_rowState
static uint8_t s_rowState[kVisibleRows];

static void drawContactRow(Adafruit_ST7789& tft, ui::Rect bounds, const void* state) {
    uint8_t v = *(const uint8_t*)state;
    int idx = v & 0x3F;
    bool selected = v & 0x80;
    bool valid = v & 0x40;
    if (!valid) return; // Renderer already cleared this row's bounds — nothing to draw
    storage::Contact c = storage::loadContact(idx);
    ui::widgets::listItem(tft, bounds, idx, c.identity.name.c_str(), selected);
}

// Right-hand panel: empty-state hint or selected contact's Org/Email/Phone.
// Redraws only when selection/count changes.
static int s_detailState = 0; // packed: selected | count<<8

// Clips to this panel's own width instead of letting Adafruit_GFX wrap at the screen edge.
static void printClipped(Adafruit_ST7789& tft, int x, int y, const String& s, int maxWidth) {
    tft.setTextWrap(false);
    tft.setCursor(x, y);
    int maxChars = maxWidth / (6 * theme::BODY_TEXT_SIZE);
    if (maxChars < 1) maxChars = 1;
    if ((int)s.length() > maxChars) {
        int keep = maxChars > 1 ? maxChars - 1 : 1;
        tft.print(s.substring(0, keep) + "\x7E");
    } else {
        tft.print(s);
    }
}

// Word-wraps a field value across lines within maxWidth (unlike printClipped's ellipsis
// truncation), stopping at maxY; returns y after the last line drawn.
static int printWrapped(Adafruit_ST7789& tft, int x, int y, const String& s, int maxWidth, int maxY) {
    tft.setTextWrap(false);
    int maxChars = maxWidth / (6 * theme::BODY_TEXT_SIZE);
    if (maxChars < 1) maxChars = 1;
    int pos = 0;
    int len = (int)s.length();
    if (len == 0) return y;
    while (pos < len && y <= maxY) {
        int remaining = len - pos;
        int take = remaining < maxChars ? remaining : maxChars;
        if (take < remaining) {
            int lastSpace = -1;
            for (int i = take; i > 0; i--) {
                if (s[pos + i - 1] == ' ') { lastSpace = i; break; }
            }
            if (lastSpace > 0) take = lastSpace;
        }
        String line = s.substring(pos, pos + take);
        line.trim();
        tft.setCursor(x, y);
        tft.print(line);
        y += theme::BODY_LINE_HEIGHT;
        pos += take;
    }
    return y;
}

static void drawDetail(Adafruit_ST7789& tft, ui::Rect bounds, const void* state) {
    int v = *(const int*)state;
    int selected = v & 0xFF;
    int count = (v >> 8) & 0xFF;

    if (count == 0) {
        tft.setTextColor(theme::COLOR_TEXT);
        tft.setTextSize(theme::BODY_TEXT_SIZE);
        printClipped(tft, bounds.x, bounds.y, "No contacts yet.", bounds.w);
        printClipped(tft, bounds.x, bounds.y + theme::BODY_LINE_HEIGHT, "Press OK to add a test contact.", bounds.w);
        return;
    }

    tft.drawFastVLine(bounds.x, bounds.y - 4, bounds.h, theme::COLOR_TEXT);

    storage::Contact sel = storage::loadContact(selected);
    int x = bounds.x + 8;
    int y = bounds.y;
    int w = bounds.w - 8;
    int maxY = bounds.y + bounds.h - theme::BODY_LINE_HEIGHT; // last wrapped line must still fit inside the panel
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    tft.setTextColor(theme::COLOR_TEXT);
    printClipped(tft, x, y, "Organisation:", w);
    y += theme::BODY_LINE_HEIGHT;
    y = printWrapped(tft, x, y, sel.identity.org, w, maxY);
    y += theme::BODY_LINE_HEIGHT;
    printClipped(tft, x, y, "Email:", w);
    y += theme::BODY_LINE_HEIGHT;
    y = printWrapped(tft, x, y, sel.identity.email, w, maxY);
    y += theme::BODY_LINE_HEIGHT;
    printClipped(tft, x, y, "Phone:", w);
    y += theme::BODY_LINE_HEIGHT;
    y = printWrapped(tft, x, y, sel.identity.phone, w, maxY);
}

static void drawFooterHint(Adafruit_ST7789& tft, ui::Rect bounds, const void* /*state*/) {
    tft.setTextColor(theme::COLOR_TEXT);
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    tft.setCursor(bounds.x, bounds.y);
    tft.print("OK=add mock  PAUSE=delete  BACK=exit");
}

static bool s_footerConstant = true; // never changes — draws exactly once per screen-enter

UI_ASSERT_REGION_STATE_FITS(int);  // s_detailState
UI_ASSERT_REGION_STATE_FITS(bool); // s_footerConstant

static ui::Region s_regions[kVisibleRows + 2];
static bool s_regionsInit = false;

static ui::Region* regions(int* count) {
    if (!s_regionsInit) {
        for (int i = 0; i < kVisibleRows; i++) {
            s_regions[i] = ui::Region{
                ui::Rect{0, (int16_t)theme::listItemY(i), (int16_t)kRowWidth, (int16_t)theme::LIST_ITEM_HEIGHT},
                drawContactRow, nullptr, &s_rowState[i], sizeof(uint8_t), theme::COLOR_BG, {}, false};
        }
        s_regions[kVisibleRows] = ui::Region{
            ui::Rect{(int16_t)kPreviewX, (int16_t)theme::LIST_START_Y, (int16_t)(cfg::DISPLAY_WIDTH - kPreviewX),
                     (int16_t)(cfg::DISPLAY_HEIGHT - theme::LIST_START_Y - theme::BODY_LINE_HEIGHT - 6)},
            drawDetail, nullptr, &s_detailState, sizeof(int), theme::COLOR_BG, {}, false};
        s_regions[kVisibleRows + 1] = ui::Region{
            ui::Rect{0, (int16_t)(cfg::DISPLAY_HEIGHT - theme::BODY_LINE_HEIGHT), (int16_t)cfg::DISPLAY_WIDTH,
                     (int16_t)theme::BODY_LINE_HEIGHT},
            drawFooterHint, nullptr, &s_footerConstant, sizeof(bool), theme::COLOR_BG, {}, false};
        s_regionsInit = true;
    }

    int total = s_cachedCount;
    clampScroll(total);
    for (int i = 0; i < kVisibleRows; i++) {
        int idx = s_scrollOffset + i;
        bool valid = idx < total;
        s_rowState[i] = (uint8_t)idx | (idx == s_selected ? 0x80 : 0) | (valid ? 0x40 : 0);
    }
    s_detailState = (s_selected & 0xFF) | ((total & 0xFF) << 8);

    *count = kVisibleRows + 2;
    return s_regions;
}

static ui::Screen kScreen{nullptr, regions, "Contacts"};

void enter() {
    s_selected = 0;
    s_scrollOffset = 0;
    refreshCount();
    ui::invalidate();
}

AppState frame() {
    int count = s_cachedCount;

    if (input::wasPressed(input::Button::Back)) {
        return AppState::MainMenu;
    }
    if (count > 0) {
        if (input::wasPressed(input::Button::JoyUp)) {
            s_selected = (s_selected - 1 + count) % count;
            ui::widgets::navSfx();
        }
        if (input::wasPressed(input::Button::JoyDown)) {
            s_selected = (s_selected + 1) % count;
            ui::widgets::navSfx();
        }
    }
    if (input::wasPressed(input::Button::Ok)) {
        storage::Contact mock;
        char mac[18];
        snprintf(mac, sizeof(mac), "AA:BB:CC:%02X:%02X:%02X",
                 (unsigned int)random(0, 256), (unsigned int)random(0, 256), (unsigned int)random(0, 256));
        mock.mac = mac;
        mock.identity.name = "Test User " + String(random(0, 1000));
        mock.identity.email = "test" + String(random(0, 1000)) + "@example.com";
        mock.identity.phone = "555-" + String(random(1000, 9999));
        mock.identity.org = "InCTF";
        storage::appendContact(mock);
        refreshCount();
    }
    if (input::wasPressed(input::Button::Pause) && count > 0) {
        storage::deleteContact(s_selected);
        refreshCount();
        if (s_selected >= s_cachedCount) s_selected = s_cachedCount > 0 ? s_cachedCount - 1 : 0;
    }

    ui::frame(kScreen);
    return AppState::Contacts;
}

} // namespace contacts

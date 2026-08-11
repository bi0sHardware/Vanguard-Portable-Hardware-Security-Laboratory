#include "challenges.h"
#include "challenge_data.h"
#include "challenge_state.h"
#include "challenge_engine.h"
#include "flagdesk.h"
#include "mission2.h"
#include "mission3.h"
#include "mission4.h"
#include "console.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../ui/renderer.h"
#include "../ui/widgets.h"
#include "../ui/theme.h"
#include <Arduino.h>
#include <cstring>

// Challenge Browser — spec §8.1, §9: entered from main menu, exits on BACK.
// FlagDesk ("Submit Flag") is shared infrastructure, not itself a challenge.
namespace challenges {

enum class Screen { StageList, StageDetail, FlagDesk, Telemetry, Uplink, Payload };
static Screen s_screen = Screen::StageList;
static int s_selected = 0;

// Cached instead of querying NVS per-stage every frame: stageListRegions()
// runs every tick while the list is on screen, and NVS access can spike
// (wear-leveling), causing visible stutter. Refreshed only when completion
// state can actually change (completeChallenge() / resetAll()).
static bool s_stageCompletedCache[kStageCount];
static bool s_stageUnlockedCache[kStageCount];

static int indexOfStageId(const char* id) {
    if (!id) return -1;
    for (int i = 0; i < kStageCount; i++) {
        if (strcmp(kRegistry[i].id, id) == 0) return i;
    }
    return -1;
}

static void refreshStageCache() {
    for (int i = 0; i < kStageCount; i++) {
        s_stageCompletedCache[i] = isCompleted(kRegistry[i].id);
    }
    for (int i = 0; i < kStageCount; i++) {
        const char* req = kRegistry[i].requiresId;
        if (!req) { s_stageUnlockedCache[i] = true; continue; }
        int ri = indexOfStageId(req);
        s_stageUnlockedCache[i] = (ri >= 0) && s_stageCompletedCache[ri];
    }
}

// ---- Stage list (Renderer-based) ----
// One row = one Region: avoids a whole-list redraw flashing on every nav press.
static void drawStageRow(Adafruit_ST7789& tft, ui::Rect bounds, const void* state) {
    uint8_t v = *(const uint8_t*)state;
    int idx = v & 0x07;
    bool selected = v & 0x08;
    bool completed = v & 0x10;
    bool locked = v & 0x20;
    const char* value = completed ? "[DONE]" : (locked ? "[LOCKED]" : nullptr);
    // Smaller than the shared list default (2): long "Level N: ..." names read poorly at that size.
    ui::widgets::listItem(tft, bounds, idx, kRegistry[idx].name, selected, value, theme::BODY_TEXT_SIZE);
}

// Clips to the panel's own width instead of letting Adafruit_GFX wrap at
// the screen edge, which would corrupt the row below.
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

static const ChallengeStage* findById(const char* id) {
    for (int i = 0; i < kStageCount; i++) {
        if (strcmp(kRegistry[i].id, id) == 0) return &kRegistry[i];
    }
    return nullptr;
}

// Word-wraps `text` into the panel width, at most `maxLines`, ellipsizing
// the last line if needed. Hard-clips to `maxY`: a line landing at/past it
// is never drawn. Returns the next y (may be >= maxY).
// The hard clip is the real safety net — a Region's bounds don't stop
// Adafruit_GFX from drawing past them, so an unclipped long hint could
// silently draw off the bottom of the display.
static int drawWrapped(Adafruit_ST7789& tft, int x, int y, const char* text,
                       int wPx, int maxLines, int maxY) {
    if (!text) return y;
    const int charW = 6 * theme::BODY_TEXT_SIZE; // Adafruit_GFX 5x7 glyph + 1px gap
    const int perLine = wPx / charW;
    if (perLine <= 1) return y;

    String s(text);
    int line = 0;
    while (s.length() && line < maxLines && y < maxY) {
        if ((int)s.length() <= perLine) {
            tft.setCursor(x, y);
            tft.print(s);
            return y + theme::BODY_LINE_HEIGHT;
        }
        // Break on last fitting space to keep words intact.
        int brk = -1;
        for (int i = perLine; i > 0; i--) {
            if (s[i] == ' ') { brk = i; break; }
        }
        if (brk <= 0) brk = perLine; // single long token: hard-break it

        String chunk = s.substring(0, brk);
        bool last = (line == maxLines - 1) || (y + theme::BODY_LINE_HEIGHT >= maxY);
        if (last && (int)s.length() > brk) {
            // Trim to make room for the ellipsis rather than overrunning.
            while ((int)chunk.length() > perLine - 3 && chunk.length()) {
                chunk.remove(chunk.length() - 1);
            }
            chunk += "...";
        }
        tft.setCursor(x, y);
        tft.print(chunk);
        y += theme::BODY_LINE_HEIGHT;
        line++;
        if (last) break;
        s = s.substring(brk);
        s.trim();
    }
    return y;
}

// Full-screen stage detail view — entered deliberately via OK, not shown
// while browsing. Replaces an earlier small panel that couldn't fit
// description+hint and caused needless serial spam on every arrow press.
static void drawStageDetail(Adafruit_ST7789& tft) {
    const ChallengeStage& stage = kRegistry[s_selected];
    bool locked = !s_stageUnlockedCache[s_selected];
    bool done = stage.flagSha256 && s_stageCompletedCache[s_selected];
    int w = cfg::DISPLAY_WIDTH - 2 * theme::MARGIN_X;
    int maxY = cfg::DISPLAY_HEIGHT - theme::BODY_LINE_HEIGHT - 6; // leave room for the footer line

    ui::widgets::clearScreen(tft);
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y}, stage.name);

    int y = theme::LIST_START_Y + 4;
    tft.setTextSize(theme::BODY_TEXT_SIZE);

    // Status line: progress, difficulty, category and state at a glance.
    char stars[8] = {0};
    for (int i = 0; i < 5; i++) stars[i] = (i < stage.difficulty) ? '*' : '.';
    const char* status = done ? "SOLVED" : (locked ? "LOCKED" : "OPEN");
    char line[64];
    if (stage.difficulty > 0) {
        snprintf(line, sizeof(line), "%d/4 solved  %s  %s", completedCount(), stars, status);
    } else {
        snprintf(line, sizeof(line), "%d/4 solved  %s", completedCount(), status);
    }
    tft.setTextColor(done ? theme::COLOR_SUCCESS
                          : (locked ? theme::COLOR_DANGER : theme::COLOR_ACCENT_DARK));
    printClipped(tft, theme::MARGIN_X, y, line, w);
    y += theme::BODY_LINE_HEIGHT;

    if (stage.category) {
        tft.setTextColor(theme::COLOR_ACCENT_DARK);
        printClipped(tft, theme::MARGIN_X, y, stage.category, w);
        y += theme::BODY_LINE_HEIGHT + 4;
    }

    if (stage.description) {
        tft.setTextColor(theme::COLOR_TEXT);
        y = drawWrapped(tft, theme::MARGIN_X, y, stage.description, w, 4, maxY);
        y += 4;
    }

    if (locked) {
        const ChallengeStage* req = stage.requiresId ? findById(stage.requiresId) : nullptr;
        String msg = "Locked - complete ";
        msg += req ? req->name : "a prior stage";
        msg += " first";
        tft.setTextColor(theme::COLOR_DANGER);
        drawWrapped(tft, theme::MARGIN_X, y, msg.c_str(), w, 2, maxY);
    } else if (stage.hint && !done) {
        int boxBottom = drawWrapped(tft, theme::MARGIN_X, y, stage.hint, w, 3, maxY);
        if (y < maxY) {
            tft.fillRect(0, y - 2, cfg::DISPLAY_WIDTH, (boxBottom - y) + 4, theme::COLOR_HIGHLIGHT);
        }
        tft.setTextColor(theme::COLOR_HIGHLIGHT_TEXT);
        String hint = String("! ") + stage.hint;
        drawWrapped(tft, theme::MARGIN_X, y, hint.c_str(), w, 3, maxY);
    }

    // Info/UartLeak stages have nothing for OK to do here.
    bool enterable = stage.kind != StageKind::Info && stage.kind != StageKind::UartLeak;
    const char* footer = (stage.kind == StageKind::FlagDesk) ? "OK: Open   BACK: Back"
                        : (locked || !enterable) ? "BACK: Back"
                        : "OK: Enter   BACK: Back";
    theme::drawCentered(tft, footer, cfg::DISPLAY_HEIGHT - 16, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
}

// Mirrors the selected stage to the serial console. Fires once per
// selection change, not every frame.
static void printStageDetails(int idx) {
    // Guards every raw Serial.print/println below — native USB-CDC writes
    // can block indefinitely with no terminal draining the buffer.
    if (!Serial) return;

    const ChallengeStage& stage = kRegistry[idx];
    bool locked = !s_stageUnlockedCache[idx];
    bool done = stage.flagSha256 && s_stageCompletedCache[idx];

    Serial.println();
    console::rule('-');
    Serial.print(console::BOLD); Serial.print(console::CYAN);
    Serial.print("  "); Serial.print(stage.name); Serial.println(console::RESET);
    if (stage.category) console::field("Category", stage.category);
    if (stage.difficulty > 0) {
        char stars[8] = {0};
        for (int i = 0; i < 5; i++) stars[i] = (i < stage.difficulty) ? '*' : '.';
        console::field("Difficulty", stars);
    }
    console::field("Status", done ? "SOLVED" : (locked ? "LOCKED" : "OPEN"),
                   done ? console::GREEN : (locked ? console::RED : console::YELLOW));
    if (stage.description) {
        Serial.println();
        Serial.println(stage.description);
    }
    if (locked) {
        const ChallengeStage* req = stage.requiresId ? findById(stage.requiresId) : nullptr;
        Serial.println();
        console::warn((String("Locked - complete \"") +
                       (req ? req->name : "a prior stage") + "\" first.").c_str());
    } else if (stage.hint && !done) {
        Serial.println();
        console::step("Hint:");
        Serial.println(stage.hint);
    }
    console::rule('-');
}

UI_ASSERT_REGION_STATE_FITS(uint8_t); // s_stageRowState

static uint8_t s_stageRowState[kStageCount];
static ui::Region s_stageRegions[kStageCount];
static bool s_stageRegionsInit = false;

static ui::Region* stageListRegions(int* count) {
    if (!s_stageRegionsInit) {
        for (int i = 0; i < kStageCount; i++) {
            s_stageRegions[i] = ui::Region{
                ui::Rect{0, (int16_t)(theme::LIST_START_Y + i * theme::LIST_ITEM_HEIGHT),
                         (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_ITEM_HEIGHT},
                drawStageRow, nullptr, &s_stageRowState[i], sizeof(uint8_t), theme::COLOR_BG, {}, false};
        }
        s_stageRegionsInit = true;
    }
    for (int i = 0; i < kStageCount; i++) {
        s_stageRowState[i] = (uint8_t)i | (i == s_selected ? 0x08 : 0) |
                             (s_stageCompletedCache[i] ? 0x10 : 0) |
                             (!s_stageUnlockedCache[i] ? 0x20 : 0);
    }
    *count = kStageCount;
    return s_stageRegions;
}

static ui::Screen kStageListScreen{nullptr, stageListRegions, "Challenges"};

static bool s_detailDirty = true;

// Opens the full-screen detail view for the selected row.
static void openStageDetail() {
    s_screen = Screen::StageDetail;
    s_detailDirty = true;
    printStageDetails(s_selected);
}

static void stageListFrame(AppState* next) {
    if (input::wasPressed(input::Button::JoyUp)) {
        s_selected = (s_selected - 1 + kStageCount) % kStageCount;
        ui::widgets::navSfx();
    }
    if (input::wasPressed(input::Button::JoyDown)) {
        s_selected = (s_selected + 1) % kStageCount;
        ui::widgets::navSfx();
    }
    if (input::wasPressed(input::Button::Back)) {
        *next = AppState::MainMenu;
        return;
    }
    bool confirm = input::wasPressed(input::Button::Ok) ||
                   input::wasPressed(input::Button::JoySelect);
    if (confirm) openStageDetail();

    ui::frame(kStageListScreen);
}

// Enters the mission/utility screen behind the currently selected stage.
static void enterSelectedStage() {
    StageKind kind = kRegistry[s_selected].kind;
    if (kind == StageKind::FlagDesk) {
        // Always reachable regardless of other stages' lock state.
        flagdesk::enter();
        s_screen = Screen::FlagDesk;
    } else if (kind == StageKind::Telemetry && s_stageUnlockedCache[s_selected]) {
        mission2::enter();
        s_screen = Screen::Telemetry;
    } else if (kind == StageKind::Uplink && s_stageUnlockedCache[s_selected]) {
        mission3::enter();
        s_screen = Screen::Uplink;
    } else if (kind == StageKind::Payload && s_stageUnlockedCache[s_selected]) {
        mission4::enter();
        s_screen = Screen::Payload;
    }
    // Info/UartLeak and locked stages: OK does nothing here.
}

static void stageDetailFrame(AppState* next) {
    (void)next;
    if (input::wasPressed(input::Button::Back)) {
        s_screen = Screen::StageList;
        ui::invalidate(); // StageList is Renderer-based; force full redraw on return
        return;
    }
    bool confirm = input::wasPressed(input::Button::Ok) ||
                   input::wasPressed(input::Button::JoySelect);
    if (confirm) {
        enterSelectedStage();
        return; // whichever screen just entered owns drawing from here
    }
    if (s_detailDirty) {
        drawStageDetail(display::tft());
        s_detailDirty = false;
    }
}

void enter() {
    s_screen = Screen::StageList;
    s_selected = 0;
    refreshStageCache();
    ui::invalidate();
    console::banner("OPERATION VAJRA :: CHALLENGE BROWSER",
                    "Select a level with OK to see its full details");
}

AppState frame() {
    AppState next = AppState::Challenges;
    switch (s_screen) {
        case Screen::StageList:
            stageListFrame(&next);
            break;
        case Screen::StageDetail:
            stageDetailFrame(&next);
            break;
        case Screen::FlagDesk:
            if (flagdesk::frame()) {
                s_screen = Screen::StageList;
                refreshStageCache(); // only place completion state can have changed (completeChallenge())
                ui::invalidate(); // StageList is Renderer-based; force full redraw on return
            }
            break;
        case Screen::Uplink:
            if (mission3::frame()) {
                s_screen = Screen::StageList;
                ui::invalidate(); // StageList is Renderer-based; force full redraw on return
            }
            break;
        case Screen::Payload:
            if (mission4::frame()) {
                s_screen = Screen::StageList;
                ui::invalidate(); // StageList is Renderer-based; force full redraw on return
            }
            break;
        case Screen::Telemetry:
            if (mission2::frame()) {
                s_screen = Screen::StageList;
                ui::invalidate(); // StageList is Renderer-based; force full redraw on return
            }
            break;
    }
    return next;
}

} // namespace challenges

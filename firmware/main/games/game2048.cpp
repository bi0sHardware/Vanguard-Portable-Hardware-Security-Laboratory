#include "game2048.h"
#include "game_ui.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../audio/audio_manager.h"
#include "../leds/led_manager.h"
#include "../ui/theme.h"
#include <Arduino.h>
#include <cstdio>

namespace games::game2048 {

static const audio::Note kSfxMerge[] = {{ 700, 30 }, { 900, 30 }};
static const audio::Note kSfxSpawn[] = {{ 1400, 20 }};
static const audio::Note kSfxNoMove[] = {{ 200, 40 }};

constexpr int GRID = 4;
constexpr int CELL = 42;
constexpr int GAP = 4;
constexpr int BOARD_W = GRID * CELL + (GRID + 1) * GAP;
constexpr int BOARD_H = BOARD_W;
constexpr int BOARD_X = (cfg::DISPLAY_WIDTH - BOARD_W) / 2;
constexpr int BOARD_Y = 30;
constexpr int FOOTER_Y = cfg::DISPLAY_HEIGHT - 12;

enum class Dir { Up, Down, Left, Right };

static int s_board[GRID][GRID];
static int s_score;
static bool s_reached2048;
static bool s_paused;
static bool s_gameOver;
static bool s_titleScreen;
static bool s_boardDirty = true;
static bool s_scoreDirty = true;
// s_prevBoard tracks what's on screen so a move repaints only changed cells,
// avoiding a full-screen flash on every keypress.
static int s_prevBoard[GRID][GRID];
static bool s_prevBoardValid = false;

static uint16_t colorBg(Adafruit_ST7789& tft)      { return tft.color565(250, 248, 239); }
static uint16_t colorFrame(Adafruit_ST7789& tft)   { return tft.color565(187, 173, 160); }
static uint16_t colorEmpty(Adafruit_ST7789& tft)   { return tft.color565(205, 193, 180); }
static uint16_t colorTextDark(Adafruit_ST7789& tft)  { return tft.color565(119, 110, 101); }

static uint16_t tileColor(Adafruit_ST7789& tft, int value) {
    switch (value) {
        case 2:    return tft.color565(238, 228, 218);
        case 4:    return tft.color565(237, 224, 200);
        case 8:    return tft.color565(242, 177, 121);
        case 16:   return tft.color565(245, 149, 99);
        case 32:   return tft.color565(246, 124, 95);
        case 64:   return tft.color565(246, 94, 59);
        case 128:  return tft.color565(237, 207, 114);
        case 256:  return tft.color565(237, 204, 97);
        case 512:  return tft.color565(237, 200, 80);
        case 1024: return tft.color565(237, 197, 63);
        case 2048: return tft.color565(237, 194, 46);
        default:   return tft.color565(60, 58, 50); // beyond 2048 -- keeps going, just runs out of named colors
    }
}

static bool emptyCellExists() {
    for (int r = 0; r < GRID; r++)
        for (int c = 0; c < GRID; c++)
            if (s_board[r][c] == 0) return true;
    return false;
}

static void spawnRandomTile() {
    int emptyR[GRID * GRID], emptyC[GRID * GRID], n = 0;
    for (int r = 0; r < GRID; r++)
        for (int c = 0; c < GRID; c++)
            if (s_board[r][c] == 0) { emptyR[n] = r; emptyC[n] = c; n++; }
    if (n == 0) return;
    int pick = random(0, n);
    s_board[emptyR[pick]][emptyC[pick]] = (random(0, 10) == 0) ? 4 : 2; // 10% chance of a 4, matches the original game's odds
}

static bool anyMovePossible() {
    if (emptyCellExists()) return true;
    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            int v = s_board[r][c];
            if (c + 1 < GRID && s_board[r][c + 1] == v) return true;
            if (r + 1 < GRID && s_board[r + 1][c] == v) return true;
        }
    }
    return false;
}

// Compact-then-merge one line; `i++` skip-ahead after a merge ensures each
// tile merges at most once per move (2,2,2,2 -> 4,4, not 8).
static bool slideMergeLine(int* line, int* scoreDelta) {
    int vals[GRID], n = 0;
    for (int i = 0; i < GRID; i++) if (line[i] != 0) vals[n++] = line[i];
    int merged[GRID] = {0, 0, 0, 0};
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (i + 1 < n && vals[i] == vals[i + 1]) {
            merged[m] = vals[i] * 2;
            *scoreDelta += merged[m];
            if (merged[m] == 2048 && !s_reached2048) {
                s_reached2048 = true;
                led::playEffect(led::EffectId::ChallengeComplete,
                                 led::EffectParams{0, 3, 90, led::kMaskAll});
            }
            m++;
            i++; // skip the tile just merged into this one
        } else {
            merged[m++] = vals[i];
        }
    }
    bool changed = false;
    for (int i = 0; i < GRID; i++) {
        if (merged[i] != line[i]) changed = true;
        line[i] = merged[i];
    }
    return changed;
}

static bool applyMove(Dir dir, int* scoreDelta) {
    bool moved = false;
    switch (dir) {
        case Dir::Left:
            for (int r = 0; r < GRID; r++) {
                int line[GRID];
                for (int c = 0; c < GRID; c++) line[c] = s_board[r][c];
                if (slideMergeLine(line, scoreDelta)) moved = true;
                for (int c = 0; c < GRID; c++) s_board[r][c] = line[c];
            }
            break;
        case Dir::Right:
            for (int r = 0; r < GRID; r++) {
                int line[GRID];
                for (int c = 0; c < GRID; c++) line[c] = s_board[r][GRID - 1 - c];
                if (slideMergeLine(line, scoreDelta)) moved = true;
                for (int c = 0; c < GRID; c++) s_board[r][GRID - 1 - c] = line[c];
            }
            break;
        case Dir::Up:
            for (int c = 0; c < GRID; c++) {
                int line[GRID];
                for (int r = 0; r < GRID; r++) line[r] = s_board[r][c];
                if (slideMergeLine(line, scoreDelta)) moved = true;
                for (int r = 0; r < GRID; r++) s_board[r][c] = line[r];
            }
            break;
        case Dir::Down:
            for (int c = 0; c < GRID; c++) {
                int line[GRID];
                for (int r = 0; r < GRID; r++) line[r] = s_board[GRID - 1 - r][c];
                if (slideMergeLine(line, scoreDelta)) moved = true;
                for (int r = 0; r < GRID; r++) s_board[GRID - 1 - r][c] = line[r];
            }
            break;
    }
    return moved;
}

static void resetGame() {
    led::stop();
    for (int r = 0; r < GRID; r++)
        for (int c = 0; c < GRID; c++)
            s_board[r][c] = 0;
    s_score = 0;
    s_reached2048 = false;
    s_paused = false;
    s_gameOver = false;
    spawnRandomTile();
    spawnRandomTile();
    s_prevBoardValid = false; // force a full redraw on the next drawBoardFull()
    s_boardDirty = true;
    s_scoreDirty = true;
}

void enter() {
    s_titleScreen = true;
    s_boardDirty = true;
    randomSeed(micros());
}

static void drawTitle() {
    auto& tft = display::tft();
    tft.fillScreen(ST77XX_BLACK);
    theme::drawCentered(tft, "2048", 90, 4, tft.color565(237, 194, 46));
    theme::drawCentered(tft, "Press START", 150, 2, ST77XX_WHITE);
}

static void drawTile(Adafruit_ST7789& tft, int r, int c, int value) {
    int x = BOARD_X + GAP + c * (CELL + GAP);
    int y = BOARD_Y + GAP + r * (CELL + GAP);
    if (value == 0) {
        tft.fillRoundRect(x, y, CELL, CELL, 4, colorEmpty(tft));
        return;
    }
    tft.fillRoundRect(x, y, CELL, CELL, 4, tileColor(tft, value));
    char buf[6];
    snprintf(buf, sizeof(buf), "%d", value);
    uint16_t textColor = (value <= 4) ? colorTextDark(tft) : ST77XX_WHITE;
    int textSize = (value < 100) ? 2 : (value < 1000 ? 2 : 1);
    // Adafruit_GFX default font is 6px wide / 8px tall per glyph at size 1.
    int textW = (int)strlen(buf) * 6 * textSize;
    int textH = 8 * textSize;
    tft.setTextColor(textColor);
    tft.setTextSize(textSize);
    tft.setCursor(x + (CELL - textW) / 2, y + (CELL - textH) / 2);
    tft.print(buf);
}

static void drawBoardFull(Adafruit_ST7789& tft) {
    tft.fillScreen(colorBg(tft));
    tft.fillRoundRect(BOARD_X, BOARD_Y, BOARD_W, BOARD_H, 6, colorFrame(tft));
    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            drawTile(tft, r, c, s_board[r][c]);
            s_prevBoard[r][c] = s_board[r][c];
        }
    }
    s_prevBoardValid = true;
}

// Redraws only cells whose value changed since the last draw.
static void drawBoardDelta(Adafruit_ST7789& tft) {
    if (!s_prevBoardValid) { drawBoardFull(tft); return; }
    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            if (s_board[r][c] != s_prevBoard[r][c]) {
                drawTile(tft, r, c, s_board[r][c]);
                s_prevBoard[r][c] = s_board[r][c];
            }
        }
    }
}

static void drawHeader(Adafruit_ST7789& tft) {
    tft.fillRect(0, 0, cfg::DISPLAY_WIDTH, BOARD_Y - 2, colorBg(tft));
    tft.setTextColor(colorTextDark(tft));
    tft.setTextSize(2);
    tft.setCursor(BOARD_X, 6);
    tft.print("2048");
    char scoreBuf[24];
    snprintf(scoreBuf, sizeof(scoreBuf), "Score: %d", s_score);
    tft.setTextSize(1);
    int textW = (int)strlen(scoreBuf) * 6;
    tft.setCursor(BOARD_X + BOARD_W - textW, 12);
    tft.print(scoreBuf);
}

static void drawFooter(Adafruit_ST7789& tft) {
    tft.fillRect(0, FOOTER_Y - 2, cfg::DISPLAY_WIDTH, 12, colorBg(tft));
    theme::drawCentered(tft, "Slide to merge  BACK=Exit", FOOTER_Y, 1, colorTextDark(tft));
}

static void drawGameOver() {
    games::drawGameOverDialog(display::tft(), s_score);
}

AppState frame() {
    if (input::wasPressed(input::Button::Back)) {
        led::stop();
        return AppState::GamesMenu;
    }

    if (s_titleScreen) {
        if (s_boardDirty) { drawTitle(); s_boardDirty = false; }
        if (input::wasPressed(input::Button::Start)) {
            resetGame();
            s_titleScreen = false;
        }
        return AppState::Game2048;
    }

    if (s_gameOver) {
        if (s_boardDirty) { drawGameOver(); s_boardDirty = false; }
        if (input::wasPressed(input::Button::Start)) {
            resetGame();
        }
        return AppState::Game2048;
    }

    if (input::wasPressed(input::Button::Pause)) {
        s_paused = !s_paused;
        s_scoreDirty = true;
        if (!s_paused) s_boardDirty = true; // resuming: board is stale under the pause banner
    }
    auto& tft = display::tft();
    if (s_paused) {
        if (s_boardDirty) { drawBoardFull(tft); drawHeader(tft); s_boardDirty = false; }
        if (s_scoreDirty) {
            games::drawPauseDialog(tft);
            s_scoreDirty = false;
        }
        return AppState::Game2048;
    }

    if (s_boardDirty) { drawBoardFull(tft); drawHeader(tft); drawFooter(tft); s_boardDirty = false; }

    Dir dir; bool pressed = true;
    if (input::wasPressed(input::Button::JoyUp)) dir = Dir::Up;
    else if (input::wasPressed(input::Button::JoyDown)) dir = Dir::Down;
    else if (input::wasPressed(input::Button::JoyLeft)) dir = Dir::Left;
    else if (input::wasPressed(input::Button::JoyRight)) dir = Dir::Right;
    else pressed = false;

    if (pressed) {
        int scoreDelta = 0;
        bool moved = applyMove(dir, &scoreDelta);
        if (moved) {
            s_score += scoreDelta;
            spawnRandomTile();
            audio::playSfx(scoreDelta > 0 ? kSfxMerge : kSfxSpawn, scoreDelta > 0 ? 2 : 1);
            drawBoardDelta(tft);
            drawHeader(tft);
            if (!anyMovePossible()) {
                s_gameOver = true;
                s_boardDirty = true; // next frame draws the game-over screen fresh
                // Full 14-LED chain, not FailurePulse's default red-only mask.
                led::playEffect(led::EffectId::FailurePulse, led::EffectParams{0, 0, 0, led::kMaskAll});
            }
        } else {
            audio::playSfx(kSfxNoMove, 1); // a move that changes nothing (blocked edge) gets a distinct short blip, not silence
        }
    }

    return AppState::Game2048;
}

} // namespace games::game2048

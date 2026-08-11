#include "snake.h"
#include "game_ui.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../audio/audio_manager.h"
#include "../leds/led_manager.h"
#include "../ui/theme.h"
#include <Arduino.h>
#include <cstdio>

namespace games::snake {

static const audio::Note kSfxCrash[] = {{ 150, 300 }};
static const audio::Note kSfxEat[]   = {{ 1200, 40 }};

constexpr int CELL = 10;
constexpr int COLS = 320 / CELL;
constexpr int ROWS = 22; // leave a score strip at the bottom (220..240)
constexpr int MAX_LEN = COLS * ROWS;
constexpr unsigned long MOVE_INTERVAL_MS = 150;
constexpr uint16_t COLOR_BORDER = 0x7BEF;

enum class Dir { Up, Down, Left, Right };

struct Point { int x, y; };

static Point s_body[MAX_LEN];
static int s_len;
static Dir s_dir;
static Point s_food;
static int s_score;
static bool s_paused;
static bool s_gameOver;
static bool s_titleScreen;
static unsigned long s_lastMoveMs;
// s_boardDirty: full redraw needed. s_scoreDirty: score strip only.
// Normal movement does a delta redraw (changed cells only) — see tetris.cpp.
static bool s_boardDirty = true;
static bool s_scoreDirty = true;

static void placeFood() {
    while (true) {
        int fx = random(0, COLS);
        int fy = random(0, ROWS);
        bool onSnake = false;
        for (int i = 0; i < s_len; i++) {
            if (s_body[i].x == fx && s_body[i].y == fy) { onSnake = true; break; }
        }
        if (!onSnake) { s_food = { fx, fy }; return; }
    }
}

static void resetGame() {
    led::stop(); // clear any lingering crash Warning blink from a previous round
    s_len = 3;
    s_body[0] = { COLS / 2, ROWS / 2 };
    s_body[1] = { COLS / 2 - 1, ROWS / 2 };
    s_body[2] = { COLS / 2 - 2, ROWS / 2 };
    s_dir = Dir::Right;
    s_score = 0;
    s_paused = false;
    s_gameOver = false;
    placeFood();
    s_lastMoveMs = millis();
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
    theme::drawCentered(tft, "SNAKE", 90, 4, ST77XX_GREEN);
    theme::drawCentered(tft, "Press START", 150, 2, ST77XX_WHITE);
}

static void drawCell(Adafruit_ST7789& tft, Point p, uint16_t color) {
    tft.fillRect(p.x * CELL, p.y * CELL, CELL - 1, CELL - 1, color);
}

// Rounded head with two "eyes" on the leading edge so travel direction reads visually.
static void drawHeadCell(Adafruit_ST7789& tft, Point p, Dir dir) {
    int x = p.x * CELL, y = p.y * CELL;
    tft.fillRoundRect(x, y, CELL - 1, CELL - 1, 2, ST77XX_YELLOW);
    uint16_t eyeColor = ST77XX_BLACK;
    switch (dir) {
        case Dir::Up:    tft.fillRect(x + 1, y + 1, 1, 1, eyeColor); tft.fillRect(x + CELL - 3, y + 1, 1, 1, eyeColor); break;
        case Dir::Down:  tft.fillRect(x + 1, y + CELL - 3, 1, 1, eyeColor); tft.fillRect(x + CELL - 3, y + CELL - 3, 1, 1, eyeColor); break;
        case Dir::Left:  tft.fillRect(x + 1, y + 1, 1, 1, eyeColor); tft.fillRect(x + 1, y + CELL - 3, 1, 1, eyeColor); break;
        case Dir::Right: tft.fillRect(x + CELL - 3, y + 1, 1, 1, eyeColor); tft.fillRect(x + CELL - 3, y + CELL - 3, 1, 1, eyeColor); break;
    }
}

// Radius is CELL/2-2, not -1: a radius-4 circle pokes 1px past the cell's
// erase rect, leaving red residue when a segment moves over eaten food.
static void drawFoodCell(Adafruit_ST7789& tft, Point p) {
    int cx = p.x * CELL + CELL / 2, cy = p.y * CELL + CELL / 2;
    tft.fillCircle(cx, cy, CELL / 2 - 2, ST77XX_RED);
}

static void drawScoreStrip(Adafruit_ST7789& tft) {
    tft.fillRect(0, ROWS * CELL, cfg::DISPLAY_WIDTH, cfg::DISPLAY_HEIGHT - ROWS * CELL, ST77XX_BLACK);
    tft.drawFastHLine(0, ROWS * CELL, cfg::DISPLAY_WIDTH, COLOR_BORDER); // safe: outside the wrapping field
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(4, ROWS * CELL + 6);
    tft.print("Score: ");
    tft.print(s_score);
}

static void drawBoardFull(Adafruit_ST7789& tft) {
    // No border on the field edge: the snake wraps through it, and an edge
    // border would get overwritten by cells drawn on top of it.
    tft.fillRect(0, 0, cfg::DISPLAY_WIDTH, ROWS * CELL, ST77XX_BLACK);
    for (int i = 0; i < s_len; i++) {
        if (i == 0) drawHeadCell(tft, s_body[i], s_dir);
        else drawCell(tft, s_body[i], ST77XX_GREEN);
    }
    drawFoodCell(tft, s_food);
}

static void drawGameOver() {
    games::drawGameOverDialog(display::tft(), s_score);
}

AppState frame() {
    if (input::wasPressed(input::Button::Back)) {
        led::stop(); // in case a crash Warning blink is still running
        return AppState::GamesMenu;
    }

    if (s_titleScreen) {
        if (s_boardDirty) { drawTitle(); s_boardDirty = false; }
        if (input::wasPressed(input::Button::Start)) {
            resetGame();
            s_titleScreen = false;
        }
        return AppState::Snake;
    }

    if (s_gameOver) {
        if (s_boardDirty) { drawGameOver(); s_boardDirty = false; }
        if (input::wasPressed(input::Button::Start)) {
            resetGame();
        }
        return AppState::Snake;
    }

    if (input::wasPressed(input::Button::Pause)) {
        s_paused = !s_paused;
        s_scoreDirty = true;
        if (!s_paused) {
            s_boardDirty = true; // resuming: board may be stale under the pause banner
            // Reset here: elapsed time still accrues during pause (early return
            // just skips acting on it), so without this a long pause + turn could
            // fire an instant move on unpause and crash the snake into itself.
            s_lastMoveMs = millis();
        }
    }
    if (s_paused) {
        auto& tft = display::tft();
        if (s_boardDirty) { drawBoardFull(tft); s_boardDirty = false; }
        if (s_scoreDirty) {
            drawScoreStrip(tft);
            games::drawPauseDialog(tft); // drawn once per pause-toggle, on top of the frozen board
            s_scoreDirty = false;
        }
        return AppState::Snake;
    }

    if (input::wasPressed(input::Button::JoyUp) && s_dir != Dir::Down) s_dir = Dir::Up;
    if (input::wasPressed(input::Button::JoyDown) && s_dir != Dir::Up) s_dir = Dir::Down;
    if (input::wasPressed(input::Button::JoyLeft) && s_dir != Dir::Right) s_dir = Dir::Left;
    if (input::wasPressed(input::Button::JoyRight) && s_dir != Dir::Left) s_dir = Dir::Right;

    auto& tft = display::tft();
    if (s_boardDirty) { drawBoardFull(tft); s_boardDirty = false; }
    if (s_scoreDirty) { drawScoreStrip(tft); s_scoreDirty = false; }

    unsigned long now = millis();
    if (now - s_lastMoveMs >= MOVE_INTERVAL_MS) {
        s_lastMoveMs = now;

        Point next = s_body[0];
        switch (s_dir) {
            case Dir::Up: next.y--; break;
            case Dir::Down: next.y++; break;
            case Dir::Left: next.x--; break;
            case Dir::Right: next.x++; break;
        }
        // Wrap around the field edges instead of dying on wall contact —
        // going off one side brings the snake back on the opposite side.
        if (next.x < 0) next.x = COLS - 1;
        else if (next.x >= COLS) next.x = 0;
        if (next.y < 0) next.y = ROWS - 1;
        else if (next.y >= ROWS) next.y = 0;

        bool crash = false;
        for (int i = 0; i < s_len; i++) {
            if (s_body[i].x == next.x && s_body[i].y == next.y) { crash = true; break; }
        }

        if (crash) {
            s_gameOver = true;
            s_boardDirty = true; // next frame draws the game-over screen fresh
            audio::playSfx(kSfxCrash, 1);
        } else {
            Point prevHead = s_body[0];
            Point tailCell = s_body[s_len - 1];
            bool ate = (next.x == s_food.x && next.y == s_food.y);

            for (int i = (ate ? s_len : s_len - 1); i > 0; i--) {
                s_body[i] = s_body[i - 1];
            }
            s_body[0] = next;

            // Delta redraw: only the cells that actually changed.
            drawCell(tft, prevHead, ST77XX_GREEN); // old head becomes a body segment
            drawHeadCell(tft, s_body[0], s_dir); // new head
            if (!ate) drawCell(tft, tailCell, ST77XX_BLACK); // tail vacated

            if (ate) {
                s_len++;
                s_score += 10;
                s_scoreDirty = true;
                audio::playSfx(kSfxEat, 1);
                // Flash the red+green center LEDs (LED1-8) twice —
                // self-terminating, doesn't need a stop() call.
                led::playEffect(led::EffectId::ChallengeComplete,
                                 led::EffectParams{0, 2, 60, (uint16_t)(led::kMaskRed | led::kMaskGreen)});
                if (s_len < MAX_LEN) {
                    placeFood();
                    drawFoodCell(tft, s_food);
                }
            }
        }
    }

    return AppState::Snake;
}

} // namespace games::snake

#include "tetris.h"
#include "game_ui.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../audio/audio_manager.h"
#include "../leds/led_chain.h"
#include "../leds/led_manager.h"
#include "../ui/theme.h"
#include <Arduino.h>
#include <cstdio>

namespace games::tetris {

// Played via the shared Audio Manager (non-blocking). Frequency values only
// distinguish note-vs-rest — see buzzer.h (active buzzer, no pitch).
static const audio::Note kSfxMove[]      = {{ 600, 15 }};
static const audio::Note kSfxRotate[]    = {{ 900, 20 }};
static const audio::Note kSfxLock[]      = {{ 400, 30 }};
static const audio::Note kSfxLineClear[] = {{ 1500, 60 }};
static const audio::Note kSfxGameOver[]  = {{ 150, 300 }};

constexpr int COLS = 10;
constexpr int ROWS = 20;
constexpr int CELL = 11;
constexpr int BOARD_X = 20;
constexpr int BOARD_Y = 10;
constexpr unsigned long DROP_INTERVAL_MS = 500;
constexpr unsigned long SOFT_DROP_INTERVAL_MS = 60;

constexpr uint16_t COLOR_GRID     = 0x18E3;
constexpr uint16_t COLOR_BORDER   = 0x7BEF;
constexpr uint16_t COLOR_PANEL_BG = 0x0841;

constexpr int PANEL_X = BOARD_X + COLS * CELL + 25; // 20 + 110 + 25 = 155
constexpr int PANEL_W = cfg::DISPLAY_WIDTH - PANEL_X - 10;
constexpr int PANEL_Y = BOARD_Y;
constexpr int PANEL_H = ROWS * CELL;

// 7 tetrominoes x 4 rotations, 4x4 bitmask (bit 15 = row0,col0 ... bit0 = row3,col3).
static const uint16_t kShapes[7][4] = {
    { 0x0F00, 0x2222, 0x00F0, 0x4444 }, // I
    { 0x0660, 0x0660, 0x0660, 0x0660 }, // O
    { 0x0E40, 0x4C40, 0x4E00, 0x4640 }, // T
    { 0x06C0, 0x8C40, 0x6C00, 0x4620 }, // S
    { 0x0C60, 0x4C80, 0xC600, 0x2640 }, // Z
    { 0x44C0, 0x8E00, 0x6440, 0x0E20 }, // J
    { 0x4460, 0x0E80, 0xC440, 0x2E00 }, // L
};
static const uint16_t kColors[7] = {
    ST77XX_CYAN, ST77XX_YELLOW, ST77XX_MAGENTA, ST77XX_GREEN,
    ST77XX_RED, ST77XX_BLUE, ST77XX_ORANGE
};

static uint8_t s_board[ROWS][COLS]; // 0 = empty, else shape index+1
static int s_pieceShape, s_pieceRot, s_pieceX, s_pieceY;
static int s_nextShape;
static int s_score;
static bool s_paused, s_gameOver, s_titleScreen;
static unsigned long s_lastDropMs;

// Redraw gating: s_boardDirty -> full board+panel repaint (piece locked /
// lines cleared / entered game). s_pieceMoved -> erase+redraw just the
// falling piece's cells (cheap, happens almost every tick). s_panelDirty ->
// panel-only redraw (score/next-piece/pause banner).
static bool s_boardDirty = true;
static bool s_pieceMoved = false;
static bool s_panelDirty = true;
static int s_prevPieceShape = -1, s_prevPieceRot = 0, s_prevPieceX = 0, s_prevPieceY = 0;
static bool s_havePrevPiece = false;

static bool cellFilled(int shape, int rot, int row, int col) {
    uint16_t mask = kShapes[shape][rot];
    return (mask & (1 << (15 - (row * 4 + col)))) != 0;
}

static bool collides(int shape, int rot, int px, int py) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!cellFilled(shape, rot, r, c)) continue;
            int bx = px + c, by = py + r;
            if (bx < 0 || bx >= COLS || by >= ROWS) return true;
            if (by >= 0 && s_board[by][bx]) return true;
        }
    }
    return false;
}

static void spawnPiece() {
    s_pieceShape = s_nextShape;
    s_nextShape = random(0, 7);
    s_pieceRot = 0;
    s_pieceX = COLS / 2 - 2;
    s_pieceY = -1;
    s_havePrevPiece = false; // new piece: don't try to erase the old one's cells
    s_panelDirty = true;     // "Next" preview changed
    if (collides(s_pieceShape, s_pieceRot, s_pieceX, s_pieceY)) {
        s_gameOver = true;
        audio::playSfx(kSfxGameOver, 1);
    }
}

static void resetGame() {
    memset(s_board, 0, sizeof(s_board));
    s_score = 0;
    s_paused = false;
    s_gameOver = false;
    s_nextShape = random(0, 7);
    spawnPiece();
    s_lastDropMs = millis();
    s_boardDirty = true;
    s_panelDirty = true;
}

void enter() {
    s_titleScreen = true;
    s_boardDirty = true;
    randomSeed(micros());
}

// Backside LEDs (LED9-14) show live stack height: highest filled row -> 0-6 LEDs lit.
static void updateStackHeightLeds() {
    int highestRow = ROWS;
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (s_board[r][c]) { highestRow = r; goto found; }
        }
    }
found:
    int stackHeight = ROWS - highestRow; // 0..ROWS
    int litCount = (stackHeight * 6) / ROWS; // scale to 6 backside LEDs
    if (litCount > 6) litCount = 6;
    static const int backsideLeds[6] = { 9, 10, 11, 12, 13, 14 };
    for (int i = 0; i < 6; i++) {
        leds::setChainLed(backsideLeds[i], i < litCount);
    }
}

static void clearLines() {
    int cleared = 0;
    for (int r = ROWS - 1; r >= 0; r--) {
        bool full = true;
        for (int c = 0; c < COLS; c++) if (!s_board[r][c]) { full = false; break; }
        if (full) {
            cleared++;
            for (int rr = r; rr > 0; rr--) {
                memcpy(s_board[rr], s_board[rr - 1], sizeof(s_board[rr]));
            }
            memset(s_board[0], 0, sizeof(s_board[0]));
            r++; // re-check same row index after shift
        }
    }
    if (cleared > 0) {
        s_score += cleared * 100;
        s_panelDirty = true;
        audio::playSfx(kSfxLineClear, 1);
    }
}

static void lockPiece() {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!cellFilled(s_pieceShape, s_pieceRot, r, c)) continue;
            int bx = s_pieceX + c, by = s_pieceY + r;
            if (by >= 0 && by < ROWS && bx >= 0 && bx < COLS) {
                s_board[by][bx] = s_pieceShape + 1;
            }
        }
    }
    audio::playSfx(kSfxLock, 1);
    clearLines();
    updateStackHeightLeds();
    spawnPiece();
    s_boardDirty = true; // board contents changed: full repaint of board area
}

// Beveled block: filled color inset 1px with white border, or black+grid
// outline if empty.
static void drawBlock(Adafruit_ST7789& tft, int col, int row, int shapeIndexPlus1) {
    int x = BOARD_X + col * CELL;
    int y = BOARD_Y + row * CELL;
    if (shapeIndexPlus1 == 0) {
        tft.fillRect(x, y, CELL, CELL, ST77XX_BLACK);
        tft.drawRect(x, y, CELL, CELL, COLOR_GRID);
    } else {
        uint16_t color = kColors[shapeIndexPlus1 - 1];
        tft.fillRect(x + 1, y + 1, CELL - 2, CELL - 2, color);
        tft.drawRect(x, y, CELL, CELL, ST77XX_WHITE);
    }
}

static void drawPieceCells(Adafruit_ST7789& tft, int shape, int rot, int px, int py, bool erase) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!cellFilled(shape, rot, r, c)) continue;
            int by = py + r;
            if (by < 0) continue;
            drawBlock(tft, px + c, by, erase ? 0 : shape + 1);
        }
    }
}

static void drawBoardFull(Adafruit_ST7789& tft) {
    // Clears the gap strip between board and panel: the pause dialog paints
    // over it, and nothing else ever redraws it, leaving a tinted band otherwise.
    tft.fillRect(BOARD_X + COLS * CELL, 0, PANEL_X - (BOARD_X + COLS * CELL), cfg::DISPLAY_HEIGHT, ST77XX_BLACK);
    tft.drawRect(BOARD_X - 2, BOARD_Y - 2, COLS * CELL + 4, ROWS * CELL + 4, COLOR_BORDER);
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            drawBlock(tft, c, r, s_board[r][c]);
        }
    }
    drawPieceCells(tft, s_pieceShape, s_pieceRot, s_pieceX, s_pieceY, false);
    s_havePrevPiece = true;
    s_prevPieceShape = s_pieceShape;
    s_prevPieceRot = s_pieceRot;
    s_prevPieceX = s_pieceX;
    s_prevPieceY = s_pieceY;
}

static void drawPanel(Adafruit_ST7789& tft) {
    tft.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_PANEL_BG);
    tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);

    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(PANEL_X + 10, PANEL_Y + 8);
    tft.print("NEXT");

    int previewBoxX = PANEL_X + 10, previewBoxY = PANEL_Y + 32, previewBoxSize = 60;
    tft.drawRect(previewBoxX, previewBoxY, previewBoxSize, previewBoxSize, COLOR_BORDER);
    tft.fillRect(previewBoxX + 1, previewBoxY + 1, previewBoxSize - 2, previewBoxSize - 2, ST77XX_BLACK);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!cellFilled(s_nextShape, 0, r, c)) continue;
            int px = previewBoxX + 14 + c * 9;
            int py = previewBoxY + 14 + r * 9;
            tft.fillRect(px + 1, py + 1, 7, 7, kColors[s_nextShape]);
            tft.drawRect(px, py, 9, 9, ST77XX_WHITE);
        }
    }

    tft.setTextSize(1);
    tft.setCursor(PANEL_X + 10, previewBoxY + previewBoxSize + 16);
    tft.print("SCORE");
    tft.setTextSize(2);
    tft.setCursor(PANEL_X + 10, previewBoxY + previewBoxSize + 28);
    tft.print(s_score);
}

static void drawTitle() {
    auto& tft = display::tft();
    tft.fillScreen(ST77XX_BLACK);
    theme::drawCentered(tft, "TETRIS", 90, 4, ST77XX_CYAN);
    theme::drawCentered(tft, "Press START", 150, 2, ST77XX_WHITE);
}

static void drawGameOver() {
    games::drawGameOverDialog(display::tft(), s_score);
}

AppState frame() {
    if (input::wasPressed(input::Button::Back)) {
        // Must use led::stop() (LED manager), not the raw driver directly, or
        // the resting challenge-progress LED pattern never gets restored.
        led::stop();
        return AppState::GamesMenu;
    }

    if (s_titleScreen) {
        if (s_boardDirty) { drawTitle(); s_boardDirty = false; }
        if (input::wasPressed(input::Button::Start)) {
            display::tft().fillScreen(ST77XX_BLACK);
            resetGame();
            s_titleScreen = false;
        }
        return AppState::Tetris;
    }

    if (s_gameOver) {
        if (s_boardDirty) { drawGameOver(); s_boardDirty = false; }
        if (input::wasPressed(input::Button::Start)) {
            display::tft().fillScreen(ST77XX_BLACK);
            resetGame();
        }
        return AppState::Tetris;
    }

    if (input::wasPressed(input::Button::Pause)) {
        s_paused = !s_paused;
        s_panelDirty = true;
        // Pause dialog overlaps the board, so resuming needs a full board
        // repaint too, not just the panel.
        if (!s_paused) {
            s_boardDirty = true;
            // Reset here: elapsed time still accrues during pause, so without
            // this a long pause could drop/lock the piece instantly on resume.
            s_lastDropMs = millis();
        }
    }
    if (s_paused) {
        auto& tft = display::tft();
        if (s_boardDirty) { drawBoardFull(tft); s_boardDirty = false; }
        if (s_panelDirty) {
            drawPanel(tft);
            games::drawPauseDialog(tft); // drawn once per pause-toggle, on top of the frozen board
            s_panelDirty = false;
        }
        return AppState::Tetris;
    }

    if (input::wasPressed(input::Button::JoyLeft) && !collides(s_pieceShape, s_pieceRot, s_pieceX - 1, s_pieceY)) {
        s_pieceX--;
        s_pieceMoved = true;
        audio::playSfx(kSfxMove, 1);
    }
    if (input::wasPressed(input::Button::JoyRight) && !collides(s_pieceShape, s_pieceRot, s_pieceX + 1, s_pieceY)) {
        s_pieceX++;
        s_pieceMoved = true;
        audio::playSfx(kSfxMove, 1);
    }
    if (input::wasPressed(input::Button::Ok)) {
        int nextRot = (s_pieceRot + 1) % 4;
        if (!collides(s_pieceShape, nextRot, s_pieceX, s_pieceY)) {
            s_pieceRot = nextRot;
            s_pieceMoved = true;
            audio::playSfx(kSfxRotate, 1);
        }
    }

    bool softDrop = input::isDown(input::Button::JoyDown);
    unsigned long interval = softDrop ? SOFT_DROP_INTERVAL_MS : DROP_INTERVAL_MS;
    unsigned long now = millis();
    if (now - s_lastDropMs >= interval) {
        s_lastDropMs = now;
        if (!collides(s_pieceShape, s_pieceRot, s_pieceX, s_pieceY + 1)) {
            s_pieceY++;
            s_pieceMoved = true;
        } else {
            lockPiece(); // sets s_boardDirty; piece identity changes, so no delta-erase needed
            if (s_gameOver) {
                // Skip this tick's board redraw so s_boardDirty survives for
                // next frame()'s s_gameOver branch to draw "GAME OVER".
                return AppState::Tetris;
            }
        }
    }

    auto& tft = display::tft();
    if (s_boardDirty) {
        drawBoardFull(tft);
        s_boardDirty = false;
        s_pieceMoved = false;
    } else if (s_pieceMoved) {
        if (s_havePrevPiece) {
            drawPieceCells(tft, s_prevPieceShape, s_prevPieceRot, s_prevPieceX, s_prevPieceY, true);
        }
        drawPieceCells(tft, s_pieceShape, s_pieceRot, s_pieceX, s_pieceY, false);
        s_havePrevPiece = true;
        s_prevPieceShape = s_pieceShape;
        s_prevPieceRot = s_pieceRot;
        s_prevPieceX = s_pieceX;
        s_prevPieceY = s_pieceY;
        s_pieceMoved = false;
    }
    if (s_panelDirty) {
        drawPanel(tft);
        s_panelDirty = false;
    }

    return AppState::Tetris;
}

} // namespace games::tetris

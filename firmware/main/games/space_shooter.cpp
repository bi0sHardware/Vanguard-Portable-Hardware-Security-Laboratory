#include "space_shooter.h"
#include "game_ui.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../audio/audio_manager.h"
#include "../leds/led_manager.h"
#include "../ui/theme.h"
#include <Arduino.h>
#include <cstdio>

namespace games::space_shooter {

static const audio::Note kSfxShoot[]    = {{ 1600, 20 }};
static const audio::Note kSfxHit[]      = {{ 200, 150 }};
static const audio::Note kSfxLifeLost[] = {{ 150, 400 }};
static const audio::Note kSfxKill[]     = {{ 900, 30 }};

constexpr int SHIP_W = 16, SHIP_H = 12;
constexpr int SHIP_Y = 216;
constexpr int SHIP_SPEED = 4;
constexpr int MAX_BULLETS = 6;
constexpr int MAX_ENEMIES = 8;
constexpr unsigned long BULLET_INTERVAL_MS = 200;
constexpr unsigned long ENEMY_SPAWN_MS = 900;
constexpr unsigned long ENEMY_STEP_MS = 60;
constexpr unsigned long SHIP_STEP_MS = 16; // throttles movement to ~60 steps/sec; unthrottled it snapped to the edge instantly
constexpr int STAR_COUNT = 24;
constexpr int HUD_Y = 4;
constexpr int HUD_H = 12;

struct Bullet { int x, y; bool active; int prevX, prevY; bool prevActive; };
struct Enemy { int x, y; bool active; int prevX, prevY; bool prevActive; };
struct Star { int16_t x, y; uint8_t size; };

// Fixed starfield drawn once; a star under a sprite's black-fill erase rect
// is lost for the game, but a full-screen redraw every tick would flicker.
static Star s_stars[STAR_COUNT];

static int s_shipX;
static int s_prevShipX;
static bool s_shipDrawn;
static Bullet s_bullets[MAX_BULLETS];
static Enemy s_enemies[MAX_ENEMIES];
static int s_score, s_lives;
static int s_prevScore, s_prevLives;
static bool s_hudDirty;
static bool s_paused, s_gameOver, s_titleScreen;
static unsigned long s_lastBulletMs, s_lastEnemySpawnMs, s_lastEnemyStepMs, s_lastShipStepMs;
// Full redraw needed (title/game-over/start/pause-toggle); everything else
// here is delta-only.
static bool s_fullRedraw = true;

static void initStarfield() {
    for (auto& s : s_stars) {
        s.x = random(0, cfg::DISPLAY_WIDTH);
        s.y = random(0, SHIP_Y - 4);
        s.size = random(0, 3) == 0 ? 2 : 1;
    }
}

void enter() {
    s_titleScreen = true;
    s_fullRedraw = true;
    randomSeed(micros());
    initStarfield();
}

static void resetGame() {
    led::stop(); // clear any lingering life-lost Warning blink from a previous round
    s_shipX = cfg::DISPLAY_WIDTH / 2;
    s_shipDrawn = false;
    for (auto& b : s_bullets) { b.active = false; b.prevActive = false; }
    for (auto& e : s_enemies) { e.active = false; e.prevActive = false; }
    s_score = 0;
    s_lives = 3;
    s_prevScore = -1;
    s_prevLives = -1;
    s_hudDirty = true;
    s_paused = false;
    s_gameOver = false;
    s_lastBulletMs = s_lastEnemySpawnMs = s_lastEnemyStepMs = s_lastShipStepMs = millis();
    initStarfield();
}

static void fireBullet() {
    for (auto& b : s_bullets) {
        if (!b.active) {
            b.active = true;
            b.prevActive = false;
            b.x = s_shipX;
            b.y = SHIP_Y - 4;
            audio::playSfx(kSfxShoot, 1);
            return;
        }
    }
}

static void spawnEnemy() {
    for (auto& e : s_enemies) {
        if (!e.active) {
            e.active = true;
            e.prevActive = false;
            e.x = random(10, cfg::DISPLAY_WIDTH - 10);
            e.y = 10;
            return;
        }
    }
}

static void drawTitle() {
    auto& tft = display::tft();
    tft.fillScreen(ST77XX_BLACK);
    theme::drawCentered(tft, "SPACE SHOOTER", 80, 3, ST77XX_CYAN);
    theme::drawCentered(tft, "Press START", 140, 2, ST77XX_WHITE);
}

static void drawGameOver() {
    games::drawGameOverDialog(display::tft(), s_score);
}

static void drawFieldFull(Adafruit_ST7789& tft) {
    tft.fillScreen(ST77XX_BLACK);
    for (auto& s : s_stars) {
        if (s.size == 1) tft.drawPixel(s.x, s.y, 0x4A49);
        else tft.fillRect(s.x, s.y, 2, 2, 0x8C71);
    }
    for (auto& b : s_bullets) if (b.active) tft.fillRect(b.x - 1, b.y - 4, 2, 6, ST77XX_YELLOW);
    for (auto& e : s_enemies) {
        if (!e.active) continue;
        tft.fillRect(e.x - 6, e.y - 6, 12, 12, ST77XX_RED);
        tft.drawRect(e.x - 6, e.y - 6, 12, 12, ST77XX_WHITE);
    }
    s_shipDrawn = false; // force ship (re)draw below
}

static void eraseShip(Adafruit_ST7789& tft, int x) {
    // Must cover the engine flare (extends to SHIP_Y+SHIP_H+4) or a trailing
    // sliver is left behind as the ship moves.
    tft.fillRect(x - SHIP_W / 2 - 1, SHIP_Y - 3, SHIP_W + 2, SHIP_H + 8, ST77XX_BLACK);
}
static void drawShip(Adafruit_ST7789& tft, int x) {
    tft.fillTriangle(x, SHIP_Y - 2, x - SHIP_W / 2, SHIP_Y + SHIP_H, x + SHIP_W / 2, SHIP_Y + SHIP_H, ST77XX_GREEN);
    tft.fillTriangle(x, SHIP_Y + SHIP_H - 4, x - SHIP_W / 4, SHIP_Y + SHIP_H, x + SHIP_W / 4, SHIP_Y + SHIP_H, ST77XX_WHITE);
    tft.fillRect(x - 2, SHIP_Y + SHIP_H, 4, 4, ST77XX_ORANGE);
}

static void drawHud(Adafruit_ST7789& tft) {
    tft.fillRect(0, HUD_Y, cfg::DISPLAY_WIDTH, HUD_H, ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(4, HUD_Y);
    tft.print("Score: ");
    tft.print(s_score);
    tft.setCursor(cfg::DISPLAY_WIDTH - 60, HUD_Y);
    tft.print("Lives: ");
    tft.print(s_lives);
}

AppState frame() {
    if (input::wasPressed(input::Button::Back)) {
        led::stop();
        return AppState::GamesMenu;
    }

    if (s_titleScreen) {
        if (s_fullRedraw) { drawTitle(); s_fullRedraw = false; }
        if (input::wasPressed(input::Button::Start)) {
            resetGame();
            s_titleScreen = false;
            s_fullRedraw = true;
        }
        return AppState::SpaceShooter;
    }

    if (s_gameOver) {
        if (s_fullRedraw) { drawGameOver(); s_fullRedraw = false; }
        if (input::wasPressed(input::Button::Start)) { resetGame(); s_fullRedraw = true; }
        return AppState::SpaceShooter;
    }

    auto& tft = display::tft();

    if (input::wasPressed(input::Button::Pause)) {
        s_paused = !s_paused;
        s_fullRedraw = true; // repaint the field cleanly under/around the pause banner either way
        // Reset all four timers on resume: elapsed time accrues during pause,
        // and a stale s_lastEnemyStepMs could trigger a spurious hit/life
        // loss/spawn on the instant the player unpauses.
        if (!s_paused) {
            unsigned long now = millis();
            s_lastBulletMs = now;
            s_lastEnemySpawnMs = now;
            s_lastEnemyStepMs = now;
            s_lastShipStepMs = now;
        }
    }
    if (s_paused) {
        if (s_fullRedraw) {
            drawFieldFull(tft);
            drawShip(tft, s_shipX);
            s_shipDrawn = true;
            s_prevShipX = s_shipX;
            drawHud(tft);
            games::drawPauseDialog(tft);
            s_fullRedraw = false;
        }
        return AppState::SpaceShooter;
    }

    if (s_fullRedraw) {
        drawFieldFull(tft);
        drawHud(tft);
        s_prevScore = s_score;
        s_prevLives = s_lives;
        s_hudDirty = false;
        s_fullRedraw = false;
    }

    int prevShipX = s_shipX;
    unsigned long nowShip = millis();
    if (nowShip - s_lastShipStepMs >= SHIP_STEP_MS) {
        s_lastShipStepMs = nowShip;
        if (input::isDown(input::Button::JoyLeft)) {
            s_shipX -= SHIP_SPEED;
            if (s_shipX < SHIP_W / 2) s_shipX = SHIP_W / 2;
        }
        if (input::isDown(input::Button::JoyRight)) {
            s_shipX += SHIP_SPEED;
            if (s_shipX > cfg::DISPLAY_WIDTH - SHIP_W / 2) s_shipX = cfg::DISPLAY_WIDTH - SHIP_W / 2;
        }
    }
    if (!s_shipDrawn || s_shipX != prevShipX) {
        if (s_shipDrawn) eraseShip(tft, prevShipX);
        drawShip(tft, s_shipX);
        s_shipDrawn = true;
    }

    unsigned long now = millis();
    if (input::isDown(input::Button::Ok) && now - s_lastBulletMs >= BULLET_INTERVAL_MS) {
        s_lastBulletMs = now;
        fireBullet();
    }

    if (now - s_lastEnemySpawnMs >= ENEMY_SPAWN_MS) {
        s_lastEnemySpawnMs = now;
        spawnEnemy();
    }

    if (now - s_lastEnemyStepMs >= ENEMY_STEP_MS) {
        s_lastEnemyStepMs = now;

        for (auto& b : s_bullets) {
            b.prevX = b.x; b.prevY = b.y; b.prevActive = b.active;
            if (b.active) b.y -= 6;
            if (b.active && b.y < 0) b.active = false;
        }

        for (auto& e : s_enemies) {
            e.prevX = e.x; e.prevY = e.y; e.prevActive = e.active;
            if (!e.active) continue;
            e.y += 3;
            if (e.y > SHIP_Y) {
                e.active = false;
                s_lives--;
                s_hudDirty = true;
                if (s_lives <= 0) {
                    s_gameOver = true;
                    audio::playSfx(kSfxLifeLost, 1);
                    // FailurePulse self-terminates; Warning blinks until stop()
                    // and would flash through the whole game-over screen.
                    led::playEffect(led::EffectId::FailurePulse);
                } else {
                    audio::playSfx(kSfxHit, 1);
                    led::playEffect(led::EffectId::ChallengeComplete, led::EffectParams{0, 0, 70, 0});
                }
            }
        }

        for (auto& e : s_enemies) {
            if (!e.active) continue;
            for (auto& b : s_bullets) {
                if (!b.active) continue;
                if (abs(b.x - e.x) < 8 && abs(b.y - e.y) < 8) {
                    e.active = false;
                    b.active = false;
                    s_score += 20;
                    s_hudDirty = true;
                    audio::playSfx(kSfxKill, 1);
                    led::playEffect(led::EffectId::SuccessFlash);
                }
            }
        }

        // Delta redraw: erase moved/deactivated, draw newly active/moved.
        for (auto& b : s_bullets) {
            bool posChanged = b.prevX != b.x || b.prevY != b.y;
            if (b.prevActive && (!b.active || posChanged)) {
                tft.fillRect(b.prevX - 1, b.prevY - 4, 2, 6, ST77XX_BLACK);
            }
            if (b.active && (!b.prevActive || posChanged)) {
                tft.fillRect(b.x - 1, b.y - 4, 2, 6, ST77XX_YELLOW);
            }
        }
        for (auto& e : s_enemies) {
            bool posChanged = e.prevX != e.x || e.prevY != e.y;
            if (e.prevActive && (!e.active || posChanged)) {
                tft.fillRect(e.prevX - 7, e.prevY - 7, 14, 14, ST77XX_BLACK);
            }
            if (e.active && (!e.prevActive || posChanged)) {
                tft.fillRect(e.x - 6, e.y - 6, 12, 12, ST77XX_RED);
                tft.drawRect(e.x - 6, e.y - 6, 12, 12, ST77XX_WHITE);
            }
        }
    }

    if (s_gameOver) {
        s_fullRedraw = true; // next frame() call draws the game-over screen fresh
        return AppState::SpaceShooter;
    }

    if (s_hudDirty && (s_score != s_prevScore || s_lives != s_prevLives)) {
        drawHud(tft);
        s_prevScore = s_score;
        s_prevLives = s_lives;
        s_hudDirty = false;
    }

    return AppState::SpaceShooter;
}

} // namespace games::space_shooter

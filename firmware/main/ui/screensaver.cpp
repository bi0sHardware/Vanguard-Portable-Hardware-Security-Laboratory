#include "screensaver.h"
#include "theme.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../display/logo_data.h"
#include "../display/vanguard_logo_data.h"
#include "../input/input.h"
#include "../leds/led_manager.h"
#include <Arduino.h>

namespace ui::screensaver {

// VANGUARD page: Vanguard's wordmark is the main/larger element, InCTF's logo the smaller
// addition on top. Background is a dense high-contrast starfield local to this file, not
// ui::starfield:: (that's shared with the Main Menu's light theme). Black sky + bright stars
// also lets InCTF's logo (native black canvas, ~79% pure 0x0000) composite with zero seam.
//
// Both logos use the same masked/recolored/scaled blit (drawRGBBitmap() has no transparency or
// scaling): Vanguard's light mint canvas is dropped and wordmark recolored white; InCTF's logo
// already has a black canvas, so only its canvas is dropped, brand colours kept.
constexpr int16_t kVanguardDstH = 92;
constexpr int16_t kVanguardDstW = (int16_t)((int32_t)VANGUARD_LOGO_WIDTH * kVanguardDstH / VANGUARD_LOGO_HEIGHT);
constexpr uint8_t kVanguardLumaThreshold = 100; // below = wordmark (draw white), at/above = canvas (skip, transparent)

// InCTF's artwork bounding box, measured off the raw bitmap (not the full 320x240 canvas):
// a wide ~299x114 banner shape. Cropped before scaling so the copy isn't mostly empty margin.
constexpr int16_t kInctfSrcX = 10, kInctfSrcY = 47, kInctfSrcW = 299, kInctfSrcH = 114;
constexpr int16_t kInctfDstH = 46;
constexpr int16_t kInctfDstW = (int16_t)((int32_t)kInctfSrcW * kInctfDstH / kInctfSrcH);
constexpr uint8_t kInctfLumaThreshold = 25; // below = pure background (skip), at/above = real artwork (keep its own colour)

static void vanguardLogoRect(int16_t& x, int16_t& y, int16_t& w, int16_t& h) {
    w = kVanguardDstW;
    h = kVanguardDstH;
    x = (int16_t)((cfg::DISPLAY_WIDTH - kVanguardDstW) / 2);
    y = 96; // main element -- centered in the screen's mid-lower area
}

static void inctfLogoRect(int16_t& x, int16_t& y, int16_t& w, int16_t& h) {
    w = kInctfDstW;
    h = kInctfDstH;
    x = (int16_t)((cfg::DISPLAY_WIDTH - kInctfDstW) / 2);
    y = 24; // added element -- sits above the Vanguard wordmark, own clear band
}

static inline uint8_t luma565(uint16_t c) {
    uint8_t r5 = (c >> 11) & 0x1F, g6 = (c >> 5) & 0x3F, b5 = c & 0x1F;
    uint16_t r8 = (uint16_t)((r5 * 527 + 23) >> 6);
    uint16_t g8 = (uint16_t)((g6 * 259 + 33) >> 6);
    uint16_t b8 = (uint16_t)((b5 * 527 + 23) >> 6);
    return (uint8_t)((r8 * 77 + g8 * 151 + b8 * 28) >> 8);
}

// Nearest-neighbor scaled blit with the source canvas dropped (transparent), recolored white or
// left as-is. Called once per screensaver entry, not per-frame, so a per-pixel loop is fine.
static void drawMaskedScaled(Adafruit_ST7789& tft, const uint16_t* src, int16_t srcFullW,
                              int16_t srcX, int16_t srcY, int16_t srcW, int16_t srcH,
                              int16_t dstX, int16_t dstY, int16_t dstW, int16_t dstH,
                              uint8_t lumaThreshold, bool below, bool recolorWhite) {
    for (int16_t dy = 0; dy < dstH; dy++) {
        int sy = srcY + (int)((int32_t)dy * srcH / dstH);
        for (int16_t dx = 0; dx < dstW; dx++) {
            int sx = srcX + (int)((int32_t)dx * srcW / dstW);
            uint16_t c = src[sy * srcFullW + sx];
            uint8_t l = luma565(c);
            bool isArt = below ? (l < lumaThreshold) : (l >= lumaThreshold);
            if (isArt) tft.drawPixel(dstX + dx, dstY + dy, recolorWhite ? ST77XX_WHITE : c);
        }
    }
}

// Dedicated dense starfield, local to this screen (see file header). Bright dots on black.
struct Star { int16_t x, y; uint8_t size; uint8_t phase; };
constexpr int kStarCount = 130; // denser than the shared module's 68
static Star s_stars[kStarCount];
static bool s_starsInitialized = false;

// Kept-clear rects (the two logo boxes) so stars never draw over opaque logo art.
static bool inClearRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static void initStars() {
    if (s_starsInitialized) return;
    s_starsInitialized = true;
    int16_t vx, vy, vw, vh, ix, iy, iw, ih;
    vanguardLogoRect(vx, vy, vw, vh);
    inctfLogoRect(ix, iy, iw, ih);
    for (int i = 0; i < kStarCount; i++) {
        int16_t x, y;
        int guard = 0;
        do {
            x = (int16_t)random(1, cfg::DISPLAY_WIDTH - 1);
            y = (int16_t)random(1, cfg::DISPLAY_HEIGHT - 14); // stay clear of the hint row band
            guard++;
        } while (guard < 8 && (inClearRect(x, y, vx, vy, vw, vh) || inClearRect(x, y, ix, iy, iw, ih)));
        s_stars[i].x = x;
        s_stars[i].y = y;
        s_stars[i].size = (uint8_t)((random(0, 6) == 0) ? 2 : 1); // mostly bold single pixels, some 2x2 bright ones
        s_stars[i].phase = (uint8_t)random(0, 255);
    }
}

static void drawStarDot(Adafruit_ST7789& tft, const Star& s, uint16_t color) {
    if (s.size >= 2) tft.fillRect(s.x, s.y, 2, 2, color);
    else tft.drawPixel(s.x, s.y, color);
}

// Brightness tiers instead of flat white: uniform brightness reads as noise, varying reads as depth.
static void drawAllStars(Adafruit_ST7789& tft) {
    initStars();
    for (int i = 0; i < kStarCount; i++) {
        const Star& s = s_stars[i];
        uint16_t color = s.size >= 2 ? ST77XX_WHITE : theme::COLOR_ACCENT;
        drawStarDot(tft, s, color);
    }
}

// Shooting stars: same tapered-trail technique as ui::starfield (head/mid/tail, erase+redraw each
// tick), reimplemented locally since this screen's colors and keep-clear rects differ.
struct ShootingStar {
    bool active = false;
    float x = 0, y = 0, vx = 0, vy = 0;
    int life = 0;
    int16_t headX = -1, headY = -1, midX = -1, midY = -1, tailX = -1, tailY = -1;
};
static ShootingStar s_shooting;

static void resetShootingStar() {
    s_shooting.active = false;
    s_shooting.headX = s_shooting.midX = s_shooting.tailX = -1;
}

static bool inBounds(int16_t x, int16_t y) {
    return x >= 0 && y >= 0 && x < cfg::DISPLAY_WIDTH && y < cfg::DISPLAY_HEIGHT - 14; // stay clear of the hint band
}

static void eraseShootingDot(Adafruit_ST7789& tft, int16_t cx, int16_t cy,
                              int16_t vx, int16_t vy, int16_t vw, int16_t vh,
                              int16_t ix, int16_t iy, int16_t iw, int16_t ih) {
    if (cx < 0 || cy < 0) return;
    if (inClearRect(cx, cy, vx, vy, vw, vh) || inClearRect(cx, cy, ix, iy, iw, ih)) return;
    tft.fillRect((int16_t)(cx - 2), (int16_t)(cy - 2), 5, 5, ST77XX_BLACK);
}

static void drawShootingDot(Adafruit_ST7789& tft, int16_t cx, int16_t cy, int size, uint16_t color,
                             int16_t vx, int16_t vy, int16_t vw, int16_t vh,
                             int16_t ix, int16_t iy, int16_t iw, int16_t ih) {
    if (cx < 0 || cy < 0) return;
    if (inClearRect(cx, cy, vx, vy, vw, vh) || inClearRect(cx, cy, ix, iy, iw, ih)) return;
    tft.fillRect((int16_t)(cx - size / 2), (int16_t)(cy - size / 2), size, size, color);
}

static void shootingStarTick(Adafruit_ST7789& tft) {
    int16_t vx, vy, vw, vh, ix, iy, iw, ih;
    vanguardLogoRect(vx, vy, vw, vh);
    inctfLogoRect(ix, iy, iw, ih);

    if (!s_shooting.active) {
        // ~1-in-30 chance per twinkle tick (~150ms) -- averages one shooting star per 4-5s.
        if (random(0, 30) != 0) return;
        bool fromLeft = random(0, 2) == 0;
        s_shooting.x = (float)random(0, cfg::DISPLAY_WIDTH);
        s_shooting.y = 0;
        constexpr float kSpeed = 8.0f;
        s_shooting.vx = (fromLeft ? 1.0f : -1.0f) * kSpeed;
        s_shooting.vy = kSpeed * 0.5f;
        s_shooting.life = 18;
        s_shooting.headX = s_shooting.midX = s_shooting.tailX = -1;
        s_shooting.active = true;
        return;
    }

    eraseShootingDot(tft, s_shooting.headX, s_shooting.headY, vx, vy, vw, vh, ix, iy, iw, ih);
    eraseShootingDot(tft, s_shooting.midX, s_shooting.midY, vx, vy, vw, vh, ix, iy, iw, ih);
    eraseShootingDot(tft, s_shooting.tailX, s_shooting.tailY, vx, vy, vw, vh, ix, iy, iw, ih);

    s_shooting.x += s_shooting.vx;
    s_shooting.y += s_shooting.vy;
    s_shooting.life--;
    int16_t hx = (int16_t)s_shooting.x, hy = (int16_t)s_shooting.y;

    if (s_shooting.life <= 0 || !inBounds(hx, hy)) {
        s_shooting.active = false;
        s_shooting.headX = s_shooting.midX = s_shooting.tailX = -1;
        return;
    }

    s_shooting.tailX = s_shooting.midX; s_shooting.tailY = s_shooting.midY;
    s_shooting.midX = s_shooting.headX; s_shooting.midY = s_shooting.headY;
    s_shooting.headX = hx; s_shooting.headY = hy;

    drawShootingDot(tft, s_shooting.tailX, s_shooting.tailY, 2, theme::COLOR_ACCENT, vx, vy, vw, vh, ix, iy, iw, ih);
    drawShootingDot(tft, s_shooting.midX, s_shooting.midY, 3, ST77XX_WHITE, vx, vy, vw, vh, ix, iy, iw, ih);
    drawShootingDot(tft, s_shooting.headX, s_shooting.headY, 5, ST77XX_WHITE, vx, vy, vw, vh, ix, iy, iw, ih); // head is the biggest
}

constexpr unsigned long kTwinkleMs = 150UL;
static unsigned long s_twinkleAtMs = 0;

static void twinkleStars(Adafruit_ST7789& tft) {
    unsigned long now = millis();
    constexpr int kPerTick = 14;
    static int cursor = 0;
    for (int n = 0; n < kPerTick; n++) {
        int i = (cursor + n) % kStarCount;
        const Star& s = s_stars[i];
        unsigned long t = (now / 120 + s.phase) % 24;
        uint16_t base = s.size >= 2 ? ST77XX_WHITE : theme::COLOR_ACCENT;
        uint16_t color = (t < 3) ? theme::COLOR_ACCENT_DARK : base; // brief dim flicker, mostly lit
        drawStarDot(tft, s, color);
    }
    cursor = (cursor + kPerTick) % kStarCount;

    shootingStarTick(tft);
}

// MENU/PROFILE hints -- white-on-black, no highlight box.
constexpr int16_t kHintEdgeMargin = 6;
constexpr int16_t kHintY = (int16_t)(cfg::DISPLAY_HEIGHT - 12);

static void drawHintLabel(Adafruit_ST7789& tft, const char* label, int16_t cursorX, int16_t cursorY) {
    int16_t x1, y1;
    uint16_t w, h;
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    tft.getTextBounds(label, cursorX, cursorY, &x1, &y1, &w, &h);
    tft.fillRect(x1, y1, (int16_t)w, (int16_t)h, ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(cursorX, cursorY);
    tft.print(label);
}

static void drawHints() {
    auto& tft = display::tft();
    tft.setTextSize(theme::BODY_TEXT_SIZE);

    static const char* kMenuLabel = "< MENU (SELECT)";
    drawHintLabel(tft, kMenuLabel, kHintEdgeMargin, kHintY);

    static const char* kProfileLabel = "PROFILE (OK) >";
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(kProfileLabel, 0, 0, &x1, &y1, &w, &h);
    int16_t profileX = (int16_t)(cfg::DISPLAY_WIDTH - kHintEdgeMargin - (int16_t)w);
    drawHintLabel(tft, kProfileLabel, profileX, kHintY);
}

static void drawScene() {
    auto& tft = display::tft();
    tft.fillScreen(ST77XX_BLACK);
    resetShootingStar(); // in-flight trail's old position no longer matches the fresh screen
    drawAllStars(tft);

    int16_t ix, iy, iw, ih;
    inctfLogoRect(ix, iy, iw, ih);
    drawMaskedScaled(tft, LOGO_BITMAP, LOGO_WIDTH, kInctfSrcX, kInctfSrcY, kInctfSrcW, kInctfSrcH,
                     ix, iy, iw, ih, kInctfLumaThreshold, /*below=*/false, /*recolorWhite=*/false);

    int16_t vx, vy, vw, vh;
    vanguardLogoRect(vx, vy, vw, vh);
    drawMaskedScaled(tft, VANGUARD_LOGO_BITMAP, VANGUARD_LOGO_WIDTH, 0, 0, VANGUARD_LOGO_WIDTH, VANGUARD_LOGO_HEIGHT,
                     vx, vy, vw, vh, kVanguardLumaThreshold, /*below=*/true, /*recolorWhite=*/true);

    drawHints();
    s_twinkleAtMs = millis();
}

void enter() {
    drawScene();
    led::playEffect(led::EffectId::Sweep, led::EffectParams{0, 0, 0, led::kMaskAll});
}

static bool anyNavInput() {
    return input::wasPressed(input::Button::JoyUp) ||
           input::wasPressed(input::Button::JoyDown) ||
           input::wasPressed(input::Button::JoyLeft) ||
           input::wasPressed(input::Button::JoyRight) ||
           input::wasPressed(input::Button::JoySelect);
}

AppState frame() {
    if (millis() - s_twinkleAtMs >= kTwinkleMs) {
        s_twinkleAtMs = millis();
        twinkleStars(display::tft());
    }

    if (anyNavInput()) {
        led::stop();
        return AppState::MainMenu;
    }
    if (input::wasPressed(input::Button::Ok)) {
        led::stop();
        // main.cpp's enterState() calls settings::enter(true) for this state -- don't call it
        // here too, or it gets clobbered by enterState()'s own no-arg call a tick later.
        return AppState::ProfileSetup;
    }

    return AppState::Screensaver;
}

} // namespace ui::screensaver

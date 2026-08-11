#include "starfield.h"
#include "../../include/config.h"
#include "theme.h"
#include <Arduino.h>
#include <math.h>

namespace ui::starfield {

struct Star {
    int16_t x, y;
    uint8_t size;   // 0 = 1px faint, 1 = 1px bold, 2 = 2x2 bold
    uint8_t phase;  // twinkle offset so stars don't all blink in lockstep
};

// Same sky colour as the rest of the UI (theme::COLOR_BG), matching the Vanguard logo bitmap's
// own baked-in background for a seamless composite. Stars are dark-on-light, not bright-on-black.
constexpr uint16_t kColorBg    = theme::COLOR_BG;
constexpr uint16_t kColorFaint = theme::COLOR_ACCENT;      // dim/half-visible
constexpr uint16_t kColorStar  = theme::COLOR_ACCENT_DARK; // fully visible "black" star

// ---- Main field (whole-screen, only re-ticked with exclusive control) ----
constexpr int kStarCount = 68;
static Star s_stars[kStarCount];
static bool s_initialized = false;

void init() {
    if (s_initialized) return;
    s_initialized = true;
    for (int i = 0; i < kStarCount; i++) {
        s_stars[i].x = (int16_t)random(2, cfg::DISPLAY_WIDTH - 2);
        s_stars[i].y = (int16_t)random(2, cfg::DISPLAY_HEIGHT - 2);
        // Mostly bold stars -- faint ones mostly read as invisible against the sky colour.
        s_stars[i].size = (uint8_t)((random(0, 5) == 0) ? 2 : (random(0, 4) == 0 ? 0 : 1));
        s_stars[i].phase = (uint8_t)random(0, 255);
    }
}

static bool inRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
    if (rw <= 0 || rh <= 0) return false;
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static void drawStar(Adafruit_ST7789& tft, const Star& s, uint16_t color) {
    if (s.size >= 2) tft.fillRect(s.x, s.y, 3, 3, color); // bold stars, bumped up a touch from 2x2
    else tft.drawPixel(s.x, s.y, color);
}

void drawFull(Adafruit_ST7789& tft) {
    init();
    resetShootingStar();
    tft.fillScreen(kColorBg);
    for (int i = 0; i < kStarCount; i++) {
        uint16_t color = (s_stars[i].size == 0) ? kColorFaint : kColorStar;
        drawStar(tft, s_stars[i], color);
    }
}

static bool inBounds(int16_t x, int16_t y) {
    return x >= 0 && y >= 0 && x < cfg::DISPLAY_WIDTH && y < cfg::DISPLAY_HEIGHT;
}

// ---- Shooting stars ----
// Tapered comet trail: big head, medium mid, small dim tail -- three fixed slots shifted each
// tick, fully erased (generous 5x5) and redrawn fresh rather than tracking exact prior size.
static void eraseDot(Adafruit_ST7789& tft, int16_t cx, int16_t cy,
                      int16_t clearX, int16_t clearY, int16_t clearW, int16_t clearH) {
    if (cx < 0 || cy < 0) return; // slot not yet populated
    if (inRect(cx, cy, clearX, clearY, clearW, clearH)) return;
    tft.fillRect((int16_t)(cx - 2), (int16_t)(cy - 2), 5, 5, kColorBg);
}

static void drawDot(Adafruit_ST7789& tft, int16_t cx, int16_t cy, int size, uint16_t color,
                     int16_t clearX, int16_t clearY, int16_t clearW, int16_t clearH) {
    if (cx < 0 || cy < 0) return;
    if (inRect(cx, cy, clearX, clearY, clearW, clearH)) return;
    tft.fillRect((int16_t)(cx - size / 2), (int16_t)(cy - size / 2), size, size, color);
}

struct ShootingStar {
    bool active = false;
    float x = 0, y = 0, vx = 0, vy = 0;
    int life = 0;
    int16_t headX = -1, headY = -1, midX = -1, midY = -1, tailX = -1, tailY = -1;
};
static ShootingStar s_shooting;

void resetShootingStar() {
    s_shooting.active = false;
    s_shooting.headX = s_shooting.midX = s_shooting.tailX = -1;
}

static void shootingStarTick(Adafruit_ST7789& tft, int16_t clearX, int16_t clearY, int16_t clearW, int16_t clearH) {
    if (!s_shooting.active) {
        // ~1-in-25 chance per twinkleTick() call (~200ms) -- averages one shooting star per 5s.
        if (random(0, 25) != 0) return;
        bool fromLeft = random(0, 2) == 0;
        s_shooting.x = (float)random(0, cfg::DISPLAY_WIDTH);
        s_shooting.y = 0;
        constexpr float kSpeed = 7.0f;
        s_shooting.vx = (fromLeft ? 1.0f : -1.0f) * kSpeed;
        s_shooting.vy = kSpeed * 0.55f; // diagonal descent, not near-horizontal
        s_shooting.life = 16;
        s_shooting.headX = s_shooting.midX = s_shooting.tailX = -1;
        s_shooting.active = true;
        return;
    }

    eraseDot(tft, s_shooting.headX, s_shooting.headY, clearX, clearY, clearW, clearH);
    eraseDot(tft, s_shooting.midX, s_shooting.midY, clearX, clearY, clearW, clearH);
    eraseDot(tft, s_shooting.tailX, s_shooting.tailY, clearX, clearY, clearW, clearH);

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

    drawDot(tft, s_shooting.tailX, s_shooting.tailY, 2, kColorFaint, clearX, clearY, clearW, clearH);
    drawDot(tft, s_shooting.midX, s_shooting.midY, 3, kColorStar, clearX, clearY, clearW, clearH);
    drawDot(tft, s_shooting.headX, s_shooting.headY, 5, kColorStar, clearX, clearY, clearW, clearH); // head is the biggest
}

// ---- Belt shooting star ----
// Same tapered-trail approach, confined to the belt strip, diagonal travel bounded so it never
// drifts into the live menu content above.
struct BeltShootingStar {
    bool active = false;
    float x = 0, y = 0, vx = 0, vy = 0;
    int16_t headX = -1, headY = -1, midX = -1, midY = -1, tailX = -1, tailY = -1;
};
static BeltShootingStar s_beltShooting;

static void beltShootingStarTick(Adafruit_ST7789& tft, int16_t beltX, int16_t beltY, int16_t beltW, int16_t beltH,
                                  int16_t clearX, int16_t clearY, int16_t clearW, int16_t clearH) {
    if (!s_beltShooting.active) {
        // Rarer than the main field's -- the strip is small, too-frequent crossings look busy.
        if (random(0, 45) != 0) return;
        bool fromLeft = random(0, 2) == 0;
        bool downward = random(0, 2) == 0;
        s_beltShooting.x = fromLeft ? beltX : (beltX + beltW);
        s_beltShooting.y = (float)(downward ? beltY + 3 : beltY + beltH - 3);
        s_beltShooting.vx = (fromLeft ? 1.0f : -1.0f) * 8.0f;
        s_beltShooting.vy = (downward ? 1.0f : -1.0f) * 3.0f;
        s_beltShooting.headX = s_beltShooting.midX = s_beltShooting.tailX = -1;
        s_beltShooting.active = true;
        return;
    }

    eraseDot(tft, s_beltShooting.headX, s_beltShooting.headY, clearX, clearY, clearW, clearH);
    eraseDot(tft, s_beltShooting.midX, s_beltShooting.midY, clearX, clearY, clearW, clearH);
    eraseDot(tft, s_beltShooting.tailX, s_beltShooting.tailY, clearX, clearY, clearW, clearH);

    s_beltShooting.x += s_beltShooting.vx;
    s_beltShooting.y += s_beltShooting.vy;
    int16_t hx = (int16_t)s_beltShooting.x, hy = (int16_t)s_beltShooting.y;

    bool outOfBounds = hx < beltX - 4 || hx > beltX + beltW + 4 || hy < beltY || hy > beltY + beltH - 1;
    if (outOfBounds) {
        s_beltShooting.active = false;
        s_beltShooting.headX = s_beltShooting.midX = s_beltShooting.tailX = -1;
        return;
    }

    s_beltShooting.tailX = s_beltShooting.midX; s_beltShooting.tailY = s_beltShooting.midY;
    s_beltShooting.midX = s_beltShooting.headX; s_beltShooting.midY = s_beltShooting.headY;
    s_beltShooting.headX = hx; s_beltShooting.headY = hy;

    drawDot(tft, s_beltShooting.tailX, s_beltShooting.tailY, 2, kColorFaint, clearX, clearY, clearW, clearH);
    drawDot(tft, s_beltShooting.midX, s_beltShooting.midY, 3, kColorStar, clearX, clearY, clearW, clearH);
    drawDot(tft, s_beltShooting.headX, s_beltShooting.headY, 5, kColorStar, clearX, clearY, clearW, clearH);
}

void twinkleTick(Adafruit_ST7789& tft, int16_t clearX, int16_t clearY, int16_t clearW, int16_t clearH) {
    init();
    static int cursor = 0;
    constexpr int kPerTick = 10;
    unsigned long now = millis();
    for (int n = 0; n < kPerTick; n++) {
        int i = (cursor + n) % kStarCount;
        const Star& s = s_stars[i];
        if (inRect(s.x, s.y, clearX, clearY, clearW, clearH)) continue;
        // Triangle-wave flicker per star, toggling visible/blends-into-sky (twinkling on a light
        // background). Skewed toward visible so the field reads as consistently starry.
        unsigned long t = (now / 150 + s.phase) % 30;
        uint16_t color;
        if (s.size == 0) color = (t < 20) ? kColorFaint : kColorBg;
        else color = (t < 5) ? kColorFaint : kColorStar;
        drawStar(tft, s, color);
    }
    cursor = (cursor + kPerTick) % kStarCount;

    shootingStarTick(tft, clearX, clearY, clearW, clearH);
}

// ---- Belt (confined to a caller-guaranteed-safe rect, safe to animate
// continuously even on an interactive screen) ----
constexpr int kBeltStarCount = 25;
static Star s_belt[kBeltStarCount];
static bool s_beltInitialized = false;
static int16_t s_beltX, s_beltY, s_beltW, s_beltH;

static void initBelt(int16_t beltX, int16_t beltY, int16_t beltW, int16_t beltH) {
    if (s_beltInitialized && s_beltX == beltX && s_beltY == beltY && s_beltW == beltW && s_beltH == beltH) return;
    s_beltInitialized = true;
    s_beltX = beltX; s_beltY = beltY; s_beltW = beltW; s_beltH = beltH;
    for (int i = 0; i < kBeltStarCount; i++) {
        s_belt[i].x = (int16_t)(beltX + random(2, beltW - 2));
        s_belt[i].y = (int16_t)(beltY + random(2, beltH - 2));
        s_belt[i].size = (uint8_t)((random(0, 5) == 0) ? 2 : (random(0, 4) == 0 ? 0 : 1));
        s_belt[i].phase = (uint8_t)random(0, 255);
    }
}

// Ringed planet -- Saturn-style tilted elliptical ring, drawn in two passes (behind globe, then
// in front) so it reads as encircling rather than floating over it. Line-art primitives only,
// monochrome in the same dark tone as the stars.
constexpr uint16_t kRingColor = theme::COLOR_ACCENT_DARK;

static void ellipsePoint(int cx, int cy, float rx, float ry, float tiltRad, float angleDeg, int16_t& outX, int16_t& outY) {
    float t = angleDeg * (float)M_PI / 180.0f;
    float ex = rx * cosf(t), ey = ry * sinf(t);
    outX = (int16_t)(cx + ex * cosf(tiltRad) - ey * sinf(tiltRad));
    outY = (int16_t)(cy + ex * sinf(tiltRad) + ey * cosf(tiltRad));
}

static void drawRingArc(Adafruit_ST7789& tft, int cx, int cy, float rx, float ry, float tiltRad,
                         float startDeg, float endDeg, uint16_t color) {
    constexpr int kSteps = 16;
    int16_t prevX = 0, prevY = 0;
    for (int i = 0; i <= kSteps; i++) {
        float deg = startDeg + (endDeg - startDeg) * i / kSteps;
        int16_t x, y;
        ellipsePoint(cx, cy, rx, ry, tiltRad, deg, x, y);
        if (i > 0) {
            tft.drawLine(prevX, prevY, x, y, color);
            tft.drawLine(prevX, prevY + 1, x, y + 1, color); // 2px-thick stroke
        }
        prevX = x; prevY = y;
    }
}

// Planet/moon radius adapts to belt height (the Main Menu's belt shrinks as items are added) --
// a fixed radius clipped against menu rows/screen edge once the belt got thin.
constexpr int kPlanetRMax = 12, kPlanetRMin = 5;

static int planetRadiusForBeltH(int16_t beltH) {
    int r = beltH / 2 - 4;
    if (r > kPlanetRMax) r = kPlanetRMax;
    if (r < kPlanetRMin) r = kPlanetRMin;
    return r;
}

static void drawPlanet(Adafruit_ST7789& tft, int cx, int cy, int kR) {
    constexpr float kTilt = -0.34f; // ~-19 degrees
    float ringRx = kR * 1.8f, ringRy = ringRx * 0.34f;
    drawRingArc(tft, cx, cy, ringRx, ringRy, kTilt, 165, 375, kRingColor);  // back half, behind globe
    tft.drawCircle(cx, cy, kR, kRingColor);
    tft.drawCircle(cx, cy, kR - 1, kRingColor); // 2px-thick outline, matches the ring's stroke weight
    // Crater dots fixed relative to kR (not random) so they stay identical every redraw.
    int cr = kR > 6 ? 1 : 1; // dot radius stays 1px even when the globe shrinks -- any smaller isn't visible
    tft.fillCircle(cx - kR / 3, cy - kR / 2, cr, kRingColor);
    tft.fillCircle(cx + kR / 4, cy - kR / 6, cr, kRingColor);
    tft.fillCircle(cx - kR / 6, cy + kR / 2, cr, kRingColor);
    drawRingArc(tft, cx, cy, ringRx, ringRy, kTilt, -15, 165, kRingColor); // front half, over globe
}

// Small plain moon (no ring), radius always half the planet's, same scale-with-belt reasoning.
static void drawMoon(Adafruit_ST7789& tft, int cx, int cy, int kR) {
    tft.drawCircle(cx, cy, kR, kRingColor);
    if (kR > 2) tft.drawCircle(cx, cy, kR - 1, kRingColor); // 2px outline once there's room for one
    // Scatter of small craters rather than one dot, which read as a highlight/reflection.
    tft.fillCircle(cx - kR / 3, cy - kR / 3, 1, kRingColor);
    tft.fillCircle(cx + kR / 6, cy - kR / 6, 1, kRingColor);
    tft.fillCircle(cx - kR / 6, cy + kR / 3, 1, kRingColor);
    if (kR > 4) tft.drawPixel(cx + kR / 3, cy + kR / 6, kRingColor);
}

constexpr int16_t kMoonOffsetX = 34; // from beltX, left side -- planet owns the right side

void drawBelt(Adafruit_ST7789& tft, int16_t beltX, int16_t beltY, int16_t beltW, int16_t beltH) {
    initBelt(beltX, beltY, beltW, beltH);
    for (int i = 0; i < kBeltStarCount; i++) {
        uint16_t color = (s_belt[i].size == 0) ? kColorFaint : kColorStar;
        drawStar(tft, s_belt[i], color);
    }
    int planetR = planetRadiusForBeltH(beltH);
    int moonR = (planetR + 1) / 2;
    if (moonR < 3) moonR = 3;
    drawMoon(tft, beltX + kMoonOffsetX, beltY + beltH / 2, moonR);
    drawPlanet(tft, beltX + beltW - 34, beltY + beltH / 2, planetR);
}

void twinkleBelt(Adafruit_ST7789& tft, int16_t beltX, int16_t beltY, int16_t beltW, int16_t beltH) {
    initBelt(beltX, beltY, beltW, beltH);
    static int cursor = 0;
    constexpr int kPerTick = 6;
    unsigned long now = millis();
    int planetR = planetRadiusForBeltH(beltH);
    int moonR = (planetR + 1) / 2;
    if (moonR < 3) moonR = 3;
    // Planet's/moon's footprints -- never redraw stars under them. Sized proportionally to radius.
    int16_t pw = (int16_t)(planetR * 4), ph = (int16_t)(planetR * 3);
    int16_t mw = (int16_t)(moonR * 3), mh = mw;
    int16_t px = beltX + beltW - 34, py = beltY + beltH / 2;
    int16_t mx = beltX + kMoonOffsetX, my = beltY + beltH / 2;
    for (int n = 0; n < kPerTick; n++) {
        int i = (cursor + n) % kBeltStarCount;
        const Star& s = s_belt[i];
        if (inRect(s.x, s.y, (int16_t)(px - pw / 2), (int16_t)(py - ph / 2), pw, ph)) continue;
        if (inRect(s.x, s.y, (int16_t)(mx - mw / 2), (int16_t)(my - mh / 2), mw, mh)) continue;
        unsigned long t = (now / 150 + s.phase) % 30;
        uint16_t color;
        if (s.size == 0) color = (t < 20) ? kColorFaint : kColorBg;
        else color = (t < 5) ? kColorFaint : kColorStar;
        drawStar(tft, s, color);
    }
    cursor = (cursor + kPerTick) % kBeltStarCount;

    beltShootingStarTick(tft, beltX, beltY, beltW, beltH,
                          (int16_t)(px - pw / 2), (int16_t)(py - ph / 2), pw, ph);
}

} // namespace ui::starfield

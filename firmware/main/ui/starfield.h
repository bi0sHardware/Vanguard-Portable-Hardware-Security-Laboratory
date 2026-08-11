#pragma once
#include <Adafruit_ST7789.h>

// Shared animated starfield background for the idle screensaver and Main Menu. Uses theme::COLOR_BG
// as sky colour (matches the Vanguard logo bitmap's own baked-in background, no seam).
//
// Two independent star populations:
//  - Main field (drawFull()/twinkleTick()): whole-screen, only safe to keep animating on a screen
//    with exclusive pixel control (idle screensaver) — interactive content redrawing over it is fine
//    for a one-time paint but unsafe to re-tick continuously behind live content.
//  - "Belt" (drawBelt()/twinkleBelt()): smaller set confined to a caller rect guaranteed untouched
//    by anything else (e.g. strip below Main Menu's list) — safe to animate continuously even while
//    the rest of the screen is live.
namespace ui::starfield {

void init();

// Paints the full-screen sky+stars backdrop in one pass. Cheap; for one-time screen-enter paint, not per-tick.
void drawFull(Adafruit_ST7789& tft);

// Re-draws a handful of main-field stars with a new twinkle phase; call periodically (~150-250ms), only
// on a screen with exclusive display control. `clearX/Y/W/H` is a rect to skip (e.g. a logo); w=0 = none.
void twinkleTick(Adafruit_ST7789& tft, int16_t clearX = 0, int16_t clearY = 0,
                  int16_t clearW = 0, int16_t clearH = 0);

// Call after any full-screen redraw so an in-flight shooting star doesn't try to erase a stale pixel.
void resetShootingStar();

// Paints a denser belt of stars plus a ringed planet in the given rect. Call once on entering a
// screen with a static safe strip to decorate.
void drawBelt(Adafruit_ST7789& tft, int16_t beltX, int16_t beltY, int16_t beltW, int16_t beltH);

// Re-draws a few belt stars with a new twinkle phase; safe every tick as long as the rect is never
// drawn to by anything else.
void twinkleBelt(Adafruit_ST7789& tft, int16_t beltX, int16_t beltY, int16_t beltW, int16_t beltH);

} // namespace ui::starfield

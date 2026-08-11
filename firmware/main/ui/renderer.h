#pragma once
#include <cstdint>
#include <cstddef>
#include <Adafruit_ST7789.h>

// Shared rendering pipeline: screens declare persistent Regions; the Renderer dirty-checks and
// redraws only regions whose state changed, avoiding full-screen SPI fills on every keypress.
namespace ui {

struct Rect { int16_t x, y, w, h; };

constexpr size_t kRegionSnapshotSize = 16;

// One redrawable region. Screens own persistent storage (`static ui::Region s_regions[N]`) so the
// snapshot buffer survives between frame() calls.
struct Region {
    Rect bounds;

    // Draws into `bounds`; must not touch pixels outside it (Renderer clears exactly `bounds` first).
    void (*draw)(Adafruit_ST7789& tft, Rect bounds, const void* state);

    // True if `state` differs from the last-drawn snapshot (see regionChangedMemcmp for the common case).
    bool (*changed)(const void* prevSnapshot, const void* state);

    const void* state;   // pointer to the screen's own state (int/enum/small struct)
    size_t stateSize;     // sizeof(*state); must be <= kRegionSnapshotSize, 0 = always redraw when marked dirty externally

    uint16_t clearColor;  // background color used to clear `bounds` before draw()
    uint8_t snapshot[kRegionSnapshotSize];
    // No default initializer: C++11 aggregate-init requires all fields listed explicitly at each call site.
    bool everDrawn;
};

// Generic comparator for POD state — memcmp against the stored snapshot.
bool regionChangedMemcmp(const void* prevSnapshot, const void* state, size_t size);

// Compile-time guard: a screen's Region state type must fit in the snapshot buffer or dirty-diffing
// silently truncates. Add one line per state type next to each `static ui::Region s_xRegions[...]`.
#define UI_ASSERT_REGION_STATE_FITS(StateType) \
    static_assert(sizeof(StateType) <= ui::kRegionSnapshotSize, \
                  #StateType " is too large for a Region's snapshot buffer (kRegionSnapshotSize=16) — " \
                  "split this screen's state across multiple Regions instead of growing one")

// A Screen just declares its regions once; the Renderer owns diffing/redraw.
struct Screen {
    // Called once when this screen becomes active; nullptr if the caller handles enter-setup itself.
    void (*onEnter)();

    // Returns the screen's persistent Region array and writes count to *count. Array storage and
    // snapshot buffers must be static/persistent.
    Region* (*regions)(int* count);

    const char* title; // header text; nullptr = no header drawn
};

// Call once per main-loop tick for the active screen. First call after invalidate() forces a full
// redraw; subsequent calls redraw only regions whose state changed.
void frame(Screen& screen);

// Forces the next frame() to fully redraw. Every migrated screen's enter() must call this
// unconditionally — screens still on the old direct-draw path can clobber the display between
// two calls for the same Renderer screen, so re-entry must be explicit, not inferred.
void invalidate();

} // namespace ui

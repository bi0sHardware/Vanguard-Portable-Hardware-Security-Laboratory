#include "renderer.h"
#include "widgets.h"
#include "theme.h"
#include "../display/display.h"
#include "../perf/perf.h"
#include <cstring>

namespace ui {

bool regionChangedMemcmp(const void* prevSnapshot, const void* state, size_t size) {
    if (size == 0) return true; // no comparable state -> always treat as changed
    return memcmp(prevSnapshot, state, size) != 0;
}

static bool s_forceRedraw = true; // true so the very first frame() ever also does a full redraw

void invalidate() {
    s_forceRedraw = true;
}

void frame(Screen& screen) {
    PERF_SCOPE("ui::frame");
    auto& tft = display::tft();

    bool justEntered = s_forceRedraw;
    s_forceRedraw = false;
    if (justEntered) {
        // Full clear, not just this screen's regions — screens have differently-sized content and
        // a partial clear can leave the previous screen's larger content visible underneath.
        widgets::clearScreen(tft);
        if (screen.onEnter) screen.onEnter();
        if (screen.title) {
            PERF_SCOPE("ui::frame.header");
            // clearFirst=false: clearScreen() already painted this rect to COLOR_BG.
            widgets::header(tft, Rect{0, 0, (int16_t)widgets::screenWidth(), (int16_t)widgets::headerHeight()}, screen.title, false);
        }
    }

    int count = 0;
    Region* regions = screen.regions(&count);
    for (int i = 0; i < count; i++) {
        Region& r = regions[i];
        bool dirty = justEntered || !r.everDrawn ||
                     (r.changed ? r.changed(r.snapshot, r.state)
                                : regionChangedMemcmp(r.snapshot, r.state, r.stateSize));
        if (!dirty) continue;

        PERF_SCOPE("ui::frame.region");
        // On screen-enter, clearScreen() already painted every region to COLOR_BG; skip the
        // redundant fill unless this region's clear colour differs.
        bool needsClear = !(justEntered && r.clearColor == theme::COLOR_BG);
        if (needsClear) {
            tft.fillRect(r.bounds.x, r.bounds.y, r.bounds.w, r.bounds.h, r.clearColor);
        }
        r.draw(tft, r.bounds, r.state);
        if (r.stateSize > 0 && r.stateSize <= kRegionSnapshotSize) {
            memcpy(r.snapshot, r.state, r.stateSize);
        }
        r.everDrawn = true;
    }
}

} // namespace ui

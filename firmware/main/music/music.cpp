#include "music.h"
#include "tracks.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../audio/audio_manager.h"
#include "../leds/led_manager.h"
#include "../ui/theme.h"
#include <Arduino.h>
#include <math.h>

// Retro cassette-tape player UI. Playback engine/button mapping unchanged from the
// original list-based screen; this is a visual redesign only.
namespace music {

// Track-switch transition uses two brief static states (EJECTING/LOADING) instead of a
// continuous slide — no local framebuffer, so a pixel-by-pixel slide would flash on
// every repaint. Static holds avoid that and avoid residue at screen edges.
enum class Phase { Browse, Ejecting, Loading };
static Phase s_phase = Phase::Browse;
static unsigned long s_phaseStartMs = 0;
constexpr unsigned long kEjectHoldMs = 320;
constexpr unsigned long kLoadHoldMs = 320;

static int s_selected = 0;
static int s_pendingSelected = 0; // target track once Ejecting finishes
static bool s_playing = false;

// Reel rotation: fixed angle steps ticked independently of note timing, for a steady
// spin regardless of which notes/rests play.
static int s_reelFrame = 0;
static unsigned long s_reelLastMs = 0;
constexpr unsigned long kReelFrameMs = 90;
constexpr int kReelSteps = 16;

// Elapsed time recomputed from the note index reported by onNoteChange, summing the
// track's note table each callback (trivial work at this table size).
static unsigned long s_trackElapsedMs = 0;
static unsigned long s_trackTotalMs = 0;

static unsigned long sumDurations(const Note* notes, int count) {
    unsigned long total = 0;
    for (int i = 0; i < count; i++) total += notes[i].durationMs;
    return total;
}

static void onNoteChange(int index, uint16_t freqHz, void* ctx) {
    (void)ctx;
    const Track& t = kTracks[s_selected];
    unsigned long elapsed = 0;
    for (int i = 0; i < index && i < t.count; i++) elapsed += t.notes[i].durationMs;
    s_trackElapsedMs = elapsed;
    // Pitch-to-LED visualizer: bass<450Hz=red, mid 450-900Hz=green, treble>900Hz=white.
    led::playEffect(led::EffectId::PitchVisualizer, led::EffectParams{freqHz, 0, 0, 0});
}

// ---- Small SFX ----
static const audio::Note kSfxEject[]  = { { 900, 40 }, { 500, 60 } };

// ---- Layout ----
// kColorShell: cassette body fill color. Cutouts drawn on the shell (reel footprints,
// tape window, hub centers) must erase back to this, not theme::COLOR_BG.
constexpr uint16_t kColorShell = theme::COLOR_ACCENT;
constexpr uint16_t kColorShellLine = theme::COLOR_ACCENT_DARK;

constexpr int kShellX = 20, kShellY = 16, kShellW = 280, kShellH = 150;
constexpr int kLabelX = kShellX + 20, kLabelY = kShellY + 10, kLabelW = kShellW - 40, kLabelH = 30;
constexpr int kTabX = kShellX + 12, kTabY = kLabelY + kLabelH + 14, kTabSize = 14;
constexpr int kCounterRightX = kShellX + kShellW - 12;
constexpr int kReelY = kShellY + 104;
constexpr int kReelLeftX = kShellX + 70, kReelRightX = kShellX + kShellW - 70;
constexpr int kReelOuterR = 26, kReelHubR = 7;
constexpr int kStatusY = kShellY + kShellH + 8;
constexpr int kProgressTextY = kStatusY + 16;
constexpr int kProgressBarY = kProgressTextY + 16;
constexpr int kProgressBarH = 7;
constexpr int kFooterY = cfg::DISPLAY_HEIGHT - 12;

static void printClipped(Adafruit_ST7789& tft, int x, int y, const String& s, int maxWidth, int textSize) {
    tft.setTextWrap(false);
    tft.setTextSize(textSize);
    tft.setCursor(x, y);
    int maxChars = maxWidth / (6 * textSize);
    if (maxChars < 1) maxChars = 1;
    if ((int)s.length() > maxChars) {
        int keep = maxChars > 1 ? maxChars - 1 : 1;
        tft.print(s.substring(0, keep) + "\x7E");
    } else {
        tft.print(s);
    }
}

static void formatTime(unsigned long ms, char* out, size_t outLen) {
    unsigned long totalSec = ms / 1000;
    // Clamp to 99:59 so -Werror=format-truncation can't fire on the theoretical range.
    if (totalSec > 5999) totalSec = 5999;
    unsigned int m = (unsigned int)(totalSec / 60), s = (unsigned int)(totalSec % 60);
    snprintf(out, outLen, "%02u:%02u", m, s);
}

static void drawSpokeLines(Adafruit_ST7789& tft, int cx, int cy, int frame, uint16_t color) {
    float angleBase = (float)frame * (360.0f / kReelSteps) * (float)M_PI / 180.0f;
    for (int i = 0; i < 3; i++) {
        float a = angleBase + i * (2.0f * (float)M_PI / 3.0f);
        int x1 = cx + (int)(cosf(a) * (kReelHubR + 2));
        int y1 = cy + (int)(sinf(a) * (kReelHubR + 2));
        int x2 = cx + (int)(cosf(a) * (kReelOuterR - 3));
        int y2 = cy + (int)(sinf(a) * (kReelOuterR - 3));
        tft.drawLine(x1, y1, x2, y2, color);
        tft.fillCircle(x2, y2, 1, color); // rivet dot at spoke tip
    }
}

// Per-reel state: full body (rim/tape/hub) repaints only when wound-tape amount changes;
// spokes (which move every tick) redraw as 3 short lines instead of the whole disc.
static int s_lastTapeR[2] = { -1, -1 };
static int s_lastSpokeFrame[2] = { -1, -1 };

static void drawReel(Adafruit_ST7789& tft, int cx, int cy, bool isSupply, int reelIdx) {
    float progress = s_trackTotalMs > 0 ? (float)s_trackElapsedMs / (float)s_trackTotalMs : 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    // Supply reel (left) starts full and shrinks; takeup reel (right) starts empty and grows.
    float amt = isSupply ? (1.0f - progress) : progress;
    int tapeR = kReelHubR + 3 + (int)((kReelOuterR - kReelHubR - 3) * amt);

    if (tapeR != s_lastTapeR[reelIdx]) {
        tft.fillCircle(cx, cy, kReelOuterR, kColorShell); // erase to shell color, not page bg
        tft.drawCircle(cx, cy, kReelOuterR, kColorShellLine);
        tft.drawCircle(cx, cy, kReelOuterR - 1, kColorShellLine); // 2px rim
        tft.fillCircle(cx, cy, tapeR, theme::COLOR_TEXT); // wound tape
        tft.drawCircle(cx, cy, tapeR, kColorShell); // seam for wound-layers look
        // Gloss highlight dot; skipped once pack is too small to fit it.
        if (tapeR > kReelHubR + 5) {
            tft.fillCircle(cx - tapeR / 3, cy - tapeR / 3, 2, kColorShellLine);
        }
        tft.fillCircle(cx, cy, kReelHubR, kColorShell);
        tft.drawCircle(cx, cy, kReelHubR, kColorShellLine);
        tft.drawCircle(cx, cy, kReelHubR - 3, kColorShellLine); // spindle-hole ring
        s_lastTapeR[reelIdx] = tapeR;
        s_lastSpokeFrame[reelIdx] = -1; // body redraw already wiped any spokes
    }

    if (s_lastSpokeFrame[reelIdx] >= 0 && s_lastSpokeFrame[reelIdx] != s_reelFrame) {
        drawSpokeLines(tft, cx, cy, s_lastSpokeFrame[reelIdx], kColorShell); // erase old position
    }
    if (s_lastSpokeFrame[reelIdx] != s_reelFrame) {
        drawSpokeLines(tft, cx, cy, s_reelFrame, theme::COLOR_ACCENT_DARK);
        s_lastSpokeFrame[reelIdx] = s_reelFrame;
    }
}

static void drawTapeWindow(Adafruit_ST7789& tft) {
    int x1 = kReelLeftX + kReelOuterR + 4, x2 = kReelRightX - kReelOuterR - 4;
    int y = kReelY;
    tft.fillRect(x1, y - 5, x2 - x1, 10, theme::COLOR_TEXT); // dark recessed tape path
    // Ticks march with s_reelFrame to simulate tape moving, independent of reel rotation.
    int offset = s_reelFrame % 8; // ticks are 8px apart
    for (int x = x1 + offset; x < x2; x += 8) {
        tft.drawFastVLine(x, y - 2, 4, kColorShell);
    }
}

// Tape counter: elapsed play seconds wrapped to 3 digits (mimics a mechanical counter).
static void drawCounter(Adafruit_ST7789& tft, int cy) {
    char buf[8];
    unsigned int val = (unsigned int)((s_trackElapsedMs / 1000) % 1000);
    snprintf(buf, sizeof(buf), "%03u", val);
    int textW = 6 * 3; // 3 chars, size 1
    tft.fillRect(kCounterRightX - textW - 1, cy - 4, textW + 2, 10, kColorShell); // clear stale digits before redraw
    tft.setTextColor(kColorShellLine);
    tft.setTextSize(1);
    tft.setCursor(kCounterRightX - textW, cy - 3);
    tft.print(buf);
}

static void drawDynamicArea(Adafruit_ST7789& tft) {
    drawReel(tft, kReelLeftX, kReelY, true, 0);
    drawReel(tft, kReelRightX, kReelY, false, 1);
    drawTapeWindow(tft);
    drawCounter(tft, kReelY);
}

static void drawLabelPlate(Adafruit_ST7789& tft, int labelY, int trackIdx) {
    tft.fillRect(kLabelX, labelY, kLabelW, kLabelH, theme::COLOR_BG);
    tft.drawRect(kLabelX, labelY, kLabelW, kLabelH, kColorShellLine);
    tft.setTextColor(theme::COLOR_TEXT);
    printClipped(tft, kLabelX + 8, labelY + 6, kTracks[trackIdx].name, kLabelW - 16, 1);
    char sub[16];
    snprintf(sub, sizeof(sub), "TRACK %02u", (unsigned)((trackIdx + 1) % 100));
    tft.setTextColor(kColorShellLine);
    printClipped(tft, kLabelX + 8, labelY + 19, sub, kLabelW - 16, 1);
}

// Side tab ("A"/"B"): alternates by track index, purely for cassette flavor.
static void drawSideTab(Adafruit_ST7789& tft, int tabY, int trackIdx) {
    tft.fillRect(kTabX, tabY, kTabSize, kTabSize, theme::COLOR_BG);
    tft.drawRect(kTabX, tabY, kTabSize, kTabSize, kColorShellLine);
    tft.setTextColor(theme::COLOR_TEXT);
    tft.setTextSize(1);
    tft.setCursor(kTabX + 4, tabY + 3);
    tft.print((trackIdx % 2 == 0) ? "A" : "B");
}

// Corner screws: decorative ringed dots inset from each corner.
static void drawCornerScrews(Adafruit_ST7789& tft) {
    const int inset = 10, r = 2;
    const int xs[2] = { kShellX + inset, kShellX + kShellW - inset };
    const int ys[2] = { kShellY + inset, kShellY + kShellH - inset };
    for (int yi = 0; yi < 2; yi++) {
        for (int xi = 0; xi < 2; xi++) {
            tft.fillCircle(xs[xi], ys[yi], r, theme::COLOR_BG);
            tft.drawCircle(xs[xi], ys[yi], r, kColorShellLine);
        }
    }
}

static void drawShellChrome(Adafruit_ST7789& tft) {
    tft.fillRoundRect(kShellX, kShellY, kShellW, kShellH, 8, kColorShell);
    tft.drawRoundRect(kShellX, kShellY, kShellW, kShellH, 8, kColorShellLine);
    tft.drawRoundRect(kShellX + 2, kShellY + 2, kShellW - 4, kShellH - 4, 6, kColorShellLine);

    drawCornerScrews(tft);
    drawLabelPlate(tft, kLabelY, s_selected);
    drawSideTab(tft, kTabY, s_selected);
    drawCounter(tft, kReelY);
    drawDynamicArea(tft);
}

static void drawStatusAndProgress(Adafruit_ST7789& tft) {
    tft.fillRect(0, kStatusY - 2, cfg::DISPLAY_WIDTH, (kProgressBarY + kProgressBarH + 4) - (kStatusY - 2), theme::COLOR_BG);

    // Icon: filled triangle for play, two bars for stopped (drawn, not text glyphs).
    bool paused = s_playing && audio::isPaused();
    int iconCx = cfg::DISPLAY_WIDTH / 2 - 46, iconCy = kStatusY + 5;
    if (s_playing && !paused) {
        tft.fillTriangle(iconCx - 4, iconCy - 5, iconCx - 4, iconCy + 5, iconCx + 5, iconCy, theme::COLOR_ACCENT_DARK);
    } else {
        tft.fillRect(iconCx - 4, iconCy - 5, 3, 10, theme::COLOR_ACCENT_DARK);
        tft.fillRect(iconCx + 2, iconCy - 5, 3, 10, theme::COLOR_ACCENT_DARK);
    }
    tft.setTextColor(theme::COLOR_ACCENT_DARK);
    tft.setTextSize(1);
    tft.setCursor(iconCx + 14, iconCy - 4);
    tft.print(paused ? "PAUSED" : (s_playing ? "PLAYING" : "STOPPED"));

    char elapsed[8], total[8], line[24];
    formatTime(s_trackElapsedMs, elapsed, sizeof(elapsed));
    formatTime(s_trackTotalMs, total, sizeof(total));
    snprintf(line, sizeof(line), "%s / %s", elapsed, total);
    theme::drawCentered(tft, line, kProgressTextY, 1, theme::COLOR_TEXT);

    int barX = kShellX, barW = kShellW;
    tft.drawRect(barX, kProgressBarY, barW, kProgressBarH, theme::COLOR_ACCENT_DARK);
    float progress = s_trackTotalMs > 0 ? (float)s_trackElapsedMs / (float)s_trackTotalMs : 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    int fillW = (int)((barW - 2) * progress);
    if (fillW > 0) tft.fillRect(barX + 1, kProgressBarY + 1, fillW, kProgressBarH - 2, theme::COLOR_SUCCESS);
}

static void drawFooter(Adafruit_ST7789& tft) {
    tft.fillRect(0, kFooterY - 2, cfg::DISPLAY_WIDTH, 12, theme::COLOR_BG);
    const char* hint = s_playing
        ? (audio::isPaused() ? "PAUSE=Resume  OK=Stop  BACK=Exit" : "PAUSE=Pause  OK=Stop  BACK=Exit")
        : "OK=Play  UP/DN=Track  BACK=Exit";
    theme::drawCentered(tft, hint, kFooterY, 1, theme::COLOR_ACCENT_DARK);
}

static void drawHeader(Adafruit_ST7789& tft) {
    theme::drawCentered(tft, "MUSIC PLAYER", 4, 1, theme::COLOR_ACCENT_DARK);
}

// Replaces the status/progress row with a centered status line during a track-switch
// transition; reuses the normal status row so no new screen area is needed.
static void drawTransitionStatus(Adafruit_ST7789& tft, const char* msg) {
    tft.fillRect(0, kStatusY - 2, cfg::DISPLAY_WIDTH, (kProgressBarY + kProgressBarH + 4) - (kStatusY - 2), theme::COLOR_BG);
    theme::drawCentered(tft, msg, kStatusY + 2, 1, theme::COLOR_ACCENT_DARK);
}

// Targeted update, not a full-screen redraw: only the label plate and status line
// actually change between transition states; the rest is pixel-identical.
static void drawTransitionState(Adafruit_ST7789& tft, int trackIdx, const char* msg) {
    drawLabelPlate(tft, kLabelY, trackIdx);
    drawSideTab(tft, kTabY, trackIdx);
    drawTransitionStatus(tft, msg);
}

static void beginEjecting() {
    s_phase = Phase::Ejecting;
    s_phaseStartMs = millis();
    audio::playSfx(kSfxEject, 2);
    drawTransitionState(display::tft(), s_selected, "EJECTING...");
}

void enter() {
    s_selected = 0;
    s_pendingSelected = 0;
    s_playing = false;
    s_phase = Phase::Browse;
    s_reelFrame = 0;
    s_reelLastMs = millis();
    s_trackElapsedMs = 0;
    s_trackTotalMs = sumDurations(kTracks[s_selected].notes, kTracks[s_selected].count);
    s_lastTapeR[0] = s_lastTapeR[1] = -1;
    s_lastSpokeFrame[0] = s_lastSpokeFrame[1] = -1;
    auto& tft = display::tft();
    tft.fillScreen(theme::COLOR_BG); // full clear so no previous-screen margin residue remains
    drawHeader(tft);
    drawShellChrome(tft);
    drawStatusAndProgress(tft);
    drawFooter(tft);
}

static void stopPlayback() {
    s_playing = false;
    audio::stop();
    led::stop();
}

AppState frame() {
    auto& tft = display::tft();
    unsigned long now = millis();

    if (s_phase == Phase::Ejecting) {
        // Nothing redraws while holding — drawTransitionState() already ran in beginEjecting().
        if (now - s_phaseStartMs >= kEjectHoldMs) {
            s_selected = s_pendingSelected;
            s_trackElapsedMs = 0;
            s_trackTotalMs = sumDurations(kTracks[s_selected].notes, kTracks[s_selected].count);
            s_phase = Phase::Loading;
            s_phaseStartMs = now;
            drawTransitionState(tft, s_selected, "LOADING...");
            drawDynamicArea(tft); // reels must reflect new track's reset progress
        }
        return AppState::MusicPlayer;
    }
    if (s_phase == Phase::Loading) {
        if (now - s_phaseStartMs >= kLoadHoldMs) {
            s_phase = Phase::Browse;
            drawStatusAndProgress(tft); // only status/footer need restoring
            drawFooter(tft);
        }
        return AppState::MusicPlayer;
    }

    // Phase::Browse -- normal interactive state.
    if (input::wasPressed(input::Button::Back)) {
        stopPlayback();
        return AppState::MainMenu;
    }

    if (!s_playing) {
        int newSelected = s_selected;
        if (input::wasPressed(input::Button::JoyUp)) {
            newSelected = (s_selected - 1 + kTrackCount) % kTrackCount;
        }
        if (input::wasPressed(input::Button::JoyDown)) {
            newSelected = (s_selected + 1) % kTrackCount;
        }
        if (newSelected != s_selected) {
            // No click SFX here: beginEjecting() immediately plays kSfxEject, which would
            // instantly replace it (audio:: has one active-note slot).
            s_pendingSelected = newSelected;
            beginEjecting();
            return AppState::MusicPlayer;
        }
    }

    // Real pause/resume, separate from OK/JoySelect's full stop+restart.
    if (s_playing && input::wasPressed(input::Button::Pause)) {
        if (audio::isPaused()) {
            audio::resume();
            s_reelLastMs = now; // avoid reel jumping forward using stale elapsed time
        } else {
            audio::pause();
        }
        drawStatusAndProgress(tft);
        drawFooter(tft);
    }

    bool confirm = input::wasPressed(input::Button::Ok) ||
                   input::wasPressed(input::Button::JoySelect);
    if (confirm) {
        if (s_playing) {
            stopPlayback();
        } else {
            const Track& t = kTracks[s_selected];
            s_playing = true;
            s_trackElapsedMs = 0;
            audio::playMelody(t.notes, (uint16_t)t.count, /*loop=*/false, onNoteChange, nullptr);
        }
        drawStatusAndProgress(tft);
        drawFooter(tft);
    }

    // audio::playMelody(loop=false) stops itself at the last note; detect that to reset
    // this screen's playing state and clear the LED visualizer.
    if (s_playing && !audio::isPlaying()) {
        s_playing = false;
        led::stop();
        drawStatusAndProgress(tft);
        drawFooter(tft);
    }

    if (s_playing && !audio::isPaused()) {
        if (now - s_reelLastMs >= kReelFrameMs) {
            s_reelLastMs = now;
            s_reelFrame = (s_reelFrame + 1) % kReelSteps;
            drawDynamicArea(tft);
            drawStatusAndProgress(tft); // progress text/bar tick alongside the reels
        }
    }

    return AppState::MusicPlayer;
}

} // namespace music

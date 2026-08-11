#include "boot.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../display/logo_data.h"
#include "../display/boot_anim_data.h"
#include "../leds/led_manager.h"
#include "../audio/audio_manager.h"
#include <Arduino.h>
#include <cstdio>
#include <jpeg_decoder.h>

namespace boot {

enum class Phase { Logo };
static Phase s_phase = Phase::Logo;
static unsigned long s_phaseStartMs = 0;

// Ascending motif timed to land as the logo finishes revealing (~2s in); non-blocking, never adds to boot timing.
static const audio::Note kBootChime[] = {
    { 784,  90 },  // G5
    { 988,  90 },  // B5
    { 1175, 90 },  // D6
    { 1568, 160 }, // G6
    { 0,    40 },
    { 1568, 90 },
    { 2093, 260 }, // C7, held
};
constexpr uint16_t kBootChimeCount = sizeof(kBootChime) / sizeof(kBootChime[0]);

// Impact sting at kRevealAtMs so the reveal (previously LED-only) has a matching audio hit; lands an octave above the intro chime's final note.
static const audio::Note kRevealChime[] = {
    { 2093, 60 }, // C7
    { 2637, 160 }, // E7, held
};
constexpr uint16_t kRevealChimeCount = sizeof(kRevealChime) / sizeof(kRevealChime[0]);

// Descending flourish before handoff, so the tail of the animation isn't total silence; resolves just before the cut.
static const audio::Note kOutroChime[] = {
    { 1568, 90 },  // G6
    { 1175, 90 },  // D6
    { 784,  200 }, // G5, held -- settles back down to the intro's opening note
};
constexpr uint16_t kOutroChimeCount = sizeof(kOutroChime) / sizeof(kOutroChime[0]);

// One reused 320x240 RGB565 buffer (150KB), not one per frame; freed in leaveLogoAnim() before WiFi/BLE need the heap.
static uint16_t* s_animBuf = nullptr;
static int s_animLastDrawnFrame = -1;

// Playback compressed into kAnimTotalMs (7s) vs source's native ~10s -- all 80 frames still play, just faster.
constexpr unsigned long kSourceDurationMs = 10005; // source mp4 actual length (ffprobe), for rescaling below
constexpr unsigned long kAnimTotalMs = 7000;
constexpr unsigned long kAnimFrameMs = kAnimTotalMs / (unsigned long)BOOT_ANIM_FRAME_COUNT;

// Reveal/outro timestamps measured from source video luminance: fade-in plateaus ~2.8s, fade-to-black starts ~8.9s.
// Scaled by kAnimTotalMs/kSourceDurationMs to land on the same visual moments under compressed playback.
// Re-measure base timestamps if gen_boot_anim.py is re-run against a different source video.
constexpr unsigned long kRevealAtMs = 2800UL * kAnimTotalMs / kSourceDurationMs;
constexpr unsigned long kOutroAtMs = 8875UL * kAnimTotalMs / kSourceDurationMs;
constexpr unsigned long kRevealPulseMs = 500;
static bool s_animRevealFired = false;
static bool s_animBreathingStarted = false;
static bool s_animOutroFired = false;

static void drawAnimFrame(int index) {
    if (!s_animBuf) return;
    const BootAnimFrame& f = BOOT_ANIM_FRAMES[index];

    esp_jpeg_image_cfg_t jpegCfg = {};
    jpegCfg.indata = const_cast<uint8_t*>(f.data);
    jpegCfg.indata_size = f.len;
    jpegCfg.outbuf = reinterpret_cast<uint8_t*>(s_animBuf);
    jpegCfg.outbuf_size = (size_t)cfg::DISPLAY_WIDTH * cfg::DISPLAY_HEIGHT * 2;
    jpegCfg.out_format = JPEG_IMAGE_FORMAT_RGB565;
    jpegCfg.out_scale = JPEG_IMAGE_SCALE_0;
    jpegCfg.flags.swap_color_bytes = 0;

    esp_jpeg_image_output_t out = {};
    if (esp_jpeg_decode(&jpegCfg, &out) != ESP_OK) return;

    if (index == 0 && Serial) {
        Serial.printf("[BOOTANIM] decoded frame0: %ux%u (expected %dx%d)\n",
                      out.width, out.height, cfg::DISPLAY_WIDTH, cfg::DISPLAY_HEIGHT);
    }

    // Blit decoder-reported dims, not nominal 320x240 -- MCU-boundary padding can shift them, and wrong stride skews the image.
    display::tft().drawRGBBitmap(0, 0, s_animBuf, out.width, out.height);
}

static void leaveLogoAnim() {
    free(s_animBuf);
    s_animBuf = nullptr;
}

static void enterLogo() {
    s_phase = Phase::Logo;
    s_phaseStartMs = millis();
    s_animLastDrawnFrame = -1;
    s_animRevealFired = false;
    s_animBreathingStarted = false;
    s_animOutroFired = false;
    s_animBuf = (uint16_t*)malloc((size_t)cfg::DISPLAY_WIDTH * cfg::DISPLAY_HEIGHT * 2);
    if (s_animBuf && BOOT_ANIM_FRAME_COUNT > 0) {
        drawAnimFrame(0);
        s_animLastDrawnFrame = 0;
    } else {
        // Allocation failed -- fall back to the static bitmap rather than a blank screen.
        display::tft().drawRGBBitmap(0, 0, LOGO_BITMAP, LOGO_WIDTH, LOGO_HEIGHT);
    }
    // LED sweep and chime start now, alongside the animation, both non-blocking.
    led::playEffect(led::EffectId::BootSweep);
    audio::playMelody(kBootChime, kBootChimeCount, /*loop=*/false);
}

void enter() {
    enterLogo();
}

// Cuts straight to the Screensaver (composites Vanguard + InCTF logos) -- no separate splash screen needed.
AppState frame() {
    unsigned long elapsed = millis() - s_phaseStartMs;
    switch (s_phase) {
        case Phase::Logo: {
            if (!s_animRevealFired && elapsed >= kRevealAtMs) {
                s_animRevealFired = true;
                led::playEffect(led::EffectId::ConnectionPulse, led::EffectParams{0, 0, 0, led::kMaskAll});
                audio::playMelody(kRevealChime, kRevealChimeCount, /*loop=*/false);
            }
            if (s_animRevealFired && !s_animBreathingStarted && elapsed >= kRevealAtMs + kRevealPulseMs) {
                s_animBreathingStarted = true;
                led::playEffect(led::EffectId::Breathing, led::EffectParams{0, 0, 0, led::kMaskAll});
            }
            if (!s_animOutroFired && elapsed >= kOutroAtMs) {
                s_animOutroFired = true;
                audio::playMelody(kOutroChime, kOutroChimeCount, /*loop=*/false);
            }
            if (s_animBuf) {
                int target = (int)(elapsed / kAnimFrameMs);
                if (target >= BOOT_ANIM_FRAME_COUNT) target = BOOT_ANIM_FRAME_COUNT - 1;
                if (target != s_animLastDrawnFrame) {
                    s_animLastDrawnFrame = target;
                    drawAnimFrame(target);
                }
            }
            if (elapsed >= kAnimTotalMs) {
                leaveLogoAnim();
                led::stop();
                return AppState::Screensaver;
            }
            return AppState::Boot;
        }
        default:
            return AppState::Screensaver;
    }
}

} // namespace boot

#pragma once
#include <cstdint>
#include "renderer.h"

// Non-blocking tween/scheduler for menu/dialog/game animations; millis()-driven, never calls delay().
// ST7789 has no alpha blending: startFade() does a stepped color-lerp, not a real brightness fade —
// backlight has no GPIO control (K/A wired straight to GND/3.3V, see pins.h TFT_BL).
namespace anim {

using AnimId = uint8_t;
constexpr AnimId kInvalidAnim = 0xFF;

AnimId startFade(uint16_t fromColor, uint16_t toColor, uint16_t durationMs,
                  void (*onStep)(uint16_t color, void* ctx), void* ctx);

AnimId startSlide(ui::Rect from, ui::Rect to, uint16_t durationMs,
                   void (*onStep)(ui::Rect r, void* ctx), void* ctx);

AnimId startBlink(uint16_t periodMs, void (*onToggle)(bool on, void* ctx), void* ctx);

// value ramps min -> max -> min repeatedly every periodMs, until cancelled.
AnimId startPulse(uint8_t minVal, uint8_t maxVal, uint16_t periodMs,
                   void (*onStep)(uint8_t val, void* ctx), void* ctx);

// One-shot integer count-up (e.g. score reveal), min -> max over durationMs,
// then done.
AnimId startCountUp(int32_t fromVal, int32_t toVal, uint16_t durationMs,
                     void (*onStep)(int32_t val, void* ctx), void* ctx);

bool isDone(AnimId id);
void cancel(AnimId id);

// Call once per main loop tick. Advances all active animations via millis(),
// invokes their callbacks. Never blocks.
void update();

} // namespace anim

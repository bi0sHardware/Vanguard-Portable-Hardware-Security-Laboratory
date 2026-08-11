#include "animation.h"
#include <Arduino.h>

namespace anim {

enum class Kind : uint8_t { None, Fade, Slide, Blink, Pulse, CountUp };

struct Slot {
    bool active = false;
    Kind kind = Kind::None;
    unsigned long startMs = 0;
    uint16_t durationMs = 0; // 0 = continuous (Blink/Pulse use periodMs instead)
    uint16_t periodMs = 0;

    // Fade
    uint16_t fromColor = 0, toColor = 0;
    void (*onStepColor)(uint16_t, void*) = nullptr;

    // Slide
    ui::Rect fromRect{}, toRect{};
    void (*onStepRect)(ui::Rect, void*) = nullptr;

    // Blink
    void (*onToggle)(bool, void*) = nullptr;
    bool blinkOn = false;
    unsigned long lastToggleMs = 0;

    // Pulse
    uint8_t minVal = 0, maxVal = 0;
    void (*onStepVal)(uint8_t, void*) = nullptr;

    // CountUp
    int32_t fromVal = 0, toVal = 0;
    void (*onStepInt)(int32_t, void*) = nullptr;

    void* ctx = nullptr;
};

constexpr int kMaxSlots = 8;
static Slot s_slots[kMaxSlots];

static AnimId allocSlot() {
    for (int i = 0; i < kMaxSlots; i++) {
        if (!s_slots[i].active) return (AnimId)i;
    }
    return kInvalidAnim;
}

static uint16_t lerpColor565(uint16_t a, uint16_t b, float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    uint8_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    uint8_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    uint8_t rr = ar + (uint8_t)((br - ar) * t);
    uint8_t rg = ag + (uint8_t)((bg - ag) * t);
    uint8_t rb = ab + (uint8_t)((bb - ab) * t);
    return (rr << 11) | (rg << 5) | rb;
}

AnimId startFade(uint16_t fromColor, uint16_t toColor, uint16_t durationMs,
                  void (*onStep)(uint16_t, void*), void* ctx) {
    AnimId id = allocSlot();
    if (id == kInvalidAnim) return id;
    Slot& s = s_slots[id];
    s = Slot{};
    s.active = true;
    s.kind = Kind::Fade;
    s.startMs = millis();
    s.durationMs = durationMs;
    s.fromColor = fromColor;
    s.toColor = toColor;
    s.onStepColor = onStep;
    s.ctx = ctx;
    if (onStep) onStep(fromColor, ctx);
    return id;
}

AnimId startSlide(ui::Rect from, ui::Rect to, uint16_t durationMs,
                   void (*onStep)(ui::Rect, void*), void* ctx) {
    AnimId id = allocSlot();
    if (id == kInvalidAnim) return id;
    Slot& s = s_slots[id];
    s = Slot{};
    s.active = true;
    s.kind = Kind::Slide;
    s.startMs = millis();
    s.durationMs = durationMs;
    s.fromRect = from;
    s.toRect = to;
    s.onStepRect = onStep;
    s.ctx = ctx;
    if (onStep) onStep(from, ctx);
    return id;
}

AnimId startBlink(uint16_t periodMs, void (*onToggle)(bool, void*), void* ctx) {
    AnimId id = allocSlot();
    if (id == kInvalidAnim) return id;
    Slot& s = s_slots[id];
    s = Slot{};
    s.active = true;
    s.kind = Kind::Blink;
    s.startMs = millis();
    s.lastToggleMs = s.startMs;
    s.periodMs = periodMs;
    s.onToggle = onToggle;
    s.blinkOn = true;
    s.ctx = ctx;
    if (onToggle) onToggle(true, ctx);
    return id;
}

AnimId startPulse(uint8_t minVal, uint8_t maxVal, uint16_t periodMs,
                   void (*onStep)(uint8_t, void*), void* ctx) {
    AnimId id = allocSlot();
    if (id == kInvalidAnim) return id;
    Slot& s = s_slots[id];
    s = Slot{};
    s.active = true;
    s.kind = Kind::Pulse;
    s.startMs = millis();
    s.periodMs = periodMs;
    s.minVal = minVal;
    s.maxVal = maxVal;
    s.onStepVal = onStep;
    s.ctx = ctx;
    if (onStep) onStep(minVal, ctx);
    return id;
}

AnimId startCountUp(int32_t fromVal, int32_t toVal, uint16_t durationMs,
                     void (*onStep)(int32_t, void*), void* ctx) {
    AnimId id = allocSlot();
    if (id == kInvalidAnim) return id;
    Slot& s = s_slots[id];
    s = Slot{};
    s.active = true;
    s.kind = Kind::CountUp;
    s.startMs = millis();
    s.durationMs = durationMs;
    s.fromVal = fromVal;
    s.toVal = toVal;
    s.onStepInt = onStep;
    s.ctx = ctx;
    if (onStep) onStep(fromVal, ctx);
    return id;
}

bool isDone(AnimId id) {
    if (id >= kMaxSlots) return true;
    return !s_slots[id].active;
}

void cancel(AnimId id) {
    if (id >= kMaxSlots) return;
    s_slots[id].active = false;
}

static void stepTimed(Slot& s, unsigned long now) {
    unsigned long elapsed = now - s.startMs;
    float t = s.durationMs ? (float)elapsed / (float)s.durationMs : 1.0f;
    bool finished = t >= 1.0f;
    if (finished) t = 1.0f;

    switch (s.kind) {
        case Kind::Fade:
            if (s.onStepColor) s.onStepColor(lerpColor565(s.fromColor, s.toColor, t), s.ctx);
            break;
        case Kind::Slide:
            if (s.onStepRect) {
                ui::Rect r;
                r.x = s.fromRect.x + (int16_t)((s.toRect.x - s.fromRect.x) * t);
                r.y = s.fromRect.y + (int16_t)((s.toRect.y - s.fromRect.y) * t);
                r.w = s.fromRect.w + (int16_t)((s.toRect.w - s.fromRect.w) * t);
                r.h = s.fromRect.h + (int16_t)((s.toRect.h - s.fromRect.h) * t);
                s.onStepRect(r, s.ctx);
            }
            break;
        case Kind::CountUp:
            if (s.onStepInt) s.onStepInt(s.fromVal + (int32_t)((s.toVal - s.fromVal) * t), s.ctx);
            break;
        default:
            break;
    }

    if (finished) s.active = false;
}

static void stepBlink(Slot& s, unsigned long now) {
    if (now - s.lastToggleMs < s.periodMs / 2) return;
    s.lastToggleMs = now;
    s.blinkOn = !s.blinkOn;
    if (s.onToggle) s.onToggle(s.blinkOn, s.ctx);
}

static void stepPulse(Slot& s, unsigned long now) {
    unsigned long cyclePos = (now - s.startMs) % s.periodMs;
    float half = s.periodMs / 2.0f;
    float t = (cyclePos < half) ? (cyclePos / half) : (2.0f - cyclePos / half);
    uint8_t val = s.minVal + (uint8_t)((s.maxVal - s.minVal) * t);
    if (s.onStepVal) s.onStepVal(val, s.ctx);
}

void update() {
    unsigned long now = millis();
    for (int i = 0; i < kMaxSlots; i++) {
        Slot& s = s_slots[i];
        if (!s.active) continue;
        switch (s.kind) {
            case Kind::Fade:
            case Kind::Slide:
            case Kind::CountUp:
                stepTimed(s, now);
                break;
            case Kind::Blink:
                stepBlink(s, now);
                break;
            case Kind::Pulse:
                stepPulse(s, now); // continuous until explicitly cancelled
                break;
            default:
                break;
        }
    }
}

} // namespace anim

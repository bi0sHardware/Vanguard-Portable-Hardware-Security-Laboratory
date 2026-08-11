#pragma once
#include <cstdint>

// Non-blocking, named LED effects on top of led_chain.*/LED_BUILTIN1 — screens ask for
// an effect by name instead of hand-rolling delay()-based sequencing. Effects are
// generic; any module can use any effect.
namespace led {

// Chain bit-group masks derived from led_chain.cpp's LED-number -> bit mapping.
constexpr uint16_t kMaskRed   = 0x00F0; // LED1-4 (IC1 high nibble)
constexpr uint16_t kMaskGreen = 0x000F; // LED5-8 (IC1 low nibble)
constexpr uint16_t kMaskWhite = 0xFC00; // LED9-14 (IC2, side LEDs)
constexpr uint16_t kMaskAll   = kMaskRed | kMaskGreen | kMaskWhite;

enum class EffectId {
    Off,
    BootSweep,         // one-shot center-out sweep across all 14 LEDs (boot only)
    PitchVisualizer,    // instantaneous freq -> color-group mapping (music player)

    ChallengeComplete,  // generic self-terminating N-flash of the masked bits
    SuccessFlash,       // ChallengeComplete preset: green group, quick double-flash
    FailurePulse,       // ChallengeComplete preset: red group, slower triple-pulse

    ConnectionPulse,     // slow on/off pulse of the masked bits, continuous until stop()
    Breathing,           // smooth 0..N..0 lit-count envelope, continuous until stop()
    Sweep,               // single point travels along the chain, wraps, continuous until stop()
    PacketFlow,          // 2-3 points chase along the chain — "data flowing" look, continuous until stop()
    SideJoin,            // center column rises bottom->top, then white sides light, holds, restarts (Screensaver ambient effect)

    Notification,       // fast blink, continuous until stop()
    Warning,             // faster blink than Notification, continuous until stop()
    Idle,                // slow single heartbeat blink on LED_BUILTIN1 only
};

// No default member initializers: needed for aggregate-init under C++11 (see ui::Region).
struct EffectParams {
    uint16_t freqHz;     // PitchVisualizer: note frequency driving the color group
    uint8_t flashCount;  // ChallengeComplete family: number of on/off flashes, 0 = default
    uint16_t periodMs;   // 0 = use the effect's built-in default period
    uint16_t mask;       // which chain bits participate, 0 = default (kMaskAll for most effects)
};

void init();   // wraps led_chain::init()

// Call once per main loop tick. Steps the active effect via millis(); never uses delay().
void update();

void playEffect(EffectId id, EffectParams params = {});
void stop(); // equivalent to playEffect(EffectId::Off) — clears the chain + builtin

// Persistent challenge-progress indicator: green consumed / red lit one at a time as
// challenges complete (clamped [0,4]). This is the chain's resting state (shown whenever
// no effect is active), not a competing effect, so other effects return to it automatically.
void setChallengeProgress(uint8_t completedCount);

// While suppressed, resting state is fully dark instead of the progress pattern — lets
// games show their own LED feedback without competing with the progress indicator.
void setBaselineSuppressed(bool suppressed);

} // namespace led

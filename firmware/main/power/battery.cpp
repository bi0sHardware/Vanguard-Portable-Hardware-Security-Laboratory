#include "battery.h"
#include "../../include/pins.h"
#include "../../include/config.h"
#include <Arduino.h>

namespace power {

static float s_voltage = cfg::BATT_FULL_V;
static unsigned long s_lastSampleMs = 0;

// Small ring of recent samples for the charging-trend heuristic.
constexpr int kTrendSamples = 4;
constexpr float kNoiseFloorV = 0.02f; // ADC/divider noise floor to ignore
static float s_trend[kTrendSamples];
static int s_trendCount = 0;
static int s_trendNext = 0;

// GPIO1 is a dedicated ADC input (100k/100k divider + 10nF cap), configured once in
// init() and never repurposed — no pause/resume or per-sample pinMode() needed.
static void sampleNow() {
    int raw = analogRead(pins::VBATT_SENSE);
    float pinVoltage = (raw / 4095.0f) * cfg::ADC_REF_VOLTAGE;
    s_voltage = pinVoltage * cfg::BATT_DIVIDER_RATIO;

    s_trend[s_trendNext] = s_voltage;
    s_trendNext = (s_trendNext + 1) % kTrendSamples;
    if (s_trendCount < kTrendSamples) s_trendCount++;
}

void init() {
    analogReadResolution(12);
    pinMode(pins::VBATT_SENSE, INPUT);
    sampleNow();
}

void update() {
    unsigned long now = millis();
    if (now - s_lastSampleMs >= cfg::BATT_SAMPLE_INTERVAL_MS) {
        s_lastSampleMs = now;
        sampleNow();
    }
}

float batteryVoltage() { return s_voltage; }

uint8_t batteryPercent() {
    float clamped = s_voltage;
    if (clamped < cfg::BATT_EMPTY_V) clamped = cfg::BATT_EMPTY_V;
    if (clamped > cfg::BATT_FULL_V) clamped = cfg::BATT_FULL_V;
    float pct = (clamped - cfg::BATT_EMPTY_V) / (cfg::BATT_FULL_V - cfg::BATT_EMPTY_V) * 100.0f;
    return (uint8_t)(pct + 0.5f);
}

bool isCharging() {
    if (s_trendCount < 2) return false;
    int newestIdx = (s_trendNext - 1 + kTrendSamples) % kTrendSamples;
    int oldestIdx = (s_trendCount < kTrendSamples) ? 0 : s_trendNext;
    float newest = s_trend[newestIdx];
    float oldest = s_trend[oldestIdx];
    // Flat-to-rising trend (within noise floor) reads as "likely charging".
    return (newest - oldest) > -kNoiseFloorV;
}

} // namespace power

#pragma once
#include <cstdint>

// Battery sense on GPIO1 (dedicated ADC input, see pins.h).
namespace power {

void init();

// Call every loop tick; internally rate-limited to cfg::BATT_SAMPLE_INTERVAL_MS.
void update();

float batteryVoltage();
uint8_t batteryPercent();

// Heuristic only: no TP4056 CHRG/STAT line wired to the MCU, so this infers "likely
// charging" from a flat-to-rising voltage trend; can misfire near-full or during load dips.
bool isCharging();

} // namespace power

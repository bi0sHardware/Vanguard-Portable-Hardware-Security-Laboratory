#pragma once
#include <cstdint>

// Driver for the 16-bit 74HC595 chain (IC1 -> IC2) plus LED_BUILTIN1.
// Bit order per spec §6 (MSB-first shift), confirmed against firmware:
//   bit 7  = LED1  (IC1 QA, red,   center top)
//   bit 6  = LED2  (IC1 QB, red,   center 2nd)
//   bit 5  = LED3  (IC1 QC, red,   center 3rd)
//   bit 4  = LED4  (IC1 QD, red,   center 4th)
//   bit 3  = LED5  (IC1 QE, green, center 5th)
//   bit 2  = LED6  (IC1 QF, green, center 6th)
//   bit 1  = LED7  (IC1 QG, green, center 7th)
//   bit 0  = LED8  (IC1 QH, green, center bottom)
//   bit 15 = LED9  (IC2 QA, white, left top)
//   bit 14 = LED10 (IC2 QB, white, left bottom)
//   bit 13 = LED11 (IC2 QC, white, right top)
//   bit 12 = LED12 (IC2 QD, white, right bottom)
//   bit 11 = LED13 (IC2 QE, white, extra status)
//   bit 10 = LED14 (IC2 QF, white, extra status)
namespace leds {

void init();

// Set/clear a single LED (1-14) in the chain and push the update out.
void setChainLed(int ledNumber /*1-14*/, bool on);

// Write the full 16-bit chain value directly (per bit map above) and latch it out.
void writeChainRaw(uint16_t value);

uint16_t chainState();

void clearAll();

// LED_BUILTIN1 (GPIO2, direct drive, active-high) — separate from the chain.
void setBuiltin(bool on);

} // namespace leds

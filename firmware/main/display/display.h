#pragma once
#include <Adafruit_ST7789.h>

// ST7789 TFT — 240x320 panel run in landscape (320x240 active), spec §5.
namespace display {

Adafruit_ST7789& tft();

void init();

// NOT hardware brightness control -- backlight is hardwired to GND/3.3V, no GPIO.
// GPIO1 is the battery-sense ADC input, not the backlight pin (see pins.h TFT_BL), so PWM there would be wrong.
// Kept as pure in-memory storage so callers persisting a "brightness" value (NVS, Settings) don't need surgery;
// nothing should rely on these actually changing what's on screen.
void setBrightness(uint8_t percent);
uint8_t brightness();

} // namespace display

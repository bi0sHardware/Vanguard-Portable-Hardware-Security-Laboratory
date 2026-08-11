#include "display.h"
#include "../../include/pins.h"
#include "../../include/config.h"
#include <SPI.h>

namespace display {

static Adafruit_ST7789 s_tft(pins::TFT_CS, pins::TFT_DC, pins::TFT_RST);
static uint8_t s_brightness = 80; // stored only -- no hardware backlight control exists, see display.h

Adafruit_ST7789& tft() { return s_tft; }

void init() {
    SPI.begin(pins::TFT_SCK, pins::TFT_MISO, pins::TFT_MOSI, pins::TFT_CS);
    s_tft.init(240, 320); // native panel resolution
    s_tft.setSPISpeed(40000000); // 40MHz — confirmed-working shared FSPI bus speed (features_and_pins.md)
    s_tft.setRotation(3); // landscape -> 320x240 active area (rotation 1 was upside down on this panel)
    // Black, not white: this is on-screen during init before boot::enter()'s first draw, and a white fill flashed on power-on.
    s_tft.fillScreen(ST77XX_BLACK);
    // No backlight PWM setup here -- backlight is hardwired always-on, see display.h.
}

void setBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    s_brightness = percent;
}

uint8_t brightness() { return s_brightness; }

} // namespace display

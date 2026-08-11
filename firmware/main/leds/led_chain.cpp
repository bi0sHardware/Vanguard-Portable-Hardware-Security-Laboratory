#include "led_chain.h"
#include "../../include/pins.h"
#include <Arduino.h>
#include <SPI.h>

// SER/SRCLK share wires with the TFT's MOSI/SCK, already claimed by SPI.begin() in
// display::init(). Clocked out via the same hardware SPI bus (display CS deasserted)
// rather than bit-banged, to avoid ripping those pins out of SPI's GPIO routing.
// Only RCLK (dedicated latch pin) is touched directly.
namespace leds {

static uint16_t s_state = 0;
static SPISettings s_spiSettings(10000000, LSBFIRST, SPI_MODE0);

void init() {
    pinMode(pins::SR_RCLK, OUTPUT);
    digitalWrite(pins::SR_RCLK, LOW);
    pinMode(pins::LED_BUILTIN1, OUTPUT);
    writeChainRaw(0);
    setBuiltin(false);
}

void writeChainRaw(uint16_t value) {
    s_state = value;
    uint8_t highByte = (value >> 8) & 0xFF; // IC2 (LED9-14), QG/QH unused
    uint8_t lowByte  = value & 0xFF;        // IC1 (LED1-8)

    // IC1 cascades from IC2: send highByte (IC2) first, then lowByte (IC1), LSB-first.
    SPI.beginTransaction(s_spiSettings);
    SPI.transfer(highByte);
    SPI.transfer(lowByte);
    SPI.endTransaction();

    digitalWrite(pins::SR_RCLK, HIGH);
    digitalWrite(pins::SR_RCLK, LOW);
}

void setChainLed(int ledNumber, bool on) {
    if (ledNumber < 1 || ledNumber > 14) return;
    int bit;
    if (ledNumber <= 8) {
        bit = 8 - ledNumber; // LED1->bit7 ... LED8->bit0
    } else {
        bit = 16 - (ledNumber - 8); // LED9->bit15 ... LED14->bit10
    }
    uint16_t next = s_state;
    if (on) next |= (1u << bit);
    else next &= ~(1u << bit);
    writeChainRaw(next);
}

uint16_t chainState() { return s_state; }

void clearAll() { writeChainRaw(0); }

void setBuiltin(bool on) {
    digitalWrite(pins::LED_BUILTIN1, on ? HIGH : LOW);
}

} // namespace leds

#pragma once
// Native ESP-IDF HAL for RadioLib, replacing ArduinoHal for the RF/LoRa subsystem.
// WHY: ArduinoHal's delay/timing wrappers behave differently under
// Arduino-as-ESP-IDF-component vs classic Arduino-ESP32, which broke SX126x
// chip detection on this hardware. This HAL bypasses that: SPI via spi_master,
// GPIO via native gpio driver, microsecond timing via esp_rom_delay_us().
#include <RadioLib.h>
#include <SPI.h>
#include "driver/gpio.h"

class EspHal : public RadioLibHal {
public:
    EspHal(int8_t sck, int8_t miso, int8_t mosi);

    void init() override;
    void term() override;

    void pinMode(uint32_t pin, uint32_t mode) override;
    void digitalWrite(uint32_t pin, uint32_t value) override;
    uint32_t digitalRead(uint32_t pin) override;
    void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override;
    void detachInterrupt(uint32_t interruptNum) override;

    void delay(RadioLibTime_t ms) override;
    void delayMicroseconds(RadioLibTime_t us) override;
    RadioLibTime_t millis() override;
    RadioLibTime_t micros() override;
    long pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) override;

    void spiBegin() override;
    void spiBeginTransaction() override;
    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) override;
    void spiEndTransaction() override;
    void spiEnd() override;

private:
    int8_t m_sck, m_miso, m_mosi;
    // 2 MHz/MSBFIRST/MODE0 (RadioLib SX126x defaults); re-applied each
    // transaction so the display's SPI settings don't bleed into LoRa's.
    SPISettings m_settings{2000000, MSBFIRST, SPI_MODE0};
};

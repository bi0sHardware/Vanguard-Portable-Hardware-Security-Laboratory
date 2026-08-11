#include "EspHal.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"
#include <cstring>

namespace {
// Bridges RadioLibHal's void(void) callback to ESP-IDF's void(void*) ISR;
// single slot since only DIO1 is ever attached at a time.
void (*s_isrCallback)(void) = nullptr;

void IRAM_ATTR isrTrampoline(void* /*arg*/) {
    if (s_isrCallback) s_isrCallback();
}

} // namespace

EspHal::EspHal(int8_t sck, int8_t miso, int8_t mosi)
    : RadioLibHal(GPIO_MODE_INPUT, GPIO_MODE_OUTPUT, 0, 1, GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE),
      m_sck(sck), m_miso(miso), m_mosi(mosi) {}

void EspHal::init() {
    spiBegin();
}

void EspHal::term() {
    // Deliberately a no-op: SPI bus is shared with the display; RadioLib
    // calls term() on its own failure paths and freeing the bus there
    // would kill the display too (was a real bug previously).
}

void EspHal::pinMode(uint32_t pin, uint32_t mode) {
    if (pin == RADIOLIB_NC) return;
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode = static_cast<gpio_mode_t>(mode);
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&cfg);
}

void EspHal::digitalWrite(uint32_t pin, uint32_t value) {
    if (pin == RADIOLIB_NC) return;
    gpio_set_level(static_cast<gpio_num_t>(pin), value);
}

uint32_t EspHal::digitalRead(uint32_t pin) {
    if (pin == RADIOLIB_NC) return 0;
    return gpio_get_level(static_cast<gpio_num_t>(pin));
}

void EspHal::attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) {
    if (interruptNum == RADIOLIB_NC) return;
    static bool isrServiceInstalled = false;
    if (!isrServiceInstalled) {
        gpio_install_isr_service(0);
        isrServiceInstalled = true;
    }
    gpio_num_t pin = static_cast<gpio_num_t>(interruptNum);
    gpio_set_intr_type(pin, static_cast<gpio_int_type_t>(mode));
    s_isrCallback = interruptCb;
    gpio_isr_handler_add(pin, isrTrampoline, nullptr);
}

void EspHal::detachInterrupt(uint32_t interruptNum) {
    if (interruptNum == RADIOLIB_NC) return;
    gpio_isr_handler_remove(static_cast<gpio_num_t>(interruptNum));
    s_isrCallback = nullptr;
}

void EspHal::delay(RadioLibTime_t ms) {
    if (ms == 0) return;
    vTaskDelay(pdMS_TO_TICKS(ms) > 0 ? pdMS_TO_TICKS(ms) : 1);
}

void EspHal::delayMicroseconds(RadioLibTime_t us) {
    // ROM busy-wait, independent of FreeRTOS tick -- needed for SX126x SPI timing margins.
    ets_delay_us(us);
}

RadioLibTime_t EspHal::millis() {
    return static_cast<RadioLibTime_t>(esp_timer_get_time() / 1000ULL);
}

RadioLibTime_t EspHal::micros() {
    return static_cast<RadioLibTime_t>(esp_timer_get_time());
}

long EspHal::pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) {
    if (pin == RADIOLIB_NC) return 0;
    pinMode(pin, GPIO_MODE_INPUT);
    uint64_t start = esp_timer_get_time();
    while (digitalRead(pin) != state) {
        if (static_cast<RadioLibTime_t>(esp_timer_get_time() - start) > timeout) return 0;
    }
    uint64_t pulseStart = esp_timer_get_time();
    while (digitalRead(pin) == state) {
        if (static_cast<RadioLibTime_t>(esp_timer_get_time() - start) > timeout) return 0;
    }
    return static_cast<long>(esp_timer_get_time() - pulseStart);
}

void EspHal::spiBegin() {
    // Idempotent (safe regardless of who claims the bus first); ss=-1 leaves
    // chip-select to RadioLib's own digitalWrite() on LORA_NSS.
    SPI.begin(m_sck, m_miso, m_mosi, -1);
}

void EspHal::spiBeginTransaction() {
    // Takes the bus mutex and reprograms clock/bit-order/mode for LoRa.
    SPI.beginTransaction(m_settings);
}

void EspHal::spiTransfer(uint8_t* out, size_t len, uint8_t* in) {
    for (size_t i = 0; i < len; i++) {
        in[i] = SPI.transfer(out[i]);
    }
}

void EspHal::spiEndTransaction() {
    SPI.endTransaction();
}

void EspHal::spiEnd() {
    // No-op: shared bus outlives the radio. See term().
}

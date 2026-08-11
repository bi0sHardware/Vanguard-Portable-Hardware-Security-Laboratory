#include "buzzer.h"
#include "../../include/pins.h"
#include <Arduino.h>

namespace buzzer {

static hw_timer_t* s_timer = nullptr;
static volatile bool s_pinState = false;

void IRAM_ATTR onTimer() {
    s_pinState = !s_pinState;
    digitalWrite(pins::BUZZER1, s_pinState ? HIGH : LOW);
}

void init() {
    pinMode(pins::BUZZER1, OUTPUT);
    digitalWrite(pins::BUZZER1, LOW);
    s_timer = timerBegin(1000000); // 1MHz tick (Arduino-ESP32 3.x timerBegin takes a frequency directly)
    timerAttachInterrupt(s_timer, &onTimer);
}

void toneAsync(unsigned int freqHz) {
    timerStop(s_timer);
    if (freqHz == 0) {
        digitalWrite(pins::BUZZER1, LOW);
        return;
    }
    // Toggle at 2x frequency to produce a square wave at freqHz.
    uint64_t alarmValueUs = 1000000UL / (freqHz * 2UL);
    timerAlarm(s_timer, alarmValueUs, true, 0);
    timerStart(s_timer);
}

void stopAsync() {
    timerStop(s_timer);
    digitalWrite(pins::BUZZER1, LOW);
}

void off() {
    stopAsync();
}

} // namespace buzzer

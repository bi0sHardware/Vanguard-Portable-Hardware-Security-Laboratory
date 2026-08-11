#include "uart_leak.h"
#include "flag_reveal.h"
#include "../../include/pins.h"
#include <Arduino.h>
#include <base64.h>

namespace uartleak {

// A standard UART baud rate, deliberately not 115200 (this firmware's own USB-CDC rate).
static constexpr unsigned long BAUD = 19200;
static constexpr unsigned long INTERVAL_MS = 1000;

// Flag must be recovered off the wire, not from a flash dump: kept
// obfuscated, decoded to a local buffer only at init() before re-encoding
// for the wire. Distinct XOR key per level — see flag_reveal.h.
static const uint8_t kKey[] = { 0x4b, 0x9e, 0x2f, 0xc7 };
static const uint8_t kFlagObfuscated[] = {
    0x3c, 0xf7, 0x5d, 0xa2, 0x2f, 0xe5, 0x59, 0xf3, 0x25, 0xf9, 0x5a, 0xf3, 0x39, 0xfa, 0x70, 0xa0,
    0x39, 0xae, 0x5a, 0xa9, 0x2f, 0xc1, 0x5b, 0xf4, 0x39, 0xf3, 0x1e, 0xa9, 0x7f, 0xf2, 0x70, 0xf7,
    0x25, 0xf2, 0x1e, 0xa9, 0x78, 0xe3,
};

static HardwareSerial s_leak(1); // UART peripheral #1 — separate from Serial (native USB-CDC)
static unsigned long s_lastTx = 0;
static String s_b64Line;

// Sent base64-encoded so finding the baud rate isn't the whole challenge — a small decode step remains.
void init() {
    s_leak.begin(BAUD, SERIAL_8N1, -1, pins::UART_LEAK_TX);
    char flag[sizeof(kFlagObfuscated) + 1];
    flagreveal::decode(kFlagObfuscated, sizeof(kFlagObfuscated), kKey, sizeof(kKey), flag);
    s_b64Line = base64::encode(flag) + "\r\n";
}

void update() {
    unsigned long now = millis();
    if (now - s_lastTx < INTERVAL_MS) return;
    s_lastTx = now;
    s_leak.print(s_b64Line);
}

} // namespace uartleak

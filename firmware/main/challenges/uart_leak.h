#pragma once

// Level 1 — hidden diagnostic UART. Continuously transmits the recon flag
// on an undocumented HardwareSerial peripheral, independent of AppState/menu.
namespace uartleak {

void init();
void update(); // call every loop() tick; non-blocking, timer-gated

} // namespace uartleak

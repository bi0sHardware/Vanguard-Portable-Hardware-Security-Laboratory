#pragma once
// Master GPIO map — INCTF 2026 Operation VAJRA badge (ESP32-S3-WROOM-1)

namespace pins {

// ---- ST7789 TFT (shares FSPI bus with LoRa + shift registers) ----
constexpr int TFT_CS   = 10;
constexpr int TFT_DC   = 5;
constexpr int TFT_RST  = 4;
constexpr int TFT_MOSI = 11; // shared with LoRa MOSI + shift-register SER
constexpr int TFT_SCK  = 12; // shared with LoRa SCK + shift-register SRCLK
constexpr int TFT_MISO = 13; // shared with LoRa MISO (unused by TFT)
// No TFT_BL constant: backlight K/A wired directly to GND/3.3V, no GPIO/PWM control.

// ---- Joystick SW1 (active-LOW, internal pull-up) ----
constexpr int JOY_UP     = 45; // also MTDI strap pin — eFuse-fixed, see §4
constexpr int JOY_DOWN   = 37;
constexpr int JOY_LEFT   = 47;
constexpr int JOY_RIGHT  = 48;
constexpr int JOY_SELECT = 21;

// ---- Discrete buttons (active-HIGH, external pull-down) ----
constexpr int BTN_PAUSE = 15; // S1
constexpr int BTN_DISP  = 17; // S2 — global shortcut to Display settings
constexpr int BTN_START = 16; // S3
constexpr int BTN_BACK  = 38; // S4
constexpr int BTN_OK    = 18; // S5

// ---- Shift registers (74HC595 x2, IC1 red/green center, IC2 white sides) ----
constexpr int SR_SER   = 11; // shared with TFT_MOSI / LoRa MOSI
constexpr int SR_SRCLK = 12; // shared with TFT_SCK / LoRa SCK
constexpr int SR_RCLK  = 8;  // latch, both ICs

// ---- Status LED / Buzzer ----
constexpr int LED_BUILTIN1 = 2; // direct GPIO drive
constexpr int BUZZER1      = 7; // bit-banged square wave, no LEDC dependency

// Battery sense: 100k/100k divider off battery +ve, 10nF filter cap, dedicated ADC pin.
constexpr int VBATT_SENSE = 1;

// ---- LoRa (RA-01SC / LLCC68), shares FSPI bus ----
constexpr int LORA_NSS   = 9;
constexpr int LORA_MOSI  = 11; // shared bus
constexpr int LORA_MISO  = 13; // shared bus
constexpr int LORA_SCK   = 12; // shared bus
constexpr int LORA_DIO1  = 6;  // chip OUTPUT — never drive as MCU output
constexpr int LORA_BUSY  = 3;  // chip OUTPUT — never drive as MCU output
// LoRa RESET: hardware pull-up only, no GPIO (NC).

// ---- System ----
constexpr int SYS_BOOT = 0; // SW3, active-LOW strap pin
// SW4 RESET is wired to EN, not a regular GPIO — no firmware handling needed.

// ---- Challenge-only: Level 1 hidden diagnostic UART ----
// GPIO43 is ESP32-S3 hardware UART0 TX; badge's Serial runs over USB-CDC/JTAG instead,
// so this is a separate, undocumented peripheral (must not be the debug UART).
// NOT VERIFIED on physical PCB; any unused WROOM-1 GPIO works as a substitute.
constexpr int UART_LEAK_TX = 43;

} // namespace pins

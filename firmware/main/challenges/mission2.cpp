#include "mission2.h"
#include "challenge_data.h"
#include "challenge_engine.h"
#include "flag_reveal.h"
#include "serial_line.h"
#include "console.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../rf/lora.h"
#include "../rf/frame_codec.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"
#include "../audio/audio_manager.h"
#include <Arduino.h>
#include <cstring>
#include <mbedtls/sha256.h>

namespace mission2 {

// Short chirp confirming a telemetry packet arrived; distinct pitch from other SFX so it reads as "packet".
static const audio::Note kSfxPacketRx[] = { { 1800, 15 }, { 2400, 20 } };

// Satellite broadcasts 10 fixed frames on a loop: Packets 1-5 plaintext,
// 6-10 encrypted. This screen forwards only frames whose Information Field
// decodes to printable ASCII (Packets 1-5); Mission 03 forwards the rest.
//
// Expected values aren't stored as plain strings (would leak via `strings`
// dump) — only the SHA-256 of the four values joined with '|' is kept, same
// pattern as flagdesk.cpp's flag check.
static constexpr const char* kTxFrequency = "435.500 MHz";
static const uint8_t kExpectedHash[32] = {
    0x7b, 0x9c, 0x36, 0xc4, 0xed, 0x9c, 0x94, 0x6e, 0x15, 0xad, 0xa8, 0xcf, 0xb7, 0xa6, 0x75, 0x80,
    0x41, 0xfb, 0xd9, 0x2c, 0x98, 0x81, 0xbc, 0xb5, 0x66, 0xbc, 0xa0, 0x33, 0x10, 0x54, 0xf1, 0x03,
};

// Required "0x"-prefixed; values are upper-cased by the caller (runSetup()).
static bool verifySetupValues(const String& frameFlag, const String& ctrl,
                               const String& pid, const String& authCmd) {
    String combined = frameFlag + "|" + ctrl + "|" + pid + "|" + authCmd;
    uint8_t digest[32];
    mbedtls_sha256((const unsigned char*)combined.c_str(), combined.length(), digest, 0);
    return memcmp(digest, kExpectedHash, sizeof(digest)) == 0;
}

// Flow: Searching -> Satellite Found (OK to continue) -> USB Ready ->
// Connected -> Streaming (OK opens Protocol Verification). Falls back to
// UsbReady from Connected/Streaming if Serial disconnects.
enum class Screen { Searching, Found, UsbReady, Connected, Streaming, Setup, Result };
static Screen s_screen;
enum class SetupPhase { FrameFlag, CtrlField, Pid, AuthCmd };
static SetupPhase s_setupPhase;

// Cycles 1..5: which of the 5 printable packets was most recently seen, not a running total.
static unsigned int s_packetCount = 0;
static String s_lastPacket;
static bool s_dirty;
static bool s_setupPassed;
static unsigned long s_resultShownAt;
static unsigned long s_connectedShownAt;
// Tracks which of Packets 1-5 have been seen at least once; drives the "all captured" nudge below.
static bool s_seenPacket[5];
static bool s_allSeenNudged;

static String s_frameFlagIn, s_ctrlIn, s_pidIn;

static void printSetupBanner() {
    console::banner("MISSION 02 :: PROTOCOL VERIFICATION",
                    "Verify the frame fields you recovered");
    console::prompt("Frame Flag");
}

void enter() {
    s_screen = Screen::Searching;
    s_packetCount = 0;
    s_lastPacket = "";
    memset(s_seenPacket, 0, sizeof(s_seenPacket));
    s_allSeenNudged = false;
    s_dirty = true;
    ui::widgets::clearScreen(display::tft());
}

static void drawListening(Adafruit_ST7789& tft) {
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y}, "Mission 02");
    tft.fillRect(0, theme::LIST_START_Y, cfg::DISPLAY_WIDTH, cfg::DISPLAY_HEIGHT - theme::LIST_START_Y, theme::COLOR_BG);

    int y = 90;
    const char* footer = "BACK=exit";
    if (!rf::selfTest()) {
        // Distinct from "Searching...": radio never came up, and no
        // firmware-triggerable reset exists, so a power cycle is the only fix.
        theme::drawCentered(tft, "Radio Offline", y, theme::HEADER_TEXT_SIZE, theme::COLOR_DANGER);
        y += 24;
        theme::drawCentered(tft, "Switch the badge", y, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
        y += theme::BODY_LINE_HEIGHT;
        theme::drawCentered(tft, "OFF and back ON", y, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
    } else if (s_screen == Screen::Searching) {
        theme::drawCentered(tft, "Searching...", y, theme::HEADER_TEXT_SIZE, theme::COLOR_TEXT);
    } else if (s_screen == Screen::Found) {
        theme::drawCentered(tft, "Satellite Found", y, theme::HEADER_TEXT_SIZE, theme::COLOR_SUCCESS);
        y += 30;
        theme::drawCentered(tft, "Press OK to continue", y, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
        footer = "OK=Continue  BACK=exit";
    } else if (s_screen == Screen::UsbReady) {
        theme::drawCentered(tft, "Mission Control", y, theme::HEADER_TEXT_SIZE, theme::COLOR_ACCENT);
        y += 24;
        theme::drawCentered(tft, "USB Ready", y, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
        y += theme::BODY_LINE_HEIGHT * 2;
        theme::drawCentered(tft, "Connect USB", y, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
        y += theme::BODY_LINE_HEIGHT;
        theme::drawCentered(tft, "Open PuTTY", y, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
    } else if (s_screen == Screen::Connected) {
        theme::drawCentered(tft, "Mission Control", y, theme::HEADER_TEXT_SIZE, theme::COLOR_SUCCESS);
        y += 24;
        theme::drawCentered(tft, "Connected", y, theme::BODY_TEXT_SIZE, theme::COLOR_SUCCESS);
    } else { // Streaming
        bool allSeen = true;
        for (int i = 0; i < 5; i++) allSeen = allSeen && s_seenPacket[i];
        theme::drawCentered(tft, "Streaming", y, theme::HEADER_TEXT_SIZE, theme::COLOR_SUCCESS);
        y += 24;
        theme::drawCentered(tft, "RX Packets...", y, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
        y += theme::BODY_LINE_HEIGHT;
        char buf[24];
        snprintf(buf, sizeof(buf), "Packet #%u", s_packetCount);
        theme::drawCentered(tft, buf, y, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
        if (allSeen) {
            y += theme::BODY_LINE_HEIGHT;
            theme::drawCentered(tft, "All 5 captured!", y, theme::BODY_TEXT_SIZE, theme::COLOR_SUCCESS);
        }
        footer = "OK=Verify Protocol  BACK=exit";
    }
    theme::drawCentered(tft, footer, cfg::DISPLAY_HEIGHT - 20, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
}

static void drawResult(Adafruit_ST7789& tft) {
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y}, "Protocol Verification");
    tft.fillRect(0, theme::LIST_START_Y, cfg::DISPLAY_WIDTH, cfg::DISPLAY_HEIGHT - theme::LIST_START_Y, theme::COLOR_BG);
    if (s_setupPassed) {
        theme::drawCentered(tft, "Protocol Verified", 90, theme::HEADER_TEXT_SIZE, theme::COLOR_SUCCESS);
        theme::drawCentered(tft, "Mission Complete", 114, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
        theme::drawCentered(tft, "See serial console", 150, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
        theme::drawCentered(tft, "for your flag", 164, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
    } else {
        theme::drawCentered(tft, "Verification Failed", 110, theme::HEADER_TEXT_SIZE, theme::COLOR_DANGER);
        theme::drawCentered(tft, "Check recovered values", 140, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
    }
    theme::drawCentered(tft, "BACK to continue", cfg::DISPLAY_HEIGHT - 20, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
}

// Reveals the flag over Serial only; player still submits via FlagDesk to
// officially mark Level 2 complete. Obfuscated (multi-byte XOR, see
// flag_reveal.h); distinct key from uart_leak.cpp/mission3.cpp.
static const uint8_t kKey[] = { 0x8d, 0x31, 0xf6, 0x5a };
static const uint8_t kFlagObfuscated[] = {
    0xfa, 0x58, 0x84, 0x3f, 0xe9, 0x4a, 0xc1, 0x69, 0xe1, 0x02, 0x9b, 0x69, 0xba, 0x43, 0x8f, 0x05,
    0xe1, 0x00, 0x98, 0x31, 0xd2, 0x02, 0xc3, 0x6d, 0xb9, 0x53, 0x9a, 0x6b, 0xb8, 0x59, 0xc5, 0x3e,
    0xf0,
};

static void printSuccessCascade() {
    console::ok("Protocol Verified.");
    char flag[sizeof(kFlagObfuscated) + 1];
    flagreveal::decode(kFlagObfuscated, sizeof(kFlagObfuscated), kKey, sizeof(kKey), flag);
    console::flagBlock("MISSION 02 COMPLETE", flag);
    console::banner("MISSION UPDATE", "Recovered satellite configuration");
    console::field("TX Frequency", kTxFrequency, console::GREEN);
    console::ok("RF transmitter unlocked.");
    console::info("Mission 03 ready: Establishing the Uplink.");
    console::step("Submit the flag via Challenges > Submit Flag to mark Level 2 complete.");
}

static void beginSetup() {
    s_screen = Screen::Setup;
    s_setupPhase = SetupPhase::FrameFlag;
    s_frameFlagIn = "";
    s_ctrlIn = "";
    s_pidIn = "";
    serialline::reset();
    s_dirty = true;
    printSetupBanner();
}

static void runSetup() {
    String line;
    if (!serialline::readEchoedLine(line)) return;

    switch (s_setupPhase) {
        case SetupPhase::FrameFlag:
            s_frameFlagIn = line;
            s_setupPhase = SetupPhase::CtrlField;
            console::prompt("UI Control Field");
            break;
        case SetupPhase::CtrlField:
            s_ctrlIn = line;
            s_setupPhase = SetupPhase::Pid;
            console::prompt("Layer 3 PID");
            break;
        case SetupPhase::Pid:
            s_pidIn = line;
            s_setupPhase = SetupPhase::AuthCmd;
            console::prompt("Authentication Cmd");
            break;
        case SetupPhase::AuthCmd: {
            String frameFlag = s_frameFlagIn, ctrl = s_ctrlIn, pid = s_pidIn, authCmd = line;
            frameFlag.toUpperCase();
            ctrl.toUpperCase();
            pid.toUpperCase();
            authCmd.toUpperCase();
            s_setupPassed = verifySetupValues(frameFlag, ctrl, pid, authCmd);
            if (s_setupPassed) {
                printSuccessCascade();
            } else {
                console::err("Verification failed - one or more values incorrect.");
            }
            s_screen = Screen::Result;
            s_resultShownAt = millis();
            s_dirty = true;
            break;
        }
    }
}

// True for phases treated as "the Listening screen"; Setup/Result handled separately.
static bool isListeningPhase() {
    return s_screen == Screen::Searching || s_screen == Screen::Found ||
           s_screen == Screen::UsbReady || s_screen == Screen::Connected ||
           s_screen == Screen::Streaming;
}

// Drops frames whose Information Field isn't printable ASCII (encrypted Packets 6-10).
static bool acceptPacket(const String& raw, String& infoOut) {
    uint8_t dest[framecodec::kAddrBytes], src[framecodec::kAddrBytes], pid, info[128];
    size_t infoLen;
    if (!framecodec::parseFrame(raw, dest, src, pid, info, sizeof(info), infoLen)) return false;
    if (!framecodec::isPrintableAscii(info, infoLen)) return false;
    infoOut = raw;
    return true;
}

bool frame() {
    if (input::wasPressed(input::Button::Back)) {
        if (isListeningPhase()) return true;
        s_screen = Screen::Streaming;
        s_dirty = true;
        return false;
    }

    if (isListeningPhase()) {
        String raw, packet;
        if (rf::pollReceive(raw) && acceptPacket(raw, packet)) {
            // Mod 5 tracks the satellite's own "Packet N" numbering; an
            // ever-increasing counter would show e.g. "Packet #47".
            s_packetCount = (s_packetCount % 5) + 1;
            s_lastPacket = packet;
            s_seenPacket[s_packetCount - 1] = true;
            audio::playSfx(kSfxPacketRx, 2);
            if (s_screen == Screen::Searching) {
                s_screen = Screen::Found;
                s_dirty = true;
            }
            // Only printed once actually Streaming; `if (Serial)` is just the no-terminal-blocks guard.
            if (s_screen == Screen::Streaming) {
                if (Serial) {
                    char label[24];
                    snprintf(label, sizeof(label), "Packet #%u", s_packetCount);
                    console::field(label, packet.c_str(), console::GREEN);
                }
                // One-time nudge once all 5 distinct packets have been seen.
                if (!s_allSeenNudged) {
                    bool allSeen = true;
                    for (int i = 0; i < 5; i++) allSeen = allSeen && s_seenPacket[i];
                    if (allSeen) {
                        s_allSeenNudged = true;
                        if (Serial) {
                            console::step("All 5 packets captured - press OK to Verify protocol.");
                        }
                    }
                }
                s_dirty = true;
            }
        }

        if (s_screen == Screen::Found && input::wasPressed(input::Button::Ok)) {
            s_screen = Screen::UsbReady;
            s_dirty = true;
        } else if (s_screen == Screen::UsbReady) {
            if (Serial) {
                console::banner("MISSION CONTROL CONNECTED", "Streaming frames below");
                s_screen = Screen::Connected;
                s_connectedShownAt = millis();
                s_dirty = true;
            }
        } else if (s_screen == Screen::Connected) {
            if (!Serial) {
                s_screen = Screen::UsbReady; // terminal closed
                s_dirty = true;
            } else if (millis() - s_connectedShownAt > 1200) {
                s_screen = Screen::Streaming;
                s_dirty = true;
            }
        } else if (s_screen == Screen::Streaming) {
            if (!Serial) {
                s_screen = Screen::UsbReady;
                s_dirty = true;
            } else if (input::wasPressed(input::Button::Ok)) {
                beginSetup();
            }
        }
    } else if (s_screen == Screen::Setup) {
        runSetup();
    } else if (s_screen == Screen::Result) {
        if (millis() - s_resultShownAt > 2500) {
            s_screen = Screen::Streaming;
            s_dirty = true;
        }
    }

    if (s_dirty) {
        auto& tft = display::tft();
        if (isListeningPhase()) drawListening(tft);
        else if (s_screen == Screen::Result) drawResult(tft);
        s_dirty = false;
    }
    return false;
}

} // namespace mission2

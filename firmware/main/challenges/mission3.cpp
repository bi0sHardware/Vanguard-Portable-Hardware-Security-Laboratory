#include "mission3.h"
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
#include <cstdio>
#include <esp_random.h>

namespace mission3 {

// Same chirp mission2.cpp plays, reused for consistency across the split loop.
static const audio::Note kSfxPacketRx[] = { { 1800, 15 }, { 2400, 20 } };

// No expected-phrase constant here — badge doesn't know the correct answer;
// player recovers the PCB silkscreen inscription and types it in. Real
// check lives in satellite_sim.cpp's kExpectedUplinkPhrase.

// How long to wait for the satellite's acknowledgement after transmitting.
static constexpr unsigned long kAckTimeoutMs = 2500;
// Satellite is half-duplex; an uplink landing mid-transmit isn't heard, so retry a few times.
static constexpr int kMaxUplinkAttempts = 5;

// Mirrors mission2.cpp's flow exactly (Searching -> Found -> UsbReady ->
// Connected -> Streaming), forwarding the encrypted half of the same loop.
enum class Screen {
    Intro, // one-time instructions shown when the player opens this screen
    Searching, Found, UsbReady, Connected, Streaming, // listening phases
    Submit, Verifying, Transmitting, AwaitingResp, Success, Failed,
};
static Screen s_screen;
static bool s_dirty;
// Which of packets 6..10 was most recently seen, synced via s_haveSeenPlain (not a running total).
static unsigned int s_packetCount = 0;
// True once a plaintext (1-5) frame is seen with no encrypted frame since —
// loop order is fixed, so the next encrypted frame is always Packet 6.
static bool s_haveSeenPlain = false;
// True once Packet #10 shown for this capture, until #6 arrives again — capture stays "closed" between.
static bool s_captureComplete = false;
static String s_err;
static unsigned long s_screenAt;
static unsigned long s_connectedShownAt;
static String s_lastFrame;
static int s_attempt = 0;

static void setScreen(Screen s) {
    s_screen = s;
    s_screenAt = millis();
    s_dirty = true;
}

void enter() {
    s_screen = Screen::Intro;
    s_packetCount = 0;
    s_haveSeenPlain = false;
    s_captureComplete = false;
    s_err = "";
    s_lastFrame = "";
    s_dirty = true;
    ui::widgets::clearScreen(display::tft());
}

// ---------------------------------------------------------------- drawing

static void drawCenteredPair(Adafruit_ST7789& tft, const char* title,
                             const char* l1, const char* l2, uint16_t titleColor) {
    ui::widgets::clearScreen(tft);
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH,
                                      (int16_t)theme::LIST_START_Y}, "Mission 03");
    theme::drawCentered(tft, title, 70, theme::LIST_TEXT_SIZE, titleColor);
    if (l1) theme::drawCentered(tft, l1, 120, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
    if (l2) theme::drawCentered(tft, l2, 150, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
    theme::drawCentered(tft, "BACK to exit", 210, theme::BODY_TEXT_SIZE,
                        theme::COLOR_ACCENT_DARK);
}

static void drawIntro(Adafruit_ST7789& tft) {
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y}, "Mission 03");
    tft.fillRect(0, theme::LIST_START_Y, cfg::DISPLAY_WIDTH, cfg::DISPLAY_HEIGHT - theme::LIST_START_Y, theme::COLOR_BG);

    int y = 80;
    theme::drawCentered(tft, "Recognise the alphabet.", y, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
    y += theme::BODY_LINE_HEIGHT * 2;
    theme::drawCentered(tft, "Capture every encrypted packet.", y, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
    y += theme::BODY_LINE_HEIGHT * 2;
    theme::drawCentered(tft, "Wait until Packet #10", y, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
    y += theme::BODY_LINE_HEIGHT;
    theme::drawCentered(tft, "before attempting decryption.", y, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);

    theme::drawCentered(tft, "OK=Continue  BACK=exit", cfg::DISPLAY_HEIGHT - 20,
                        theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
}

static void drawListening(Adafruit_ST7789& tft) {
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y}, "Mission 03");
    tft.fillRect(0, theme::LIST_START_Y, cfg::DISPLAY_WIDTH, cfg::DISPLAY_HEIGHT - theme::LIST_START_Y, theme::COLOR_BG);

    int y = 90;
    const char* footer = "BACK=exit";
    if (!rf::selfTest()) {
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
    } else if (s_captureComplete) { // Streaming, capture closed after #10
        theme::drawCentered(tft, "Capture Complete", y, theme::HEADER_TEXT_SIZE, theme::COLOR_SUCCESS);
        y += 24;
        theme::drawCentered(tft, "Press OK to submit", y, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
        y += theme::BODY_LINE_HEIGHT;
        theme::drawCentered(tft, "your answer", y, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
        footer = "OK=Submit Uplink  BACK=exit";
    } else { // Streaming
        theme::drawCentered(tft, "Streaming", y, theme::HEADER_TEXT_SIZE, theme::COLOR_SUCCESS);
        y += 24;
        theme::drawCentered(tft, "RX Frames...", y, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
        y += theme::BODY_LINE_HEIGHT;
        char buf[24];
        snprintf(buf, sizeof(buf), "Packet #%u", s_packetCount);
        theme::drawCentered(tft, buf, y, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
        footer = "OK=Submit Uplink  BACK=exit";
    }
    theme::drawCentered(tft, footer, cfg::DISPLAY_HEIGHT - 20, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
}

static void draw() {
    Adafruit_ST7789& tft = display::tft();
    switch (s_screen) {
        case Screen::Intro:
            drawIntro(tft);
            break;
        case Screen::Searching: case Screen::Found: case Screen::UsbReady:
        case Screen::Connected: case Screen::Streaming:
            drawListening(tft);
            break;
        case Screen::Submit:
            break; // no OLED redraw -- console owns this phase
        case Screen::Verifying:
            drawCenteredPair(tft, "Verifying...", "Checking payload", nullptr,
                             theme::COLOR_ACCENT);
            break;
        case Screen::Transmitting:
            drawCenteredPair(tft, "UPLINK", "Transmitting...", nullptr,
                             theme::COLOR_ACCENT);
            break;
        case Screen::AwaitingResp:
            drawCenteredPair(tft, "Awaiting", "Satellite", "Response...",
                             theme::COLOR_ACCENT);
            break;
        case Screen::Success:
            drawCenteredPair(tft, "Authentication", "Accepted", "DOWNLINK OK",
                             theme::COLOR_ACCENT);
            break;
        case Screen::Failed:
            drawCenteredPair(tft, "Uplink Failed", s_err.c_str(), nullptr,
                             theme::COLOR_ACCENT_DARK);
            break;
    }
}

// ------------------------------------------------------------- protocol

static bool isListeningPhase() {
    return s_screen == Screen::Searching || s_screen == Screen::Found ||
           s_screen == Screen::UsbReady || s_screen == Screen::Connected ||
           s_screen == Screen::Streaming;
}

// dest=satellite, src=ground — mirror of satellite_sim.cpp's downlink addressing.
static String buildUplinkFrame(const uint8_t* info, size_t n) {
    uint8_t dest[framecodec::kAddrBytes], src[framecodec::kAddrBytes];
    framecodec::satelliteAddress(false, dest);
    framecodec::groundAddress(true, src);
    return framecodec::buildFrame(dest, src, framecodec::kPid, info, n);
}

// Only checks the payload fits the frame's Information Field — badge never
// validates content, the satellite alone decides accept/reject.
static bool copyPlainPayload(const String& in, uint8_t* out, size_t cap, size_t& outLen) {
    String text = in;
    text.trim();
    if (text.length() == 0 || text.length() > cap) return false;
    for (size_t i = 0; i < text.length(); i++) out[i] = (uint8_t)text[i];
    outLen = text.length();
    return true;
}

// Per-attempt token appended as ":XXXX", echoed back in the satellite's
// ACK/NACK. All badges share the same addressing on one shared satellite,
// so without this a badge could accept ANY other badge's "ACK=AUTH_OK" as
// its own. Regenerated only per Submit; retransmit() reuses the same token.
static char s_uplinkToken[5]; // 4 hex chars + nul

static void generateUplinkToken() {
    uint32_t r = esp_random();
    snprintf(s_uplinkToken, sizeof(s_uplinkToken), "%04X", (unsigned)(r & 0xFFFF));
}

static void fail(const char* why) {
    s_err = why;
    setScreen(Screen::Failed);
    console::err(why);
    console::warn("Uplink rejected - re-check the payload you recovered.");
}

static void beginSubmit() {
    s_lastFrame = "";
    serialline::reset();
    setScreen(Screen::Submit);
    console::banner("MISSION 03 :: SUBMIT UPLINK", "Enter the translated inscription you recovered");
    console::prompt("Uplink Payload");
}

// Transmitted exactly as given, with no encoding or correctness check by the badge.
static void parseAndTransmit(const String& payload) {
    setScreen(Screen::Verifying);
    draw(); // this phase is too brief to survive to the next frame() tick

    uint8_t info[64];
    size_t n = 0; // always set by copyPlainPayload() on success
    if (!copyPlainPayload(payload, info, sizeof(info) - 5, n)) { // -5 reserves room for ":XXXX"
        fail("Empty or oversized payload");
        return;
    }
    console::ok("Payload accepted.");

    // Append this attempt's token so the satellite can echo it back.
    generateUplinkToken();
    info[n++] = ':';
    memcpy(info + n, s_uplinkToken, 4);
    n += 4;

    if (!rf::selfTest()) {
        fail("Radio offline");
        console::warn("Radio did not initialise - switch the badge OFF and back ON.");
        return;
    }

    setScreen(Screen::Transmitting);
    draw();
    String pkt = buildUplinkFrame(info, n);

    console::step("Transmitting uplink...");
    if (!rf::transmit(pkt)) {
        fail("TX failed");
        return;
    }
    console::ok("Uplink transmitted.");
    console::step("Awaiting satellite response...");
    s_attempt = 1;
    s_lastFrame = pkt; // retained so a retry resends it verbatim
    setScreen(Screen::AwaitingResp);
}

static void retransmit() {
    rf::transmit(s_lastFrame);
}

// ---------------------------------------------------------------- frame

bool frame() {
    if (input::wasPressed(input::Button::Back)) {
        if (s_screen == Screen::Intro || isListeningPhase()) return true;
        setScreen(Screen::Streaming);
        return false;
    }

    if (s_screen == Screen::Intro) {
        if (input::wasPressed(input::Button::Ok)) {
            setScreen(Screen::Searching);
        }
        if (s_dirty) { s_dirty = false; draw(); }
        return false;
    }

    if (isListeningPhase()) {
        String raw;
        if (rf::pollReceive(raw)) {
            uint8_t dest[framecodec::kAddrBytes], src[framecodec::kAddrBytes], pid, info[128];
            size_t infoLen;
            if (framecodec::parseFrame(raw, dest, src, pid, info, sizeof(info), infoLen)) {
                bool printable = framecodec::isPrintableAscii(info, infoLen);
                if (printable) {
                    // Sync marker only: loop order is fixed, so the next encrypted frame is always Packet 6.
                    s_haveSeenPlain = true;
                } else if (s_haveSeenPlain || (s_packetCount >= 6 && s_packetCount < 10)) {
                    // Resync to #6, or continue mid-sequence. Otherwise
                    // (opened mid-loop with no plaintext boundary seen)
                    // ignore, so display never starts anywhere but #6.
                    s_packetCount = s_haveSeenPlain ? 6 : s_packetCount + 1;
                    s_haveSeenPlain = false;
                    if (s_packetCount == 6) s_captureComplete = false; // fresh capture starting
                    audio::playSfx(kSfxPacketRx, 2);
                    if (s_screen == Screen::Searching) {
                        setScreen(Screen::Found);
                    }
                    // Only printed once actually Streaming (see mission2.cpp).
                    if (s_screen == Screen::Streaming) {
                        if (Serial) {
                            char label[24];
                            snprintf(label, sizeof(label), "Packet #%u", s_packetCount);
                            console::field(label, raw.c_str(), console::GREEN);
                            if (s_packetCount == 10) {
                                console::step("Packets 6-10 captured - decrypt them, Transmit the message decoded");
                                console::step(", then press OK to Transmit the hidden message");
                            }
                        }
                        if (s_packetCount == 10) s_captureComplete = true;
                        s_dirty = true;
                    }
                }
            }
        }

        if (s_screen == Screen::Found && input::wasPressed(input::Button::Ok)) {
            setScreen(Screen::UsbReady);
        } else if (s_screen == Screen::UsbReady) {
            if (Serial) {
                console::banner("MISSION CONTROL CONNECTED", "Streaming encrypted frames below");
                setScreen(Screen::Connected);
                s_connectedShownAt = millis();
            }
        } else if (s_screen == Screen::Connected) {
            if (!Serial) {
                setScreen(Screen::UsbReady); // terminal closed
            } else if (millis() - s_connectedShownAt > 1200) {
                setScreen(Screen::Streaming);
            }
        } else if (s_screen == Screen::Streaming) {
            if (!Serial) {
                setScreen(Screen::UsbReady);
            } else if (input::wasPressed(input::Button::Ok)) {
                beginSubmit();
            }
        }
    } else if (s_screen == Screen::Submit) {
        String line;
        if (serialline::readEchoedLine(line)) {
            parseAndTransmit(line);
        }
    } else if (s_screen == Screen::AwaitingResp) {
        String resp;
        // Only the satellite's own ACK counts, not its unrelated loop
        // traffic arriving mid-wait — keep listening rather than failing.
        if (rf::pollReceive(resp)) {
            uint8_t dest[framecodec::kAddrBytes], src[framecodec::kAddrBytes], pid, info[64];
            size_t infoLen;
            uint8_t expectedDest[framecodec::kAddrBytes], expectedSrc[framecodec::kAddrBytes];
            framecodec::groundAddress(false, expectedDest);
            framecodec::satelliteAddress(true, expectedSrc);
            // SECURITY: also check SRC, not just DEST+text — otherwise
            // anyone who's solved Level 2 could forge an ACK from any source.
            if (framecodec::parseFrame(resp, dest, src, pid, info, sizeof(info), infoLen) &&
                memcmp(dest, expectedDest, framecodec::kAddrBytes) == 0 &&
                memcmp(src, expectedSrc, framecodec::kAddrBytes) == 0) {
                String text;
                for (size_t i = 0; i < infoLen; i++) text += (char)info[i];
                // An ACK/NACK addressed to "us" could be for a different
                // badge; require our own token, or ignore (not fail) it.
                String expectedSuffix = String(":") + s_uplinkToken;
                bool tokenMatches = text.endsWith(expectedSuffix);
                String base = tokenMatches ? text.substring(0, text.length() - expectedSuffix.length()) : text;
                if (tokenMatches && base == "ACK=AUTH_OK") {
                    console::field("Downlink", resp.c_str(), console::CYAN);
                    setScreen(Screen::Success);
                    // Deliberately does not call completeChallenge() here —
                    // submission stays separate from solving until FlagDesk.
                    console::ok("Authentication Accepted.");
                    {
                        // Obfuscated on-screen copy only; authoritative copy
                        // is a SHA-256 hash. Distinct key per level.
                        static const uint8_t kKey[] = { 0x62, 0xea, 0x1c, 0x97 };
                        static const uint8_t kFlagObfuscated[] = {
                            0x15, 0x83, 0x6e, 0xf2, 0x06, 0x91, 0x69, 0xe7, 0x0e, 0xdb, 0x72, 0xfc, 0x3d,
                            0x89, 0x2c, 0xfa, 0x0f, 0xde, 0x72, 0xf3, 0x3d, 0xde, 0x7f, 0xf4, 0x51, 0x9a,
                            0x2b, 0xa4, 0x06, 0x97,
                        };
                        char flag[sizeof(kFlagObfuscated) + 1];
                        flagreveal::decode(kFlagObfuscated, sizeof(kFlagObfuscated), kKey, sizeof(kKey), flag);
                        console::flagBlock("MISSION 03 COMPLETE", flag);
                    }
                    console::step("Submit this flag via Challenges > Submit Flag to unlock Mission 04.");
                } else if (tokenMatches && base == "ACK=AUTH_FAIL") {
                    // Explicit rejection: frame was heard but didn't match, so no point retransmitting.
                    console::field("Downlink", resp.c_str(), console::CYAN);
                    fail("Authentication failed - payload does not match.");
                    console::warn("Re-check the translated inscription and try again.");
                }
            }
        }
        if (s_screen == Screen::AwaitingResp && millis() - s_screenAt > kAckTimeoutMs) {
            if (s_attempt < kMaxUplinkAttempts) {
                s_attempt++;
                char buf[48];
                snprintf(buf, sizeof(buf), "No response - retransmitting (%d/%d)",
                         s_attempt, kMaxUplinkAttempts);
                console::step(buf);
                retransmit();
                s_screenAt = millis();
            } else {
                fail("No satellite response");
                console::warn("Check the satellite simulator is powered and in range.");
            }
        }
    }

    if (s_dirty) { s_dirty = false; draw(); }
    return false;
}

} // namespace mission3

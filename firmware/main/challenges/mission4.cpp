#include "mission4.h"
#include "challenge_data.h"
#include "challenge_engine.h"
#include "console.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../rf/lora.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"
#include "../audio/audio_manager.h"
#include "../leds/led_manager.h"
#include <Arduino.h>
#include <cstring>

namespace mission4 {

// Short, quiet tick per chunk (~43 per transfer) -- reads as "data flowing", not a notification.
static const audio::Note kSfxChunkTick[] = { { 2600, 6 } };

// Sized for the generated payload with headroom. Static, not heap-allocated
// (no allocation after boot convention).
static constexpr size_t kMaxPayload = 16384;
static constexpr int    kMaxChunks  = 256;

static uint8_t  s_payload[kMaxPayload];
static uint16_t s_chunkLen[kMaxChunks];   // bytes stored per chunk slot
static bool     s_chunkSeen[kMaxChunks];
static int      s_totalChunks = 0;
static int      s_seenCount = 0;
static size_t   s_payloadLen = 0;
static size_t   s_chunkOffset[kMaxChunks];

enum class Phase { Waiting, Receiving, Complete, Downloading };
static Phase s_phase;
static bool s_dirty;
static int  s_lastDrawnCount = -1;
static String s_line;

static void resetPayload() {
    memset(s_chunkSeen, 0, sizeof(s_chunkSeen));
    memset(s_chunkLen, 0, sizeof(s_chunkLen));
    memset(s_chunkOffset, 0, sizeof(s_chunkOffset));
    s_totalChunks = 0;
    s_seenCount = 0;
    s_payloadLen = 0;
}

void enter() {
    // Deliberately doesn't reset payload/phase: both are file-static and
    // survive screen re-entry, so progress isn't wiped. Only RESET or a fresh boot clears it.
    s_line = "";
    s_dirty = true;
    s_lastDrawnCount = -1;
    ui::widgets::clearScreen(display::tft());

    console::banner("MISSION 04 :: OPERATION VANGUARD",
                    "Classified payload downlink");
    if (s_totalChunks == 0) {
        s_phase = Phase::Waiting;
        console::info("Complete Level 3's uplink to start the payload transfer.");
    } else if (s_seenCount >= s_totalChunks) {
        s_phase = Phase::Complete;
        console::ok("Payload already received - run DOWNLOAD to retrieve it.");
    } else {
        s_phase = Phase::Receiving;
        char buf[64];
        snprintf(buf, sizeof(buf), "Resuming - %d / %d chunks received.", s_seenCount, s_totalChunks);
        console::info(buf);
    }
    console::step("Commands: STATUS | DOWNLOAD | RESET");
    if (Serial) Serial.println();
}

// ---------------------------------------------------------------- drawing
//
// Split into a full redraw (draw(), phase changes only) and a targeted
// counter redraw (drawCounter(), every chunk). Full clearScreen() on every
// chunk would be a visible full-screen flash (~43 SPI transfers/transfer).

// Sized generously enough for any count kMaxChunks (256) can produce.
static constexpr int16_t kCounterY = 135;
static constexpr int16_t kCounterH = 20;

static void drawCounter() {
    Adafruit_ST7789& tft = display::tft();
    char buf[40];
    snprintf(buf, sizeof(buf), "%d / %d", s_seenCount, s_totalChunks);
    tft.fillRect(0, kCounterY - 2, cfg::DISPLAY_WIDTH, kCounterH, theme::COLOR_BG);
    theme::drawCentered(tft, buf, kCounterY, theme::LIST_TEXT_SIZE, theme::COLOR_TEXT);
}

static void draw() {
    Adafruit_ST7789& tft = display::tft();
    ui::widgets::clearScreen(tft);
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH,
                                      (int16_t)theme::LIST_START_Y}, "Mission 04");
    switch (s_phase) {
        case Phase::Waiting:
            if (!rf::selfTest()) {
                // Distinguishes "radio never came up" from "nothing heard yet" (see mission2.cpp).
                theme::drawCentered(tft, "Radio Offline", 90, theme::LIST_TEXT_SIZE,
                                    theme::COLOR_DANGER);
                theme::drawCentered(tft, "Switch badge OFF/ON", 130, theme::BODY_TEXT_SIZE,
                                    theme::COLOR_TEXT);
            } else {
                theme::drawCentered(tft, "Waiting for", 90, theme::LIST_TEXT_SIZE,
                                    theme::COLOR_ACCENT);
                theme::drawCentered(tft, "Level 3 Uplink", 130, theme::LIST_TEXT_SIZE,
                                    theme::COLOR_ACCENT);
                theme::drawCentered(tft, "Payload streams automatically", 165,
                                    theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
                theme::drawCentered(tft, "once authenticated", 185,
                                    theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
            }
            break;
        case Phase::Receiving:
            theme::drawCentered(tft, "Receiving Payload", 80, theme::LIST_TEXT_SIZE,
                                theme::COLOR_ACCENT);
            drawCounter();
            break;
        case Phase::Complete:
            theme::drawCentered(tft, "Payload Complete", 80, theme::LIST_TEXT_SIZE,
                                theme::COLOR_ACCENT);
            theme::drawCentered(tft, "On your computer, run:", 130, theme::BODY_TEXT_SIZE,
                                theme::COLOR_TEXT);
            theme::drawCentered(tft, "python3 download.py", 155, theme::BODY_TEXT_SIZE,
                                theme::COLOR_ACCENT_DARK);
            break;
        case Phase::Downloading:
            theme::drawCentered(tft, "USB Connected", 90, theme::LIST_TEXT_SIZE,
                                theme::COLOR_ACCENT);
            theme::drawCentered(tft, "Downloading...", 135, theme::BODY_TEXT_SIZE,
                                theme::COLOR_TEXT);
            break;
    }
    theme::drawCentered(tft, "BACK to exit", 210, theme::BODY_TEXT_SIZE,
                        theme::COLOR_ACCENT_DARK);
}

// ------------------------------------------------------------ LoRa chunks

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Parses "VGDIMG:<idx>:<total>:<offset>:<hex>" and files the chunk away.
// Offset is explicit, not index*stride: the badge starts listening mid-
// stream, and an inferred stride from a short first chunk would corrupt everything.
static void handleChunk(const String& pkt) {
    if (!pkt.startsWith("VGDIMG:")) return;

    int p1 = pkt.indexOf(':', 7);
    int p2 = pkt.indexOf(':', p1 + 1);
    int p3 = pkt.indexOf(':', p2 + 1);
    if (p1 < 0 || p2 < 0 || p3 < 0) return;

    int idx    = pkt.substring(7, p1).toInt();
    int total  = pkt.substring(p1 + 1, p2).toInt();
    long offIn = pkt.substring(p2 + 1, p3).toInt();
    String hex = pkt.substring(p3 + 1);
    hex.trim();
    if (offIn < 0) return;

    if (total <= 0 || total > kMaxChunks) return;
    if (idx < 0 || idx >= total) return;
    if (hex.length() % 2 != 0) return;

    // First valid chunk defines the transfer; a changed total means the
    // satellite restarted with a different payload, so start over.
    if (s_totalChunks == 0) {
        s_totalChunks = total;
        s_phase = Phase::Receiving;
        s_dirty = true;
        // Stopped on BACK mid-transfer and on completion below, so it never leaks into another screen.
        led::playEffect(led::EffectId::PacketFlow, led::EffectParams{0, 0, 0, led::kMaskWhite});
        console::ok("Satellite acquired - payload transfer started.");
    } else if (s_totalChunks != total) {
        console::warn("Payload restarted by satellite - discarding partial transfer.");
        resetPayload();
        s_totalChunks = total;
    }

    if (s_chunkSeen[idx]) return; // duplicate from the satellite's loop

    size_t nbytes = hex.length() / 2;
    size_t off = (size_t)offIn;
    if (off + nbytes > kMaxPayload) {
        console::err("Payload larger than badge storage - aborting.");
        return;
    }

    for (size_t i = 0; i < nbytes; i++) {
        int hi = hexVal(hex[i * 2]), lo = hexVal(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return; // malformed; drop the whole chunk
        s_payload[off + i] = (uint8_t)((hi << 4) | lo);
    }

    s_chunkSeen[idx] = true;
    s_chunkLen[idx] = (uint16_t)nbytes;
    s_chunkOffset[idx] = off;
    s_seenCount++;
    if (off + nbytes > s_payloadLen) s_payloadLen = off + nbytes;
    audio::playSfx(kSfxChunkTick, 1);

    if (s_seenCount >= s_totalChunks) {
        s_phase = Phase::Complete;
        s_dirty = true; // phase change -> needs the full draw(), not just the counter
        led::stop(); // end the PacketFlow effect now that there's nothing left flowing
        console::ok("Payload transfer complete.");
        char buf[64];
        snprintf(buf, sizeof(buf), "%u bytes in %d chunks",
                 (unsigned)s_payloadLen, s_totalChunks);
        console::field("Received", buf, console::GREEN);
        console::info("On your computer, run python3 download.py");
        console::info("This saves payload.bin - decode the image to find the flag.");
        // Deliberately does not call completeChallenge() here — that still
        // requires submitting the flag found inside via FlagDesk.
    }
    // No unconditional s_dirty here: frame()'s count check already catches
    // per-chunk redraws via the lightweight drawCounter() path.
}

// ------------------------------------------------------------- USB serial

static void dumpPayload() {
    if (s_phase != Phase::Complete && s_seenCount == 0) {
        console::warn("No payload received yet.");
        return;
    }
    if (s_seenCount < s_totalChunks) {
        console::warn("Payload incomplete - download would be corrupt.");
        char buf[48];
        snprintf(buf, sizeof(buf), "%d / %d chunks", s_seenCount, s_totalChunks);
        console::field("Progress", buf, console::YELLOW);
        return;
    }

    // Bypasses console:: for the raw hex dump (no ANSI wanted), so needs its own blocking-write guard.
    if (!Serial) return;

    Phase prev = s_phase;
    s_phase = Phase::Downloading;
    draw();

    console::step("Transferring payload to Mission Control...");
    // BEGIN/END markers with byte count let the player's script capture the exact span.
    Serial.print("PAYLOAD_BEGIN ");
    Serial.println((unsigned)s_payloadLen);
    for (size_t i = 0; i < s_payloadLen; i++) {
        char b[3];
        snprintf(b, sizeof(b), "%02X", s_payload[i]);
        Serial.print(b);
        if ((i + 1) % 32 == 0) Serial.println();
    }
    if (s_payloadLen % 32 != 0) Serial.println();
    Serial.println("PAYLOAD_END");
    console::ok("Transfer complete - save as payload.bin and analyse it.");

    s_phase = prev;
    s_dirty = true;
}

static void handleCommand(String line) {
    line.trim();
    if (!line.length()) return;
    line.toUpperCase();

    if (line == "STATUS") {
        char buf[64];
        if (s_totalChunks == 0) {
            console::info("No transfer in progress - waiting for satellite.");
        } else {
            snprintf(buf, sizeof(buf), "%d / %d chunks", s_seenCount, s_totalChunks);
            console::field("Progress", buf,
                           s_seenCount >= s_totalChunks ? console::GREEN : console::YELLOW);
            snprintf(buf, sizeof(buf), "%u bytes", (unsigned)s_payloadLen);
            console::field("Payload", buf);
        }
    } else if (line == "DOWNLOAD") {
        dumpPayload();
    } else if (line == "RESET") {
        resetPayload();
        s_phase = Phase::Waiting;
        s_dirty = true;
        console::info("Payload buffer cleared - listening again.");
    } else {
        console::warn("Unknown command. Try STATUS, DOWNLOAD or RESET.");
    }
}

// ---------------------------------------------------------------- frame

bool frame() {
    if (input::wasPressed(input::Button::Back)) {
        // Stop the PacketFlow effect if mid-transfer; Complete already stopped it in handleChunk().
        if (s_phase == Phase::Receiving) led::stop();
        return true;
    }

    // Non-echoing: script-driven, same reasoning as mission3.
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            if (s_line.length()) { String l = s_line; s_line = ""; handleCommand(l); }
        } else if (s_line.length() < 64) {
            s_line += c;
        }
    }

    String pkt;
    if (rf::pollReceive(pkt)) handleChunk(pkt);

    // s_dirty means a full draw() is needed (phase change); a bare count
    // change during Receiving only needs drawCounter().
    if (s_dirty) {
        s_dirty = false;
        s_lastDrawnCount = s_seenCount;
        draw();
    } else if (s_phase == Phase::Receiving && s_seenCount != s_lastDrawnCount) {
        s_lastDrawnCount = s_seenCount;
        drawCounter();
    }
    return false;
}

} // namespace mission4

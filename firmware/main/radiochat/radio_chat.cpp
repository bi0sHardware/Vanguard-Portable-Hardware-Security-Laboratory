#include "radio_chat.h"
#include "text_entry.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../storage/storage.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"
#include "../leds/led_manager.h"
#include "../leds/led_chain.h"
#include "../audio/audio_manager.h"
#include "../audio/buzzer.h"
#include "../rf/radio_link.h"
#include "../glitch/glitch.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>

// Radio Chat -- field-radio app on radiolink: live peer discovery via RSSI,
// reliable delivery via sendReliable()/ACK.
//
// Uses dirty-flag/direct-draw (like snake.cpp) rather than the Region
// renderer -- screens here are driven by continuous radio state that fits
// "redraw when changed" better than diffing static row content.
namespace radiochat {

// ---------------------------------------------------------------------
// Quick message tables
// ---------------------------------------------------------------------

static const char* kQuickMessages[] = {
    "Hello!", "Hi!", "Good luck!", "GG!", "Ready?", "Thanks!",
    "Need help?", "Mission complete!", "Radio check.", "Copy that.",
    "Nice solve!", "See you.",
};
constexpr int kQuickMessageCount = sizeof(kQuickMessages) / sizeof(kQuickMessages[0]);
constexpr int kCustomMessageIndex = kQuickMessageCount; // "Custom..." row, one past the built-ins

// Pre-encoded fixed phrases (no generic encoder). "/" marks a word gap, a
// bare space marks a letter gap, matching manual keying's own output so
// decodeKnownMorse() can compare either source the same way.
struct MorsePhrase { const char* text; const char* morse; };
static const MorsePhrase kMorsePhrases[] = {
    { "HELLO",            ".... . .-.. .-.. ---" },
    { "READY",            ".-. . .- -.. -.--" },
    { "GOOD LUCK",        "--. --- --- -.. / .-.. ..- -.-. -.-" },
    { "GG",               "--. --." },
    { "THANK YOU",        "- .... .- -. -.- / -.-- --- ..-" },
    { "COPY THAT",        "-.-. --- .--. -.-- / - .... .- -" },
    { "RADIO CHECK",      ".-. .- -.. .. --- / -.-. .... . -.-. -.-" },
    { "MISSION COMPLETE", "-- .. ... ... .. --- -. / -.-. --- -- .--. .-.. . - ." },
};
constexpr int kMorsePhraseCount = sizeof(kMorsePhrases) / sizeof(kMorsePhrases[0]);

// Exact match only. Returns nullptr (caller shows raw dots/dashes) otherwise.
static const char* decodeKnownMorse(const String& morse) {
    for (int i = 0; i < kMorsePhraseCount; i++) {
        if (morse == kMorsePhrases[i].morse) return kMorsePhrases[i].text;
    }
    return nullptr;
}

// ---------------------------------------------------------------------
// Screen state machine
// ---------------------------------------------------------------------

enum class Screen {
    Startup,
    Home,
    ContactPicker,       // shared by both Quick Messages and Morse entry points
    QuickMessages,
    Sending,
    Received,            // incoming-message card, can interrupt any other screen
    MorseHome,
    MorseManual,
    MorseQuickMessages,
};

// Which flow the current ContactPicker/Sending visit belongs to -- Quick
// Messages and Morse share one picker screen rather than duplicating it.
enum class Flow { Text, Morse, Attack };

static Screen s_screen = Screen::Startup;
static Flow s_flow = Flow::Text;
static bool s_dirty = true;
static bool s_radioOffline = false;
static unsigned long s_screenAtMs = 0;

// Copied out of the live peer table, not held as a pointer/index -- peerCount()
// can reorder/drop entries as beacons age out; the send must keep its target.
static char s_peerLinkId[7] = {0};
static char s_peerName[17] = {0};

static int s_pickerSelected = 0;
// Tracks WHICH PEER (not which slot) is highlighted -- see contactPickerFrame().
static char s_pickerSelectedLinkId[7] = {0};
static int s_quickSelected = 0;
static int s_morseHomeSelected = 0;
static int s_morseQuickSelected = 0;

// ---- Sending screen ----
static uint8_t s_sendPayload[radiolink::kMaxPayload];
static uint8_t s_sendPayloadLen = 0;
static radiolink::Type s_sendType = radiolink::Type::TextMsg;
static bool s_sendComplete = false;
static bool s_sendAcked = false;
// Attack-only: true once the defender's AttackBlocked/AttackLanded reply
// resolved this send (see onMessage()) -- keeps onSendDone() from overwriting
// that with the generic transport ack.
static bool s_sendResultKnown = false;
static bool s_sendBlockedBySecure = false; // meaningful only when s_sendResultKnown
static bool s_txOnAir = false; // ticks via setTxIndicator, drives the pulse animation

// Real dot/dash light+buzzer playback while a MorseMsg is on the Sending
// screen, reusing manual keying's timing constants. Decoupled from the actual
// (faster) radio TX -- cut short if the real send resolves first (see
// sendingFrame()'s resultAnnounced block).
static bool s_sendMorseActive = false;
static int s_sendMorsePos = 0;
static bool s_sendMorseSounding = false;
static unsigned long s_sendMorseNextAtMs = 0;

// Same idea, receive side. Kept as separate state (not shared with send-side)
// so an in-flight receive playback can never interact with a send one.
static bool s_rxMorseActive = false;
static int s_rxMorsePos = 0;
static bool s_rxMorseSounding = false;
static unsigned long s_rxMorseNextAtMs = 0;

// Delta-redraw tracking for the Sending screen; declared here since beginSend()
// (defined earlier) resets it.
enum class SendingPhase { None, InFlight, Complete };
static SendingPhase s_sendingPhaseDrawn = SendingPhase::None;

// ---- Received card ----
static String s_rxSenderName;
static String s_rxText;      // TextMsg body, or MorseMsg's decoded meaning (only when s_rxDecoded)
static String s_rxMorseRaw;  // MorseMsg's actual dot/dash payload -- always shown for a MorseMsg
static bool s_rxWasMorse = false;
static bool s_rxDecoded = false; // only meaningful when s_rxWasMorse
static Screen s_screenBeforeRx = Screen::Home; // resume point after dismissing the card

// ---- Manual Morse keying ----
// Two dedicated buttons (Ok=dash, JoySelect=dot) rather than hold-duration:
// duration-based keying was error-prone to hit consistently on real hardware.
// Each symbol has a fixed duration (kMorseDotMs/kMorseDashMs) started on
// press and timed out on its own.
static String s_morseBuf;
static bool s_morseSounding = false;         // a fixed-duration symbol tone/LED is currently playing
static unsigned long s_morseSoundStartMs = 0;
static unsigned long s_morseSoundDurationMs = 0;
static unsigned long s_morseLastSymbolEndMs = 0;
static bool s_morseGapInserted = true; // true = no gap owed right now (buffer empty or gap already placed)
// Delta-redraw tracking for the Manual Morse screen; declared here since
// morseHomeFrame() (defined earlier) resets s_morseManualChromeDrawn on entry.
static bool s_morseManualChromeDrawn = false;
static String s_morseManualLastShown;
static bool s_morseManualLastSounding = false;
// 180ms/550ms: comfortably above the "just a blip" threshold while keeping
// the standard 3x dot/dash ratio. Both audio and LED key off the same
// s_morseSoundDurationMs, keeping sound and light in sync.
constexpr unsigned long kMorseDotMs           = 180;  // fixed tone/LED duration for a keyed dot
constexpr unsigned long kMorseDashMs          = 550;  // fixed tone/LED duration for a keyed dash (~3x dot, standard Morse ratio)
constexpr unsigned long kMorseLetterGapMs     = 600;  // pause >= this (below word gap) -> letter boundary
constexpr unsigned long kMorseWordGapMs       = 1400; // pause >= this -> word boundary (" / ")
constexpr unsigned long kMorseSendGapMs       = 2600; // pause >= this with a non-empty buffer -> finalize & send

// ---- Audio ----
static const audio::Note kSfxStartupChirp[] = { { 900, 40 }, { 1400, 50 } };
static const audio::Note kSfxTxChirp[]      = { { 1600, 30 }, { 0, 20 }, { 1600, 30 } };
static const audio::Note kSfxRxChirp[]      = { { 1200, 40 }, { 1800, 60 } };
static const audio::Note kSfxErrorBeep[]    = { { 300, 60 }, { 0, 40 }, { 300, 60 } };

// Screen-transition cue: a bright segment sweeps once along the header's
// rule line whenever the screen changes. Confined to that single pixel row
// so it can't interact with any screen's own delta-tracking.
constexpr unsigned long kTransitionMs = 220;
constexpr int kTransitionSegW = 40;
static bool s_transitionActive = false;
static unsigned long s_transitionStartMs = 0;
static int s_transitionLastX = -1;

static void startTransition() {
    s_transitionActive = true;
    s_transitionStartMs = millis();
    s_transitionLastX = -1;
}

// Called every tick regardless of s_dirty -- the sweep must keep animating
// even on ticks where the screen's own content didn't change.
static void drawTransitionSweep() {
    if (!s_transitionActive) return;
    auto& tft = display::tft();
    unsigned long elapsed = millis() - s_transitionStartMs;
    int travel = cfg::DISPLAY_WIDTH + kTransitionSegW;
    if (elapsed >= kTransitionMs) {
        if (s_transitionLastX >= 0) {
            tft.drawFastHLine(s_transitionLastX, theme::HEADER_RULE_Y, kTransitionSegW, theme::COLOR_ACCENT);
        }
        s_transitionActive = false;
        return;
    }
    int x = (int)((elapsed * travel) / kTransitionMs) - kTransitionSegW;
    if (x == s_transitionLastX) return;
    if (s_transitionLastX >= 0) {
        tft.drawFastHLine(s_transitionLastX, theme::HEADER_RULE_Y, kTransitionSegW, theme::COLOR_ACCENT);
    }
    int drawX = x < 0 ? 0 : x;
    int drawW = (x < 0) ? (kTransitionSegW + x) : kTransitionSegW;
    if (drawX + drawW > cfg::DISPLAY_WIDTH) drawW = cfg::DISPLAY_WIDTH - drawX;
    if (drawW > 0) tft.drawFastHLine(drawX, theme::HEADER_RULE_Y, drawW, ST77XX_WHITE);
    s_transitionLastX = x;
}

static void setScreen(Screen s) {
    if (s != s_screen) startTransition();
    s_screen = s;
    s_screenAtMs = millis();
    s_dirty = true;
}

// ---------------------------------------------------------------------
// radiolink callbacks
// ---------------------------------------------------------------------

static void onSendDone(bool acked, uint8_t /*seq*/, void* /*ctx*/) {
    // For Attack, don't finalize if the defender's AttackBlocked/AttackLanded
    // reply (see onMessage()) already did -- that's a strictly better answer
    // than the generic ack, which can't distinguish glitched from defended.
    if (s_sendType == radiolink::Type::Attack && s_sendResultKnown) return;
    s_sendComplete = true;
    s_sendAcked = acked;
    s_dirty = true;
}

// Fires every tick while radiolink is active (level, not edge -- see
// radio_link.cpp's update()); led::playEffect() resets phase/start time each
// call, so calling it unconditionally would restart the pulse every tick.
static void onTxIndicator(bool onAir, void* /*ctx*/) {
    // Fires for every packet including the periodic background beacon on
    // every screen, not just user sends. Scoped to Sending only, or a white
    // LED ripple + dirty flag would fire from background beacon traffic
    // regardless of what the player was doing.
    if (s_screen != Screen::Sending) {
        s_txOnAir = onAir;
        return;
    }
    // Skip the generic ripple for a Morse send -- the LED belongs to the real
    // dot/dash playback (s_sendMorseActive) instead; running both would fight.
    if (!s_sendMorseActive) {
        if (onAir && !s_txOnAir) {
            led::playEffect(led::EffectId::ConnectionPulse, led::EffectParams{0, 0, 220, led::kMaskWhite});
            s_dirty = true;
        } else if (!onAir && s_txOnAir) {
            led::stop();
            s_dirty = true;
        }
    }
    s_txOnAir = onAir;
}

// Fires for any validated, non-duplicate message addressed to us or
// broadcast, from any screen. Only TextMsg/MorseMsg are shown; Ping/Bye are
// link-housekeeping.
static void onMessage(const radiolink::Message& m, void* /*ctx*/) {
    // Badge Attack (main/glitch/glitch.h), checked before the chat filter.
    // Defender's side: reply with the exact outcome -- the generic ack alone
    // can't distinguish "glitched" from "defended".
    if (m.type == radiolink::Type::Attack) {
        if (storage::isSecureBadgeEnabled()) {
            radiolink::sendUnreliable(m.src, radiolink::Type::AttackBlocked, 0, nullptr, 0);
        } else {
            radiolink::sendUnreliable(m.src, radiolink::Type::AttackLanded, 0, nullptr, 0);
            glitch::trigger();
        }
        return;
    }
    // Attacker's side: reply to an Attack WE sent. Ignore stray/duplicate
    // replies once we've moved on; s_sendResultKnown gates onSendDone() from
    // clobbering whichever outcome gets here first.
    if (m.type == radiolink::Type::AttackBlocked || m.type == radiolink::Type::AttackLanded) {
        if (s_screen == Screen::Sending && s_sendType == radiolink::Type::Attack &&
            !s_sendResultKnown && strncmp(m.src, s_peerLinkId, 6) == 0) {
            s_sendResultKnown = true;
            s_sendBlockedBySecure = (m.type == radiolink::Type::AttackBlocked);
            // Only a confirmed, undefended landing counts toward Secure Badge unlock.
            if (!s_sendBlockedBySecure) storage::setHasAttackedSomeone();
            s_sendComplete = true;
            s_sendAcked = true; // it was genuinely delivered either way
            s_dirty = true;
        }
        return;
    }
    if (m.type != radiolink::Type::TextMsg && m.type != radiolink::Type::MorseMsg) return;

    // Don't clobber an already-open Received card if two messages land close
    // together -- the second is simply missed. No inbox/history by design.
    if (s_screen == Screen::Received) return;

    const radiolink::Peer* p = radiolink::findPeer(m.src);
    s_rxSenderName = (p && p->name[0]) ? p->name : (String("Badge ") + m.src);

    if (m.type == radiolink::Type::MorseMsg) {
        s_rxWasMorse = true;
        s_rxMorseRaw = String((const char*)m.payload, m.payloadLen);
        const char* decoded = decodeKnownMorse(s_rxMorseRaw);
        s_rxDecoded = (decoded != nullptr);
        s_rxText = decoded ? decoded : "";
    } else {
        s_rxWasMorse = false;
        s_rxDecoded = false;
        s_rxText = String((const char*)m.payload, m.payloadLen);
    }

    s_screenBeforeRx = s_screen;
    // Arrival chirp either way; a MorseMsg plays its own dot/dash pattern
    // (tickReceiveMorse()) instead of the generic white double-blink.
    audio::playSfx(kSfxRxChirp, 2);
    if (m.type == radiolink::Type::MorseMsg) {
        led::stop(); // quiesce the effect system -- tickReceiveMorse() drives the chain directly
        s_rxMorseActive = true;
        s_rxMorsePos = 0;
        s_rxMorseSounding = false;
        s_rxMorseNextAtMs = millis();
    } else {
        led::playEffect(led::EffectId::Notification, led::EffectParams{0, 0, 150, led::kMaskWhite});
    }
    setScreen(Screen::Received);
}

// ---------------------------------------------------------------------
// Startup animation (~1s: antenna/signal-bars + white LED scan + chirp)
// ---------------------------------------------------------------------

constexpr unsigned long kStartupMs = 900; // ceiling, not a target -- entered/exited often

// Delta-tracked, not a full redraw every call, to avoid a flash/strobe over
// the ~900ms window: chrome draws once, other elements only redraw on change.
static bool s_startupChromeDrawn = false;
static bool s_startupLastBlink = false;
static int s_startupLastBars = -1;
static int s_startupLastLabel = -1; // -1 = not drawn yet, 0 = "Initializing...", 1 = "Searching..."

static void drawStartup(unsigned long elapsed) {
    auto& tft = display::tft();
    int cx = cfg::DISPLAY_WIDTH / 2, topY = 100;

    if (!s_startupChromeDrawn) {
        tft.fillScreen(ST77XX_BLACK);
        theme::drawCentered(tft, "RADIO CHAT", 60, theme::HEADER_TEXT_SIZE, ST77XX_WHITE);
        for (int i = 0; i < 5; i++) {
            int bw = 8, gap = 4;
            int bx = cx - (5 * (bw + gap)) / 2 + i * (bw + gap);
            tft.drawRect(bx, 175 - 30, bw, 30, theme::COLOR_ACCENT);
        }
        s_startupChromeDrawn = true;
    }

    // Antenna icon blink -- redraw only its own bounding box, on phase flip.
    bool blinkOn = (elapsed / 150) % 2 == 0;
    if (blinkOn != s_startupLastBlink || s_startupLastBars < 0) {
        uint16_t antColor = blinkOn ? ST77XX_WHITE : theme::COLOR_ACCENT;
        tft.fillRect(cx - 16, topY - 8, 32, 40, ST77XX_BLACK);
        tft.drawFastVLine(cx, topY, 30, antColor);
        tft.drawLine(cx, topY, cx - 14, topY + 16, antColor);
        tft.drawLine(cx, topY, cx + 14, topY + 16, antColor);
        tft.fillCircle(cx, topY - 4, 3, antColor);
        s_startupLastBlink = blinkOn;
    }

    // Only paint newly-lit bars, never repaint ones already filled.
    int barsLit = (int)((elapsed * 5) / kStartupMs);
    if (barsLit > 5) barsLit = 5;
    if (barsLit != s_startupLastBars) {
        int from = s_startupLastBars < 0 ? 0 : s_startupLastBars;
        for (int i = from; i < barsLit; i++) {
            int bw = 8, gap = 4;
            int bx = cx - (5 * (bw + gap)) / 2 + i * (bw + gap);
            int bh = 6 + i * 5;
            int by = 175 - bh;
            tft.fillRect(bx, by, bw, bh, ST77XX_WHITE);
        }
        s_startupLastBars = barsLit;
    }

    int labelIdx = elapsed < kStartupMs / 2 ? 0 : 1;
    if (labelIdx != s_startupLastLabel) {
        tft.fillRect(0, 192, cfg::DISPLAY_WIDTH, 14, ST77XX_BLACK);
        theme::drawCentered(tft, labelIdx == 0 ? "Initializing..." : "Searching...", 200,
                            theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT);
        s_startupLastLabel = labelIdx;
    }
}

static void enterStartup() {
    setScreen(Screen::Startup);
    s_startupChromeDrawn = false;
    s_startupLastBlink = false;
    s_startupLastBars = -1;
    s_startupLastLabel = -1;
    led::playEffect(led::EffectId::Sweep, led::EffectParams{0, 0, 60, led::kMaskWhite});
    audio::playSfx(kSfxStartupChirp, 2);
}

// ---------------------------------------------------------------------
// Home
// ---------------------------------------------------------------------

static const char* kHomeItems[] = { "Quick Messages", "Morse (CW)", "Attack", "Back" };
constexpr int kHomeCount = 4;
static int s_homeSelected = 0;

// Chrome draws once; the list only redraws the row that lost and the row
// that gained selection, not a full clearScreen()+redraw per key press.
constexpr int16_t kHomeListTop = (int16_t)(theme::LIST_START_Y + 6 + (theme::BODY_LINE_HEIGHT + 2) +
                                            (theme::BODY_LINE_HEIGHT + 6) + 10);
static bool s_homeChromeDrawn = false;
static int s_homeLastSelected = -1;
// Chrome-drawn flags for other list screens, declared here since they're
// reset from earlier functions (contactPickerFrame()/morseHomeFrame()).
static bool s_quickChromeDrawn = false;
static bool s_morseHomeChromeDrawn = false;
static bool s_morseQuickChromeDrawn = false;

static void drawHomeRow(Adafruit_ST7789& tft, int i, int16_t y) {
    ui::Rect row{0, y, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_ITEM_HEIGHT};
    tft.fillRect(row.x, row.y, row.w, row.h, theme::COLOR_BG);
    ui::widgets::listItem(tft, row, i, kHomeItems[i], i == s_homeSelected);
}

static void drawHome() {
    auto& tft = display::tft();
    bool fresh = !s_homeChromeDrawn;
    if (fresh) {
        ui::widgets::clearScreen(tft);
        ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y},
                            "Radio Chat");

        int y = theme::LIST_START_Y + 6;
        tft.setTextSize(theme::BODY_TEXT_SIZE);
        tft.setTextColor(theme::COLOR_ACCENT_DARK);
        tft.setCursor(theme::MARGIN_X, y);
        tft.print("Status");
        tft.setTextColor(theme::COLOR_SUCCESS);
        tft.setCursor(theme::MARGIN_X + 70, y);
        tft.print(s_radioOffline ? "OFFLINE" : "READY");
        y += theme::BODY_LINE_HEIGHT + 2;

        tft.setTextColor(theme::COLOR_TEXT);
        tft.setCursor(theme::MARGIN_X, y);
        tft.print("Channel 07");
        y += theme::BODY_LINE_HEIGHT + 6;

        tft.drawFastHLine(theme::MARGIN_X, y, cfg::DISPLAY_WIDTH - 2 * theme::MARGIN_X, theme::COLOR_ACCENT);
        s_homeChromeDrawn = true;
        s_homeLastSelected = -1;
    }

    for (int i = 0; i < kHomeCount; i++) {
        // On a fresh chrome draw every row must paint regardless of selection,
        // or rows other than the initial one stay blank until navigated onto.
        if (fresh || i == s_homeSelected || i == s_homeLastSelected) {
            drawHomeRow(tft, i, (int16_t)(kHomeListTop + i * theme::LIST_ITEM_HEIGHT));
        }
    }
    s_homeLastSelected = s_homeSelected;
}

static void enterHome() {
    setScreen(Screen::Home);
    s_homeSelected = 0;
    s_homeChromeDrawn = false;
    // Idle: slow beaconing (fast only while a picker is open).
    radiolink::setBeaconEnabled(true);
    radiolink::setBeaconIntervalMs(18000);
    // enterStartup()'s Sweep effect runs until stop() is called explicitly --
    // must be stopped here or it'd keep animating past the startup phase.
    led::stop();
}

// Declared here since homeFrame() (defined before drawContactPicker()) must
// call enterContactPicker() -- resetting s_pickerChromeDrawn, or a stale
// true would skip the one-time chrome paint and rows would draw over Home.
static bool s_pickerChromeDrawn = false;
static unsigned long s_pickerLastUpdateMs = 0;

static void enterContactPicker() {
    // Fast beaconing while the picker is open, restored to idle on leaving.
    radiolink::setBeaconIntervalMs(5000);
    s_pickerChromeDrawn = false;
    s_pickerLastUpdateMs = 0;
    s_pickerSelectedLinkId[0] = 0; // start at top of list, not stuck on a stale peer
}

// Returns true if the player wants to exit the whole app.
static bool homeFrame() {
    if (input::wasPressed(input::Button::JoyUp)) { s_homeSelected = (s_homeSelected - 1 + kHomeCount) % kHomeCount; ui::widgets::navSfx(); s_dirty = true; }
    if (input::wasPressed(input::Button::JoyDown)) { s_homeSelected = (s_homeSelected + 1) % kHomeCount; ui::widgets::navSfx(); s_dirty = true; }

    bool confirm = input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::JoySelect);
    if (confirm) {
        if (s_homeSelected == 0) { s_flow = Flow::Text; s_pickerSelected = 0; radiolink::requestScan(); enterContactPicker(); setScreen(Screen::ContactPicker); }
        else if (s_homeSelected == 1) { s_flow = Flow::Morse; s_pickerSelected = 0; radiolink::requestScan(); enterContactPicker(); setScreen(Screen::ContactPicker); }
        else if (s_homeSelected == 2) { s_flow = Flow::Attack; s_pickerSelected = 0; radiolink::requestScan(); enterContactPicker(); setScreen(Screen::ContactPicker); }
        else if (s_homeSelected == 3) return true; // "Back" row
    }
    return false;
}

// Declared here (ahead of quickMessagesFrame()) since contactPickerFrame()
// must call it directly for the Attack flow, which has no message to compose.
static void beginSend(radiolink::Type type, const uint8_t* payload, uint8_t len) {
    s_sendType = type;
    s_sendPayloadLen = len > radiolink::kMaxPayload ? radiolink::kMaxPayload : len;
    if (payload && s_sendPayloadLen > 0) memcpy(s_sendPayload, payload, s_sendPayloadLen); // Attack sends a null/zero-length payload
    s_sendComplete = false;
    s_sendAcked = false;
    s_sendResultKnown = false;
    s_sendBlockedBySecure = false;
    s_txOnAir = false;
    s_sendingPhaseDrawn = SendingPhase::None;
    // Quiesce the effect system so led::update() can't fight the playback's
    // direct raw writes for the whole Morse-send window.
    s_sendMorseActive = (type == radiolink::Type::MorseMsg);
    if (s_sendMorseActive) {
        s_sendMorsePos = 0;
        s_sendMorseSounding = false;
        s_sendMorseNextAtMs = millis();
        led::stop();
    }
    radiolink::SendResult r = radiolink::sendReliable(s_peerLinkId, type, 0, s_sendPayload,
                                                      s_sendPayloadLen, onSendDone, nullptr);
    if (r != radiolink::SendResult::Sent) {
        // Couldn't start (radio busy) -- report immediately rather than
        // sitting on a "Sending..." screen that will never resolve.
        s_sendComplete = true;
        s_sendAcked = false;
    }
    setScreen(Screen::Sending);
}

// ---------------------------------------------------------------------
// Contact picker (shared by Text and Morse flows)
// ---------------------------------------------------------------------

// Delta-tracked against a per-row snapshot rather than full redraw every
// tick, since RSSI updates continuously. Each row slot only repaints when
// its own content differs from what's on screen (hand-rolled since this
// screen isn't Region-based).
struct PickerRowSnapshot { bool valid; bool selected; bool known; int16_t rssi; char name[17]; };
// A 7th row would overlap the footer hint text (drawn once, never redrawn),
// so an RSSI/selection update to it would permanently paint over the footer.
// 6 rows fit cleanly with margin to spare.
constexpr int kPickerVisibleRows = 6;
static PickerRowSnapshot s_pickerRowSnap[kPickerVisibleRows];
static bool s_pickerEmptyShown = false; // whether the "Searching.../No badges" state is currently on screen
constexpr unsigned long kPickerUpdateIntervalMs = 200; // throttles RSSI-driven redraws to a readable rate

// RSSI jitters +/-1-2dBm between samples even for a stationary peer; a
// deadband (compared against the last *displayed* value) avoids redrawing
// every row on every poll from jitter alone.
constexpr int16_t kPickerRssiDeadbandDbm = 3;

static void drawContactPickerRow(Adafruit_ST7789& tft, int i, int y) {
    int count = radiolink::peerCount();
    bool valid = i < count && i < kPickerVisibleRows;
    PickerRowSnapshot cur{};
    cur.valid = valid;
    if (valid) {
        const radiolink::Peer& p = radiolink::peerAt(i);
        cur.selected = (i == s_pickerSelected);
        cur.known = p.knownContact;
        cur.rssi = p.rssiDbm;
        strncpy(cur.name, p.name[0] ? p.name : p.linkId, sizeof(cur.name) - 1);
    }
    PickerRowSnapshot& snap = s_pickerRowSnap[i];
    int16_t rssiDelta = cur.rssi - snap.rssi;
    bool rssiChanged = rssiDelta >= kPickerRssiDeadbandDbm || rssiDelta <= -kPickerRssiDeadbandDbm;
    bool changed = (cur.valid != snap.valid) ||
                   (cur.valid && (cur.selected != snap.selected || cur.known != snap.known ||
                                  rssiChanged || strcmp(cur.name, snap.name) != 0));
    if (!changed) return;
    if (cur.valid && !rssiChanged) cur.rssi = snap.rssi; // redrawing for another reason -- keep last displayed RSSI

    ui::Rect row{0, (int16_t)y, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_ITEM_HEIGHT};
    if (!cur.valid) {
        tft.fillRect(row.x, row.y, row.w, row.h, theme::COLOR_BG);
    } else {
        char label[40];
        snprintf(label, sizeof(label), "%s%s", cur.known ? "* " : "  ", cur.name);
        char value[16];
        snprintf(value, sizeof(value), "%ddBm", cur.rssi);
        // listItem() only clears background for a selected row; a normal row
        // draws text straight over old content. Must clear by hand here since
        // this screen isn't Region-based, or RSSI digit changes garble.
        tft.fillRect(row.x, row.y, row.w, row.h, theme::COLOR_BG);
        ui::widgets::listItem(tft, row, i, label, cur.selected, value);
    }
    snap = cur;
}

static void drawContactPicker() {
    auto& tft = display::tft();
    if (!s_pickerChromeDrawn) {
        ui::widgets::clearScreen(tft);
        ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y},
                            "Select Contact");
        theme::drawCentered(tft, "OK=select  BACK=cancel", cfg::DISPLAY_HEIGHT - 16,
                            theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
        for (auto& s : s_pickerRowSnap) s = PickerRowSnapshot{};
        s_pickerEmptyShown = false;
        s_pickerChromeDrawn = true;
    }

    int count = radiolink::peerCount();
    if (count == 0) {
        if (!s_pickerEmptyShown) {
            tft.fillRect(0, theme::LIST_START_Y, cfg::DISPLAY_WIDTH, cfg::DISPLAY_HEIGHT - theme::LIST_START_Y, theme::COLOR_BG);
            theme::drawCentered(tft, "Searching...", 110, theme::HEADER_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
            theme::drawCentered(tft, "No badges in range yet", 140, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
            s_pickerEmptyShown = true;
        }
        return;
    }
    if (s_pickerEmptyShown) {
        // Coming back from empty state -- clear the text before diffing rows.
        tft.fillRect(0, theme::LIST_START_Y, cfg::DISPLAY_WIDTH, cfg::DISPLAY_HEIGHT - theme::LIST_START_Y, theme::COLOR_BG);
        for (auto& s : s_pickerRowSnap) s = PickerRowSnapshot{};
        s_pickerEmptyShown = false;
    }

    if (s_pickerSelected >= count) s_pickerSelected = count - 1;
    int y = theme::LIST_START_Y + 4;
    for (int i = 0; i < kPickerVisibleRows; i++) {
        drawContactPickerRow(tft, i, y);
        y += theme::LIST_ITEM_HEIGHT;
    }
}

static void contactPickerFrame() {
    int count = radiolink::peerCount(); // re-sorts s_peers by live RSSI on every call -- see below
    int visible = count < kPickerVisibleRows ? count : kPickerVisibleRows; // no scrolling yet -- clamp to what's shown

    // peerCount() re-sorts its table by live RSSI every call, so a bare index
    // could silently point at a different peer between navigating to it and
    // pressing OK. Track the selected peer's stable linkId and re-locate its
    // current index every tick instead.
    if (visible > 0) {
        int found = -1;
        for (int i = 0; i < visible; i++) {
            if (strncmp(radiolink::peerAt(i).linkId, s_pickerSelectedLinkId, 6) == 0) { found = i; break; }
        }
        s_pickerSelected = (found >= 0) ? found : 0;
        strncpy(s_pickerSelectedLinkId, radiolink::peerAt(s_pickerSelected).linkId, 6);
        s_pickerSelectedLinkId[6] = 0;

        if (input::wasPressed(input::Button::JoyUp)) {
            s_pickerSelected = (s_pickerSelected - 1 + visible) % visible;
            strncpy(s_pickerSelectedLinkId, radiolink::peerAt(s_pickerSelected).linkId, 6);
            s_pickerSelectedLinkId[6] = 0;
            ui::widgets::navSfx(); s_dirty = true;
        }
        if (input::wasPressed(input::Button::JoyDown)) {
            s_pickerSelected = (s_pickerSelected + 1) % visible;
            strncpy(s_pickerSelectedLinkId, radiolink::peerAt(s_pickerSelected).linkId, 6);
            s_pickerSelectedLinkId[6] = 0;
            ui::widgets::navSfx(); s_dirty = true;
        }
    }
    // Live-updating but throttled; drawContactPicker() is delta-tracked per row.
    if (millis() - s_pickerLastUpdateMs >= kPickerUpdateIntervalMs) {
        s_pickerLastUpdateMs = millis();
        s_dirty = true;
    }

    bool confirm = input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::JoySelect);
    if (confirm && visible > 0) {
        const radiolink::Peer& p = radiolink::peerAt(s_pickerSelected);
        strncpy(s_peerLinkId, p.linkId, sizeof(s_peerLinkId) - 1);
        s_peerLinkId[sizeof(s_peerLinkId) - 1] = 0;
        strncpy(s_peerName, p.name[0] ? p.name : p.linkId, sizeof(s_peerName) - 1);
        s_peerName[sizeof(s_peerName) - 1] = 0;
        radiolink::setBeaconIntervalMs(18000); // leaving the picker -- back to idle pacing
        if (s_flow == Flow::Text) {
            s_quickSelected = 0;
            s_quickChromeDrawn = false;
            setScreen(Screen::QuickMessages);
        } else if (s_flow == Flow::Morse) {
            s_morseHomeSelected = 0;
            s_morseHomeChromeDrawn = false;
            setScreen(Screen::MorseHome);
        } else {
            // Attack: no message to compose, fire immediately.
            beginSend(radiolink::Type::Attack, nullptr, 0);
        }
    }
}

// ---------------------------------------------------------------------
// Quick Messages (+ Custom via text_entry)
// ---------------------------------------------------------------------

static bool s_customEntryOpen = false;

// 6 visible rows, scrolled to keep selection in view (13 total entries won't
// fit). Chrome draws once; rows fully redraw only when the scroll window
// shifts, otherwise just the row that lost/gained selection.
constexpr int kQuickVisible = 6;
constexpr int16_t kQuickListTop = (int16_t)(theme::LIST_START_Y + 18);
static int s_quickLastTop = -1;
static int s_quickLastSelected = -1;

static void drawQuickRow(Adafruit_ST7789& tft, int i, int16_t y) {
    const char* label = (i == kCustomMessageIndex) ? "Custom..." : kQuickMessages[i];
    ui::Rect row{0, y, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_ITEM_HEIGHT};
    tft.fillRect(row.x, row.y, row.w, row.h, theme::COLOR_BG);
    ui::widgets::listItem(tft, row, i, label, i == s_quickSelected);
}

static void drawQuickMessages() {
    auto& tft = display::tft();
    int total = kQuickMessageCount + 1;
    int top = s_quickSelected - kQuickVisible / 2;
    if (top < 0) top = 0;
    if (top > total - kQuickVisible) top = total > kQuickVisible ? total - kQuickVisible : 0;

    if (!s_quickChromeDrawn) {
        ui::widgets::clearScreen(tft);
        ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y},
                            "Quick Messages");
        char sub[40];
        snprintf(sub, sizeof(sub), "To: %s", s_peerName);
        tft.setTextSize(theme::BODY_TEXT_SIZE);
        tft.setTextColor(theme::COLOR_ACCENT_DARK);
        tft.setCursor(theme::MARGIN_X, theme::LIST_START_Y + 2);
        tft.print(sub);
        theme::drawCentered(tft, "OK=send  BACK=cancel", cfg::DISPLAY_HEIGHT - 16,
                            theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
        s_quickChromeDrawn = true;
        s_quickLastTop = -1;
        s_quickLastSelected = -1;
    }

    if (top != s_quickLastTop) {
        for (int slot = 0; slot < kQuickVisible; slot++) {
            int i = top + slot;
            int16_t y = (int16_t)(kQuickListTop + slot * theme::LIST_ITEM_HEIGHT);
            if (i < total) drawQuickRow(tft, i, y);
        }
        s_quickLastTop = top;
    } else {
        for (int slot = 0; slot < kQuickVisible; slot++) {
            int i = top + slot;
            if (i >= total) continue;
            if (i == s_quickSelected || i == s_quickLastSelected) {
                drawQuickRow(tft, i, (int16_t)(kQuickListTop + slot * theme::LIST_ITEM_HEIGHT));
            }
        }
    }
    s_quickLastSelected = s_quickSelected;
}

static void quickMessagesFrame() {
    int total = kQuickMessageCount + 1;
    if (input::wasPressed(input::Button::JoyUp)) { s_quickSelected = (s_quickSelected - 1 + total) % total; ui::widgets::navSfx(); s_dirty = true; }
    if (input::wasPressed(input::Button::JoyDown)) { s_quickSelected = (s_quickSelected + 1) % total; ui::widgets::navSfx(); s_dirty = true; }

    bool confirm = input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::JoySelect);
    if (confirm) {
        if (s_quickSelected == kCustomMessageIndex) {
            text_entry::enter("Custom Message", "", 24);
            s_customEntryOpen = true;
        } else {
            const char* text = kQuickMessages[s_quickSelected];
            beginSend(radiolink::Type::TextMsg, (const uint8_t*)text, (uint8_t)strlen(text));
        }
    }
}

// ---------------------------------------------------------------------
// Sending (driven by setTxIndicator -- genuinely animated, TX is
// non-blocking at SF7 on the badge channel)
// ---------------------------------------------------------------------

// Chrome draws once when the in-flight phase begins; only the bar fill and
// label redraw per tick, and the result screen draws its own chrome once on
// the phase transition -- avoids a full redraw (and visible flash) per tick.
static bool s_sendingLastTxOnAir = false;
static int s_sendingLastFillW = -1;
constexpr int kSendingBarW = 160, kSendingBarH = 14;
constexpr int kSendingBarX = (cfg::DISPLAY_WIDTH - kSendingBarW) / 2, kSendingBarY = 130;

static void drawSending() {
    auto& tft = display::tft();

    if (!s_sendComplete) {
        if (s_sendingPhaseDrawn != SendingPhase::InFlight) {
            ui::widgets::clearScreen(tft);
            ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y},
                                s_sendType == radiolink::Type::MorseMsg ? "Morse TX" :
                                s_sendType == radiolink::Type::Attack ? "Attack" : "Sending");
            theme::drawCentered(tft, s_sendType == radiolink::Type::Attack ? "Attacking..." : "Sending...",
                                90, theme::HEADER_TEXT_SIZE, theme::COLOR_TEXT);
            tft.drawRect(kSendingBarX, kSendingBarY, kSendingBarW, kSendingBarH, theme::COLOR_ACCENT_DARK);
            s_sendingPhaseDrawn = SendingPhase::InFlight;
            s_sendingLastFillW = -1;
            s_sendingLastTxOnAir = !s_txOnAir; // force the label below to draw on the first pass
        }
        int fillW = (int)((millis() - s_screenAtMs) % 1000) * (kSendingBarW - 4) / 1000;
        if (fillW != s_sendingLastFillW || s_txOnAir != s_sendingLastTxOnAir) {
            tft.fillRect(kSendingBarX + 2, kSendingBarY + 2, kSendingBarW - 4, kSendingBarH - 4, theme::COLOR_BG);
            tft.fillRect(kSendingBarX + 2, kSendingBarY + 2, fillW, kSendingBarH - 4,
                        s_txOnAir ? ST77XX_WHITE : theme::COLOR_ACCENT);
            s_sendingLastFillW = fillW;
        }
        if (s_txOnAir != s_sendingLastTxOnAir) {
            tft.fillRect(0, 154, cfg::DISPLAY_WIDTH, 14, theme::COLOR_BG);
            theme::drawCentered(tft, s_txOnAir ? "TRANSMITTING" : "Awaiting ACK...", 160,
                                theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
            s_sendingLastTxOnAir = s_txOnAir;
        }
    } else if (s_sendingPhaseDrawn != SendingPhase::Complete) {
        ui::widgets::clearScreen(tft);
        ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y},
                            s_sendType == radiolink::Type::MorseMsg ? "Morse TX" :
                            s_sendType == radiolink::Type::Attack ? "Attack" : "Sending");
        const char* resultText;
        uint16_t resultColor;
        if (s_sendType == radiolink::Type::Attack && s_sendResultKnown && s_sendBlockedBySecure) {
            resultText = "Target Secure!";
            resultColor = theme::COLOR_DANGER; // attack did not succeed
        } else if (s_sendAcked) {
            resultText = (s_sendType == radiolink::Type::Attack) ? "Attack Landed" : "Delivered";
            resultColor = theme::COLOR_SUCCESS;
        } else {
            resultText = "No Response";
            resultColor = theme::COLOR_DANGER;
        }
        theme::drawCentered(tft, resultText, 100, theme::HEADER_TEXT_SIZE, resultColor);
        theme::drawCentered(tft, "OK to continue", 150, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
        s_sendingPhaseDrawn = SendingPhase::Complete;
    }
}

// Plays s_sendPayload (encoded dots/dashes/spaces/'/' word-gaps) through the
// LED+buzzer, one tick at a time, driven by the string instead of button
// presses. kMorseDotMs doubles as the inter-symbol gap within a letter.
static void tickSendMorse() {
    if (!s_sendMorseActive) return;
    unsigned long now = millis();
    if (s_sendMorseSounding) {
        if (now < s_sendMorseNextAtMs) return;
        buzzer::stopAsync();
        leds::clearAll();
        s_sendMorseSounding = false;
        s_sendMorsePos++;
        s_sendMorseNextAtMs = now + kMorseDotMs; // inter-symbol gap
        return;
    }
    if (now < s_sendMorseNextAtMs) return;
    if (s_sendMorsePos >= s_sendPayloadLen) { s_sendMorseActive = false; return; }
    char c = (char)s_sendPayload[s_sendMorsePos];
    if (c == '.' || c == '-') {
        unsigned long dur = (c == '-') ? kMorseDashMs : kMorseDotMs;
        if (audio::isSoundEnabled()) buzzer::toneAsync(700);
        leds::writeChainRaw(led::kMaskWhite);
        s_sendMorseSounding = true;
        s_sendMorseNextAtMs = now + dur;
    } else if (c == '/') {
        s_sendMorsePos++;
        s_sendMorseNextAtMs = now + kMorseWordGapMs;
    } else { // space (or any other separator) -> letter gap
        s_sendMorsePos++;
        s_sendMorseNextAtMs = now + kMorseLetterGapMs;
    }
}

static void sendingFrame() {
    // TX ripple is started/stopped by onTxIndicator()'s own edge detection,
    // not here -- see that function's comment.
    s_dirty = true; // drawSending() itself is now delta-tracked, so ticking it every frame is cheap

    if (!s_sendComplete) tickSendMorse();

    if (s_sendComplete) {
        static bool resultAnnounced = false;
        if (!resultAnnounced) {
            resultAnnounced = true;
            led::stop();
            // Cut Morse playback short if the real send resolved first --
            // a stray tone/light after the result screen would read as a glitch.
            if (s_sendMorseActive) {
                s_sendMorseActive = false;
                buzzer::stopAsync();
            }
            if (s_sendAcked) {
                led::playEffect(led::EffectId::Notification, led::EffectParams{0, 0, 120, led::kMaskWhite});
            } else {
                audio::playSfx(kSfxErrorBeep, 2);
            }
        }
        bool dismiss = input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::Back) ||
                       (millis() - s_screenAtMs > 3000); // auto-return so a missed OK doesn't strand the player here
        if (dismiss) {
            resultAnnounced = false;
            led::stop();
            if (s_flow == Flow::Morse) { s_morseHomeChromeDrawn = false; setScreen(Screen::MorseHome); }
            else enterHome();
        }
    }
}

// ---------------------------------------------------------------------
// Received card (interrupts any screen; OK returns to where we were, or
// Home if that screen no longer makes sense to resume)
// ---------------------------------------------------------------------

// Word-wraps `text` centered across up to maxLines lines. Used for both a
// plain quoted message and a raw Morse string (which can run much longer).
// Returns the number of lines actually drawn.
static int drawWrappedCentered(Adafruit_ST7789& tft, const String& text, int16_t startY, int maxLines) {
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    int maxCharsPerLine = (cfg::DISPLAY_WIDTH - 2 * theme::MARGIN_X) / (6 * theme::BODY_TEXT_SIZE);
    if (maxCharsPerLine < 1) maxCharsPerLine = 1;
    int pos = 0, len = (int)text.length(), line = 0;
    int16_t y = startY;
    while (pos < len && line < maxLines) {
        int remaining = len - pos;
        int take = remaining < maxCharsPerLine ? remaining : maxCharsPerLine;
        bool lastLine = (line == maxLines - 1) && (pos + take < len);
        if (lastLine) take = maxCharsPerLine - 1; // room for the truncation glyph below
        if (take < remaining) {
            int lastSpace = -1;
            for (int i = take; i > 0; i--) {
                if (text[pos + i - 1] == ' ') { lastSpace = i; break; }
            }
            if (lastSpace > 0 && !lastLine) take = lastSpace;
        }
        String seg = text.substring(pos, pos + take);
        seg.trim();
        if (lastLine) seg += "\x7E";
        theme::drawCentered(tft, seg.c_str(), y, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
        y += theme::BODY_LINE_HEIGHT;
        pos += take;
        line++;
        if (lastLine) break;
    }
    return line;
}

static void drawReceived() {
    auto& tft = display::tft();
    ui::widgets::clearScreen(tft);
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y},
                        "New Message");

    theme::drawCentered(tft, "From", 70, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
    theme::drawCentered(tft, s_rxSenderName.c_str(), 92, theme::HEADER_TEXT_SIZE, theme::COLOR_TEXT);

    if (s_rxWasMorse) {
        // Always shown AS Morse -- the translation (when recognized) is a
        // secondary line underneath, not a replacement for the raw pattern.
        String quoted = "\"" + s_rxMorseRaw + "\"";
        int lines = drawWrappedCentered(tft, quoted, 122, 3);
        int16_t afterY = (int16_t)(122 + lines * theme::BODY_LINE_HEIGHT + 4);
        theme::drawCentered(tft, s_rxDecoded ? ("= " + s_rxText).c_str() : "(unrecognized pattern)",
                            afterY, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
    } else {
        String quoted = "\"" + s_rxText + "\"";
        drawWrappedCentered(tft, quoted, 148, 2);
    }

    theme::drawCentered(tft, "[ OK ]", cfg::DISPLAY_HEIGHT - 20, theme::HEADER_TEXT_SIZE, theme::COLOR_ACCENT);
}

// Mirrors tickSendMorse(), reading s_rxMorseRaw instead of the outgoing buffer.
static void tickReceiveMorse() {
    if (!s_rxMorseActive) return;
    unsigned long now = millis();
    if (s_rxMorseSounding) {
        if (now < s_rxMorseNextAtMs) return;
        buzzer::stopAsync();
        leds::clearAll();
        s_rxMorseSounding = false;
        s_rxMorsePos++;
        s_rxMorseNextAtMs = now + kMorseDotMs;
        return;
    }
    if (now < s_rxMorseNextAtMs) return;
    if (s_rxMorsePos >= (int)s_rxMorseRaw.length()) { s_rxMorseActive = false; return; }
    char c = s_rxMorseRaw[s_rxMorsePos];
    if (c == '.' || c == '-') {
        unsigned long dur = (c == '-') ? kMorseDashMs : kMorseDotMs;
        if (audio::isSoundEnabled()) buzzer::toneAsync(700);
        leds::writeChainRaw(led::kMaskWhite);
        s_rxMorseSounding = true;
        s_rxMorseNextAtMs = now + dur;
    } else if (c == '/') {
        s_rxMorsePos++;
        s_rxMorseNextAtMs = now + kMorseWordGapMs;
    } else {
        s_rxMorsePos++;
        s_rxMorseNextAtMs = now + kMorseLetterGapMs;
    }
}

static void receivedFrame() {
    if (s_rxMorseActive) tickReceiveMorse();
    if (input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::JoySelect) ||
        input::wasPressed(input::Button::Back)) {
        // Cut playback short on early dismiss, same as the send-side cleanup.
        if (s_rxMorseActive) {
            s_rxMorseActive = false;
            buzzer::stopAsync();
            leds::clearAll();
        }
        // No inbox/history -- always resume at Home, not wherever interrupted.
        enterHome();
    }
}

// ---------------------------------------------------------------------
// Morse Home (Manual / Quick Messages / Back)
// ---------------------------------------------------------------------

static const char* kMorseHomeItems[] = { "Manual", "Quick Messages", "Back" };
constexpr int kMorseHomeCount = 3;

// Chrome once, delta rows -- same fix as drawHome()/drawQuickMessages().
constexpr int16_t kMorseHomeListTop = (int16_t)(theme::LIST_START_Y + 30);
static int s_morseHomeLastSelected = -1;

static void drawMorseHomeRow(Adafruit_ST7789& tft, int i, int16_t y) {
    ui::Rect row{0, y, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_ITEM_HEIGHT};
    tft.fillRect(row.x, row.y, row.w, row.h, theme::COLOR_BG);
    ui::widgets::listItem(tft, row, i, kMorseHomeItems[i], i == s_morseHomeSelected);
}

static void drawMorseHome() {
    auto& tft = display::tft();
    bool fresh = !s_morseHomeChromeDrawn;
    if (fresh) {
        ui::widgets::clearScreen(tft);
        ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y},
                            "Morse (CW)");
        char sub[40];
        snprintf(sub, sizeof(sub), "To: %s", s_peerName);
        tft.setTextSize(theme::BODY_TEXT_SIZE);
        tft.setTextColor(theme::COLOR_ACCENT_DARK);
        tft.setCursor(theme::MARGIN_X, theme::LIST_START_Y + 4);
        tft.print(sub);
        s_morseHomeChromeDrawn = true;
        s_morseHomeLastSelected = -1;
    }
    for (int i = 0; i < kMorseHomeCount; i++) {
        // See drawHome()'s matching comment on why `fresh` is needed.
        if (fresh || i == s_morseHomeSelected || i == s_morseHomeLastSelected) {
            drawMorseHomeRow(tft, i, (int16_t)(kMorseHomeListTop + i * theme::LIST_ITEM_HEIGHT));
        }
    }
    s_morseHomeLastSelected = s_morseHomeSelected;
}

static void morseHomeFrame() {
    if (input::wasPressed(input::Button::JoyUp)) { s_morseHomeSelected = (s_morseHomeSelected - 1 + kMorseHomeCount) % kMorseHomeCount; ui::widgets::navSfx(); s_dirty = true; }
    if (input::wasPressed(input::Button::JoyDown)) { s_morseHomeSelected = (s_morseHomeSelected + 1) % kMorseHomeCount; ui::widgets::navSfx(); s_dirty = true; }

    bool confirm = input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::JoySelect);
    if (confirm) {
        if (s_morseHomeSelected == 0) {
            s_morseBuf = "";
            s_morseSounding = false;
            s_morseGapInserted = true;
            s_morseManualChromeDrawn = false;
            led::stop(); // quiescent Off state -- morseManualFrame() drives the chain directly
            setScreen(Screen::MorseManual);
        } else if (s_morseHomeSelected == 1) {
            s_morseQuickSelected = 0;
            s_morseQuickChromeDrawn = false;
            setScreen(Screen::MorseQuickMessages);
        }
    }
}

// ---------------------------------------------------------------------
// Morse Quick Messages
// ---------------------------------------------------------------------

// Chrome once, delta rows -- same fix as the other list screens above.
constexpr int kMorseQuickVisible = 5;
constexpr int16_t kMorseQuickListTop = (int16_t)(theme::LIST_START_Y + 4);
static int s_morseQuickLastTop = -1;
static int s_morseQuickLastSelected = -1;

static void drawMorseQuickRow(Adafruit_ST7789& tft, int i, int16_t y) {
    ui::Rect row{0, y, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_ITEM_HEIGHT};
    tft.fillRect(row.x, row.y, row.w, row.h, theme::COLOR_BG);
    ui::widgets::listItem(tft, row, i, kMorsePhrases[i].text, i == s_morseQuickSelected);
}

static void drawMorseQuickMessages() {
    auto& tft = display::tft();
    int top = s_morseQuickSelected - kMorseQuickVisible / 2;
    if (top < 0) top = 0;
    if (top > kMorsePhraseCount - kMorseQuickVisible) top = kMorsePhraseCount > kMorseQuickVisible ? kMorsePhraseCount - kMorseQuickVisible : 0;

    if (!s_morseQuickChromeDrawn) {
        ui::widgets::clearScreen(tft);
        ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y},
                            "Morse Quick Msgs");
        theme::drawCentered(tft, "OK=send  BACK=cancel", cfg::DISPLAY_HEIGHT - 16,
                            theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
        s_morseQuickChromeDrawn = true;
        s_morseQuickLastTop = -1;
        s_morseQuickLastSelected = -1;
    }

    if (top != s_morseQuickLastTop) {
        for (int slot = 0; slot < kMorseQuickVisible; slot++) {
            int i = top + slot;
            if (i < kMorsePhraseCount) drawMorseQuickRow(tft, i, (int16_t)(kMorseQuickListTop + slot * theme::LIST_ITEM_HEIGHT));
        }
        s_morseQuickLastTop = top;
    } else {
        for (int slot = 0; slot < kMorseQuickVisible; slot++) {
            int i = top + slot;
            if (i >= kMorsePhraseCount) continue;
            if (i == s_morseQuickSelected || i == s_morseQuickLastSelected) {
                drawMorseQuickRow(tft, i, (int16_t)(kMorseQuickListTop + slot * theme::LIST_ITEM_HEIGHT));
            }
        }
    }
    s_morseQuickLastSelected = s_morseQuickSelected;
}

static void morseQuickMessagesFrame() {
    if (input::wasPressed(input::Button::JoyUp)) { s_morseQuickSelected = (s_morseQuickSelected - 1 + kMorsePhraseCount) % kMorsePhraseCount; ui::widgets::navSfx(); s_dirty = true; }
    if (input::wasPressed(input::Button::JoyDown)) { s_morseQuickSelected = (s_morseQuickSelected + 1) % kMorsePhraseCount; ui::widgets::navSfx(); s_dirty = true; }

    bool confirm = input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::JoySelect);
    if (confirm) {
        const char* morse = kMorsePhrases[s_morseQuickSelected].morse;
        beginSend(radiolink::Type::MorseMsg, (const uint8_t*)morse, (uint8_t)strlen(morse));
    }
}

// ---------------------------------------------------------------------
// Manual Morse keying
// ---------------------------------------------------------------------

// Delta-tracked: chrome draws once; the accumulated-morse text and key
// indicator circle redraw only when they change.
static void drawMorseManual() {
    auto& tft = display::tft();
    if (!s_morseManualChromeDrawn) {
        ui::widgets::clearScreen(tft);
        ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y}, "TX");
        theme::drawCentered(tft, "OK=dash  SELECT=dot", cfg::DISPLAY_HEIGHT - 34,
                            theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
        theme::drawCentered(tft, "Pause idle to send, BACK=cancel", cfg::DISPLAY_HEIGHT - 16,
                            theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
        s_morseManualChromeDrawn = true;
        s_morseManualLastShown = "\x01"; // sentinel that can never match s_morseBuf, forces the first draw below
        s_morseManualLastSounding = !s_morseSounding;
    }

    String shown = s_morseBuf.length() ? s_morseBuf : String("(keying...)");
    if (shown != s_morseManualLastShown) {
        tft.fillRect(theme::MARGIN_X, 60, cfg::DISPLAY_WIDTH - 2 * theme::MARGIN_X, 40, theme::COLOR_BG);
        tft.setTextSize(theme::LIST_TEXT_SIZE);
        tft.setTextColor(ST77XX_WHITE);
        tft.setTextWrap(true);
        tft.setCursor(theme::MARGIN_X, 70);
        tft.print(shown);
        s_morseManualLastShown = shown;
    }

    if (s_morseSounding != s_morseManualLastSounding) {
        tft.fillCircle(cfg::DISPLAY_WIDTH / 2, 170, 18, s_morseSounding ? ST77XX_WHITE : theme::COLOR_ACCENT_DARK);
        s_morseManualLastSounding = s_morseSounding;
    }
}

static void morseManualFrame() {
    unsigned long now = millis();

    if (s_morseSounding && now - s_morseSoundStartMs >= s_morseSoundDurationMs) {
        buzzer::stopAsync();
        leds::clearAll();
        s_morseSounding = false;
        s_morseLastSymbolEndMs = now;
        s_morseGapInserted = false;
        s_dirty = true;
    }

    if (!s_morseSounding) {
        bool dashPressed = input::wasPressed(input::Button::Ok);
        bool dotPressed = input::wasPressed(input::Button::JoySelect);
        if (dashPressed || dotPressed) {
            s_morseBuf += dashPressed ? '-' : '.';
            s_morseSoundDurationMs = dashPressed ? kMorseDashMs : kMorseDotMs;
            s_morseSoundStartMs = now;
            s_morseSounding = true;
            s_morseGapInserted = true; // a symbol just started; no gap decision owed right now
            // buzzer:: directly, not audio::playSfx() -- its articulation gap
            // (85%-duration cut) would chop this fixed-duration symbol short.
            if (audio::isSoundEnabled()) buzzer::toneAsync(700);
            // Direct chain write, not led::playEffect() -- no named effect
            // means "on for exactly this duration"; relies on the effect
            // system being quiescent going into Morse Manual (see led::stop()).
            leds::writeChainRaw(led::kMaskWhite);
            s_dirty = true;
        }
    }

    if (!s_morseSounding && s_morseBuf.length()) {
        unsigned long idle = now - s_morseLastSymbolEndMs;
        // Send check must NOT be gated by s_morseGapInserted like the gap
        // insertion below -- that flag flips true at the word gap (1400ms),
        // before the send threshold (2600ms), which would block the send
        // check from ever running for the rest of this idle period.
        if (idle >= kMorseSendGapMs) {
            String trimmed = s_morseBuf;
            trimmed.trim();
            while (trimmed.endsWith("/")) { trimmed.remove(trimmed.length() - 1); trimmed.trim(); }
            if (trimmed.length()) {
                beginSend(radiolink::Type::MorseMsg, (const uint8_t*)trimmed.c_str(), (uint8_t)trimmed.length());
            }
            s_morseBuf = "";
            s_morseGapInserted = true;
            s_dirty = true;
        } else if (!s_morseGapInserted) {
            if (idle >= kMorseWordGapMs) {
                if (!s_morseBuf.endsWith(" / ")) s_morseBuf += " / ";
                s_morseGapInserted = true;
                s_dirty = true;
            } else if (idle >= kMorseLetterGapMs) {
                if (!s_morseBuf.endsWith(" ")) s_morseBuf += ' ';
                s_morseGapInserted = true;
                s_dirty = true;
            }
        }
    }

    if (input::wasPressed(input::Button::Back) && !s_morseSounding) {
        buzzer::stopAsync();
        led::stop();
        s_morseHomeChromeDrawn = false;
        setScreen(Screen::MorseHome);
    }
}

// ---------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------

void enter() {
    s_flow = Flow::Text;
    s_screen = Screen::Startup;
    s_screenAtMs = millis();
    s_dirty = true;
    s_morseBuf = "";
    s_morseSounding = false;

    String name = storage::loadMyIdentity().name;
    s_radioOffline = !radiolink::begin(name.length() ? name.c_str() : nullptr);
    if (!s_radioOffline) {
        radiolink::setRxHandler(onMessage, nullptr);
        radiolink::setTxIndicator(onTxIndicator, nullptr);
        enterStartup();
    }
}

AppState frame() {
    // Global Back-to-exit: only from Home (deeper screens back out one level).
    if (s_screen == Screen::Home && input::wasPressed(input::Button::Back)) {
        radiolink::end();
        led::stop(); // defense-in-depth against the Sweep-outlives-Startup leak
        return AppState::MainMenu;
    }
    if (s_radioOffline) {
        if (input::wasPressed(input::Button::Back)) {
            radiolink::end();
            led::stop();
            return AppState::MainMenu;
        }
        if (s_dirty) {
            auto& tft = display::tft();
            ui::widgets::clearScreen(tft);
            theme::drawCentered(tft, "Radio Offline", 100, theme::HEADER_TEXT_SIZE, theme::COLOR_DANGER);
            theme::drawCentered(tft, "Switch badge OFF/ON", 130, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
            theme::drawCentered(tft, "BACK to exit", cfg::DISPLAY_HEIGHT - 16, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
            s_dirty = false;
        }
        return AppState::RadioChat;
    }

    radiolink::update(); // first thing every tick, before any drawing -- see radio_link.h

    // Custom-message text entry runs as its own sub-loop layered on QuickMessages.
    if (s_customEntryOpen) {
        bool confirmed = false;
        if (text_entry::frame(&confirmed)) {
            s_customEntryOpen = false;
            if (confirmed) {
                const char* text = text_entry::result();
                if (text[0]) beginSend(radiolink::Type::TextMsg, (const uint8_t*)text, (uint8_t)strlen(text));
                else { s_quickChromeDrawn = false; setScreen(Screen::QuickMessages); }
            } else {
                s_quickChromeDrawn = false;
                setScreen(Screen::QuickMessages);
            }
        }
        return AppState::RadioChat;
    }

    // Back handling for every non-Home screen: one level up.
    if (input::wasPressed(input::Button::Back)) {
        switch (s_screen) {
            case Screen::ContactPicker:
                radiolink::setBeaconIntervalMs(18000);
                enterHome();
                break;
            case Screen::QuickMessages:
                setScreen(Screen::ContactPicker);
                enterContactPicker();
                break;
            case Screen::MorseHome:
                setScreen(Screen::ContactPicker);
                enterContactPicker();
                break;
            case Screen::MorseQuickMessages:
                s_morseHomeChromeDrawn = false;
                setScreen(Screen::MorseHome);
                break;
            case Screen::Sending:
                break; // ignored -- the send is already in flight, let it resolve
            case Screen::MorseManual:
                break; // handled inside morseManualFrame() (must not interrupt a held key)
            default:
                break;
        }
    }

    switch (s_screen) {
        case Screen::Startup: {
            unsigned long elapsed = millis() - s_screenAtMs;
            if (elapsed >= kStartupMs) { enterHome(); break; }
            drawStartup(elapsed);
            return AppState::RadioChat; // startup draws every tick (animated), skip the dirty-flag path below
        }
        case Screen::Home:
            if (homeFrame()) { radiolink::end(); return AppState::MainMenu; }
            break;
        case Screen::ContactPicker: contactPickerFrame(); break;
        case Screen::QuickMessages: quickMessagesFrame(); break;
        case Screen::Sending: sendingFrame(); break;
        case Screen::Received: receivedFrame(); break;
        case Screen::MorseHome: morseHomeFrame(); break;
        case Screen::MorseQuickMessages: morseQuickMessagesFrame(); break;
        case Screen::MorseManual: morseManualFrame(); break;
    }

    if (s_dirty) {
        switch (s_screen) {
            case Screen::Startup: break; // drawn above, returned early
            case Screen::Home: drawHome(); break;
            case Screen::ContactPicker: drawContactPicker(); break;
            case Screen::QuickMessages: drawQuickMessages(); break;
            case Screen::Sending: drawSending(); break;
            case Screen::Received: drawReceived(); break;
            case Screen::MorseHome: drawMorseHome(); break;
            case Screen::MorseQuickMessages: drawMorseQuickMessages(); break;
            case Screen::MorseManual: drawMorseManual(); break;
        }
        s_dirty = false;
    }
    drawTransitionSweep();
    return AppState::RadioChat;
}

} // namespace radiochat

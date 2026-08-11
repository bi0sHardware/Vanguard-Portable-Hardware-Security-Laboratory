#include "flagdesk.h"
#include "challenge_data.h"
#include "challenge_engine.h"
#include "challenge_state.h"
#include "serial_line.h"
#include "console.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"
#include <Arduino.h>
#include <cstring>
#include <mbedtls/sha256.h>

namespace flagdesk {

// Stages store only a SHA-256 digest of the flag, never the text itself. Hash the submission and compare digests.
static bool checkFlag(const challenges::ChallengeStage* stage, const String& submitted) {
    if (!stage || !stage->flagSha256) return false;
    uint8_t digest[32];
    mbedtls_sha256((const unsigned char*)submitted.c_str(), submitted.length(), digest, 0);
    return memcmp(digest, stage->flagSha256, sizeof(digest)) == 0;
}

// Data-driven lookup by "lvl<level>" id, so no changes needed if Level 5+ is added.
static const challenges::ChallengeStage* findLevelStage(int level) {
    char id[8];
    snprintf(id, sizeof(id), "lvl%d", level);
    for (int i = 0; i < challenges::kStageCount; i++) {
        if (strcmp(challenges::kRegistry[i].id, id) == 0) return &challenges::kRegistry[i];
    }
    return nullptr;
}

static const challenges::ChallengeStage* findStageById(const char* id) {
    if (!id) return nullptr;
    for (int i = 0; i < challenges::kStageCount; i++) {
        if (strcmp(challenges::kRegistry[i].id, id) == 0) return &challenges::kRegistry[i];
    }
    return nullptr;
}

enum class Phase { SelectLevel, EnterFlag, Result };
static Phase s_phase;
static int s_level;
static bool s_lastCorrect;
static bool s_dirty;
static unsigned long s_resultShownAt;

// Lists all four levels with their current state.
static void printLevelMenu() {
    // Guard against the no-terminal blocking-write freeze (see console.cpp).
    if (!Serial) return;
    for (int lvl = 1; lvl <= 4; lvl++) {
        const challenges::ChallengeStage* stage = findLevelStage(lvl);
        if (!stage) continue;
        bool done = challenges::isCompleted(stage->id);
        // No id-keyed isUnlocked() exists, so mirror its logic directly.
        bool locked = stage->requiresId && !challenges::isCompleted(stage->requiresId);
        char label[8];
        snprintf(label, sizeof(label), "  %d.", lvl);
        Serial.print(label);
        Serial.print(' ');
        Serial.print(stage->name);
        if (done) { Serial.print(console::GREEN); Serial.print("  [SOLVED]"); }
        else if (locked) { Serial.print(console::DIM); Serial.print("  [LOCKED]"); }
        Serial.println(console::RESET);
    }
}

static void printBanner() {
    console::banner("VANGUARD FLAG SUBMISSION", "PuTTY / serial flag desk");
    printLevelMenu();
    console::prompt("Select level (1-4)");
}

static void printFlagPrompt() {
    char msg[24];
    snprintf(msg, sizeof(msg), "Flag for Level %d", s_level);
    console::prompt(msg);
}

void enter() {
    s_phase = Phase::SelectLevel;
    serialline::reset();
    s_dirty = true;
    // FlagDesk isn't Renderer-based, so it must clear its own canvas on
    // entry or the stage list underneath would show through.
    ui::widgets::clearScreen(display::tft());
    printBanner();
}

static void drawScreen() {
    auto& tft = display::tft();
    // Full clear before every redraw: theme::drawCentered() draws glyphs
    // transparently, so a wrong then right flag would leave leftover pixels
    // from "FAILURE" showing through "SUCCESS".
    ui::widgets::clearScreen(tft);
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y}, "Flag Submission");
    theme::drawCentered(tft, "Use PuTTY on the", 90, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
    theme::drawCentered(tft, "badge's serial port", 105, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
    if (s_phase == Phase::Result) {
        theme::drawCentered(tft, s_lastCorrect ? "SUCCESS" : "FAILURE", 150, theme::HEADER_TEXT_SIZE,
                             s_lastCorrect ? theme::COLOR_SUCCESS : theme::COLOR_DANGER);
    }
    theme::drawCentered(tft, "BACK to exit", cfg::DISPLAY_HEIGHT - 20, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
}

bool frame() {
    if (input::wasPressed(input::Button::Back)) {
        return true;
    }

    String line;
    switch (s_phase) {
        case Phase::SelectLevel:
            if (serialline::readEchoedLine(line)) {
                int lvl = line.toInt();
                if (lvl >= 1 && lvl <= 4 && findLevelStage(lvl)) {
                    s_level = lvl;
                    s_phase = Phase::EnterFlag;
                    printFlagPrompt();
                } else {
                    console::err("Invalid level. Enter 1-4.");
                    console::prompt("Select level (1-4)");
                }
            }
            break;
        case Phase::EnterFlag: {
            if (serialline::readEchoedLine(line)) {
                const challenges::ChallengeStage* stage = findLevelStage(s_level);
                // SECURITY: gate on the level being unlocked, not just flag
                // match, so a leaked/guessed later-level flag can't skip prerequisites.
                bool locked = stage && stage->requiresId &&
                              !challenges::isCompleted(stage->requiresId);
                s_lastCorrect = stage && !locked && checkFlag(stage, line);
                char msg[64];
                if (s_lastCorrect) {
                    snprintf(msg, sizeof(msg), "SUCCESS - Level %d flag accepted.", s_level);
                    console::ok(msg);
                    challenges::completeChallenge(stage->id);
                    if (stage->reward) console::field("Reward", stage->reward, console::GREEN);
                    console::flagBlock("ACCEPTED", line.c_str());
                } else if (locked) {
                    const challenges::ChallengeStage* req = findStageById(stage->requiresId);
                    snprintf(msg, sizeof(msg), "Level %d is still locked.", s_level);
                    console::err(msg);
                    console::warn((String("Complete \"") +
                                  (req ? req->name : "the prior level") +
                                  "\" first.").c_str());
                } else {
                    snprintf(msg, sizeof(msg), "FAILURE - incorrect flag for Level %d.", s_level);
                    console::err(msg);
                }
                s_phase = Phase::Result;
                s_resultShownAt = millis();
                s_dirty = true;
            }
            break;
        }
        case Phase::Result:
            if (millis() - s_resultShownAt > 2000) {
                s_phase = Phase::SelectLevel;
                s_dirty = true;
                printBanner();
            }
            break;
    }
    if (s_dirty) { drawScreen(); s_dirty = false; }
    return false;
}

} // namespace flagdesk

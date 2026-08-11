#include "console.h"

namespace console {

// Every public function bails out if nothing has the USB CDC port open.
// HWCDC Serial writes can BLOCK if no host is draining the TX buffer, which
// would freeze the whole (single-threaded) badge with no terminal attached.
// `if (Serial)` reads HWCDC's DTR/connected state to guard against this.
static bool connected() { return (bool)Serial; }

void rule(char c, uint8_t width) {
    if (!connected()) return;
    Serial.print(DIM);
    Serial.print(CYAN);
    for (uint8_t i = 0; i < width; i++) Serial.print(c);
    Serial.println(RESET);
}

void banner(const char* title, const char* subtitle) {
    if (!connected()) return;
    Serial.println();
    rule('=');
    Serial.print(BOLD); Serial.print(CYAN);
    Serial.print("  ");
    Serial.print(title);
    Serial.println(RESET);
    if (subtitle) {
        Serial.print(DIM); Serial.print(WHITE);
        Serial.print("  ");
        Serial.print(subtitle);
        Serial.println(RESET);
    }
    rule('=');
}

static void tagged(const char* color, const char* tag, const char* msg) {
    if (!connected()) return;
    Serial.print(color);
    Serial.print(tag);
    Serial.print(RESET);
    Serial.print(' ');
    Serial.println(msg);
}

void ok(const char* msg)   { tagged(GREEN,  "[ OK ]", msg); }
void info(const char* msg) { tagged(CYAN,   "[ ** ]", msg); }
void warn(const char* msg) { tagged(YELLOW, "[ !! ]", msg); }
void err(const char* msg)  { tagged(RED,    "[ XX ]", msg); }
void step(const char* msg) { tagged(DIM,    "[ >> ]", msg); }

void field(const char* key, const char* value, const char* valueColor) {
    if (!connected()) return;
    Serial.print("   ");
    Serial.print(DIM); Serial.print(WHITE);
    Serial.print(key);
    Serial.print(RESET);
    // Pad to a fixed column so fields line up.
    int pad = 16 - (int)strlen(key);
    for (int i = 0; i < pad; i++) Serial.print(' ');
    Serial.print(": ");
    if (valueColor) Serial.print(valueColor);
    Serial.print(value);
    Serial.println(RESET);
}

void flagBlock(const char* label, const char* flag) {
    if (!connected()) return;
    Serial.println();
    rule('*');
    Serial.print(BOLD); Serial.print(GREEN);
    Serial.print("  ");
    Serial.println(label);
    Serial.print(BOLD); Serial.print(YELLOW);
    Serial.print("  ");
    Serial.println(flag);
    Serial.print(RESET);
    rule('*');
    Serial.println();
}

void prompt(const char* label) {
    if (!connected()) return;
    Serial.println();
    Serial.print(BOLD); Serial.print(CYAN);
    Serial.print(label);
    Serial.println(RESET);
    Serial.print(GREEN);
    Serial.print("> ");
    Serial.print(RESET);
}

void pollConnection() {
    // Debounced: some USB-CDC drivers bounce DTR briefly during
    // negotiation, which without this printed the welcome banner twice.
    static bool s_stable = false;
    static bool s_lastRaw = false;
    static unsigned long s_lastChangeMs = 0;
    constexpr unsigned long kDebounceMs = 150;

    bool raw = connected();
    if (raw != s_lastRaw) {
        s_lastRaw = raw;
        s_lastChangeMs = millis();
    }
    if (raw != s_stable && millis() - s_lastChangeMs >= kDebounceMs) {
        bool wasConnected = s_stable;
        s_stable = raw;
        if (s_stable && !wasConnected) {
            banner("VANGUARD GROUND TERMINAL", "Serial link established");
            Serial.print(DIM); Serial.print(WHITE);
            Serial.println("  Navigate the badge's on-screen menu to interact --");
            Serial.println("  Challenges, Contacts and PeerDrop all use this console");
            Serial.println("  when open.");
            Serial.print(RESET);
            Serial.println();
        }
    }
}

} // namespace console

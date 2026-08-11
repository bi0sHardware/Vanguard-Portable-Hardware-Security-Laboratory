#include "serial_line.h"

namespace serialline {

static String s_lineBuf;
static bool s_ignoreNextLF = false;

void reset() {
    s_lineBuf = "";
    s_ignoreNextLF = false;
}

bool readEchoedLine(String& outLine) {
    // `Serial &&` guards the echo writes: buffered RX bytes can outlive a
    // terminal disconnect, and echoing then would be a blocking HWCDC write with nobody to drain it.
    while (Serial && Serial.available()) {
        char c = (char)Serial.read();
        if (s_ignoreNextLF) {
            s_ignoreNextLF = false;
            if (c == '\n') continue;
        }
        if (c == '\r' || c == '\n') {
            Serial.println();
            outLine = s_lineBuf;
            outLine.trim();
            s_lineBuf = "";
            if (c == '\r') s_ignoreNextLF = true;
            return true;
        }
        if (c == 0x7F || c == 0x08) { // backspace/delete
            if (s_lineBuf.length()) {
                s_lineBuf.remove(s_lineBuf.length() - 1);
                Serial.print("\b \b");
            }
            continue;
        }
        s_lineBuf += c;
        Serial.write((uint8_t)c);
    }
    return false;
}

} // namespace serialline

#pragma once
#include <Arduino.h>

// Shared PuTTY-friendly line reader over USB-CDC Serial — factored out of
// flagdesk.cpp so other screens don't reimplement echo/backspace handling.
// Single static buffer is safe since only one screen is active at a time.
namespace serialline {

// Echoes each typed char back (plain PuTTY doesn't locally echo). Returns
// true on the tick a full line completes, trimmed into outLine.
// PuTTY sends bare '\r' for Enter; a follow-on '\n' is absorbed.
bool readEchoedLine(String& outLine);

// Resets the internal buffer — call on screen entry so a stale partial line doesn't leak in.
void reset();

} // namespace serialline

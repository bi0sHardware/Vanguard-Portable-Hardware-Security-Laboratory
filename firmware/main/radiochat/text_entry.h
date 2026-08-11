#pragma once
#include <cstddef>

// On-device character-picker text entry, using only the badge's joystick+screen.
// Built for Radio Chat's "Custom..." message but kept generic for reuse.
//
// Controls: Up/Down cycles charset at cursor, Left/Right moves cursor, Ok
// confirms, Back cancels, Pause deletes at cursor.
//
// Not a full AppState. Call enter() once, then frame() every tick while open;
// frame() returns true exactly once, the tick the user confirms or cancels.
namespace text_entry {

void enter(const char* title, const char* initial, size_t maxLen);

// Owns the whole screen. Returns true once, when input is finalized;
// *confirmed is true for Ok, false for Back.
bool frame(bool* confirmed);

// Valid only after frame() has returned true with *confirmed == true.
const char* result();

} // namespace text_entry

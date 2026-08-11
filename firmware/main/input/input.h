#pragma once

// Joystick SW1 (active-LOW, internal pull-up) + buttons S1-S5 (active-HIGH,
// external pull-down). Spec §7. Hardware RC filtering is already present on
// the board; this module adds software debounce + edge detection on top.
namespace input {

enum class Button {
    JoyUp, JoyDown, JoyLeft, JoyRight, JoySelect,
    Pause, Disp, Start, Back, Ok,
    Count
};

void init();

// Call once per loop tick before querying state.
void update();

bool isDown(Button b);
bool wasPressed(Button b);  // rising edge this tick (debounced)
bool wasReleased(Button b); // falling edge this tick (debounced)

} // namespace input

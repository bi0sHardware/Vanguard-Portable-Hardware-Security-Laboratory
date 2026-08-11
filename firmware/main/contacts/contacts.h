#pragma once
#include "../../include/state.h"

// Contacts Manager — spec §8.7: lists exchanged contacts (Name/Organisation),
// preview panel shows Email/Phone, OK generates a random mock contact for
// testing, PAUSE deletes the selected contact (re-packing NVS).
namespace contacts {

void enter();
AppState frame();

} // namespace contacts

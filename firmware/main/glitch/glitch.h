#pragma once
#include "../../include/state.h"

// Badge Attack: opt-in "hack this badge" minigame between consenting participants. Anyone with Radio Chat or
// Ship Battle open (shared badge-link channel) is a valid target. An unblocked "Attack" packet takes over the
// victim's display for a few seconds (glitched visuals, "Never Gonna Give You Up" riff, flashing LEDs, disabled
// input), then hands control back. Landing an attack on someone else (storage::hasAttackedSomeone()) is the only
// way to unlock the "Secure Badge" defense toggle in Settings.
// Cross-cutting like challenges::consumeMissionCompleteEvent(): main.cpp's loop() polls consumeTriggerEvent()
// at high priority and pre-empts whatever screen the victim is on.
namespace glitch {

// Called by Radio Chat's/Ship Battle's RX handler on a validated Attack packet when not defended against.
// Idempotent while already pending/active.
void trigger();

// True exactly once when an attack has landed and hasn't been actioned yet (one-shot-consume pattern).
bool consumeTriggerEvent();

// returnTo: AppState the victim was on when the attack landed; control returns there once the effect ends.
void enter(AppState returnTo);
AppState frame(); // returns AppState::Glitched while still running, then returnTo

} // namespace glitch

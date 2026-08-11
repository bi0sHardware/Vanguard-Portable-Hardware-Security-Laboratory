#pragma once

// Reusable challenge engine: goes from "a stage was solved" to NVS/LED/audio
// updates. Kept separate from challenges.h since init()/update() must run
// every loop() tick regardless of active AppState (background tasks like
// Level 1's UART leak keep running off-screen).
namespace challenges {

void init();   // starts always-on background challenge tasks
void update(); // call every loop() tick regardless of AppState

// True if `index` has no unlock requirement, or its requiresId is already completed.
bool isUnlocked(int index);

// Completed count excluding the FlagDesk utility entry — drives LED progress baseline.
int completedCount();

// Idempotent: completing an already-completed stage is a silent no-op.
void completeChallenge(const char* id);

// True exactly once, the tick all four Challenge levels become complete.
// main.cpp polls this every tick and force-switches AppState to MissionComplete.
bool consumeMissionCompleteEvent();

// Clears all completion state. Testing/reset use only.
void resetAll();

} // namespace challenges

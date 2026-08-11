#pragma once
#include "../../include/state.h"

// PeerDrop — spec §8.4: BLE proximity contact exchange.
// Scan -> Confirm (Receive/Send) -> Waiting -> Exchange -> Success/Failure.
// Downloads identity CSV, writes "ACK", saves contact to NVS.
namespace peerdrop {

void enter();
AppState frame();

// Idempotent no-op-safe stop of BLE scan + advertising, for forced-transition
// paths that skip the current screen's own frame() cleanup.
void stopIfActive();

} // namespace peerdrop

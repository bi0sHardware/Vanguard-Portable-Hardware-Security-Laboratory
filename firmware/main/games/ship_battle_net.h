#pragma once
#include <Arduino.h>

// Ship Battle's protocol/turn state machine, separate from ship_battle.cpp's
// drawing code. Independent consumer of main/rf/radio_link.h; never touches
// radiochat's Type values or session state.
//
// Wire messages (see radio_link_types.h): HostAdv, JoinReq, JoinAccept,
// JoinReject, FleetReady, Fire, FireResult, GameSync, GameOver, Forfeit.
//
// Games are identified by (peerLinkId, sessionId) together, never sessionId
// alone -- two hosts can coincidentally share a sessionId.
//
// Turn idempotency (key correctness rule): the defender caches the last
// (turn, outcome) it resolved and RESENDS it if the same turn's Fire
// arrives again (lost FireResult or attacker reboot/resend), instead of
// re-resolving -- re-resolving would double-count a hit/sink.
namespace shipnet {

constexpr int kBoardSize   = 8;
constexpr int kShipCount   = 5;
constexpr int kFleetCells  = 17; // 5+4+3+3+2
extern const uint8_t kShipSizes[kShipCount]; // {5,4,3,3,2}, Carrier..Destroyer

enum class Phase : uint8_t {
    Idle,             // radiolink not active / not in a game
    Hosting,          // broadcasting HostAdv, waiting for a JoinReq
    Joining,          // sent JoinReq, waiting for JoinAccept/JoinReject
    JoinRejected,     // host said Busy -- terminal until the player backs out
    Deploy,           // placing own fleet locally, no radio traffic yet
    WaitFleet,        // own FleetReady sent, waiting for opponent's
    MyTurn,           // my turn to pick a cell and fire
    AwaitResult,      // Fire sent, waiting for the defender's FireResult
    TheirTurn,        // waiting for the opponent's Fire
    GameOver,         // fleet destroyed either way, or a clean Forfeit
    Aborted,          // opponent silent past the timeout -- NOT a win
};

enum class OverResult : uint8_t { None, Win, Lose, OpponentLost, YouForfeited, OpponentForfeited };

enum class Cell : uint8_t { Empty, Ship, Miss, Hit, Sunk };

struct HostInfo {
    char linkId[7];
    char name[17];
    uint8_t session;
    int16_t rssiDbm;
    unsigned long lastHeardMs;
};

// ---- lifecycle -------------------------------------------------------
bool begin(const char* displayName); // wraps radiolink::begin(); false if radio unavailable
void end();                          // wraps radiolink::end(), resets all local state
void update();                       // call FIRST every tick, before drawing (drives radiolink::update() too)

// ---- discovery (Join flow) --------------------------------------------
int hostCount();
const HostInfo& hostAt(int i);

// ---- host / join ------------------------------------------------------
void startHosting(const char* myName);
void joinHost(const char* peerLinkId, uint8_t session, const char* myName);
void cancelHostOrJoin(); // back out of Hosting/Joining/JoinRejected to Idle

// ---- deploy (local only, no radio traffic) -----------------------------
// row/col in [0, kBoardSize). Returns false on out-of-bounds or overlap
// with an already-placed ship; the caller's UI state is untouched either
// way (idempotent to retry with a different cell/orientation).
bool placeShip(int shipIndex, uint8_t row, uint8_t col, bool horizontal);
void autoPlaceFleet(); // random valid placement of all 5 ships, replaces any partial placement
void clearFleet();
bool shipPlaced(int shipIndex);
bool fleetComplete();     // all 5 ships placed
void confirmFleetReady(); // sends FleetReady, transitions Deploy -> WaitFleet (no-op if fleet incomplete)

// ---- gameplay -----------------------------------------------------------
bool fireAt(uint8_t row, uint8_t col); // only valid in MyTurn on an unfired opponent cell
void forfeit();                        // sends Forfeit (best-effort, twice), -> GameOver immediately

// ---- state accessors for the UI ----------------------------------------
Phase phase();
bool isHost();
const char* opponentLinkId();
const char* opponentName();
Cell myCell(uint8_t row, uint8_t col);    // my fleet + incoming shots against me
Cell theirCell(uint8_t row, uint8_t col); // my shots at them + results
int myShipsRemaining();                   // cells of mine not yet hit
int theirShipsRemaining();                // as last reported by them (starts at kFleetCells)
OverResult overResult();
unsigned long silentMs();  // ms since anything heard from the opponent -- drives "Reconnecting" UI
uint8_t pendingFireCell();  // cell awaiting a result, for UI highlight during AwaitResult

} // namespace shipnet

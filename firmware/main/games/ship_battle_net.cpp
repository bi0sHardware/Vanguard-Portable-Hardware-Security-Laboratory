#include "ship_battle_net.h"
#include "../rf/radio_link.h"
#include "../storage/storage.h"
#include "../glitch/glitch.h"
#include <esp_random.h>
#include <string.h>
#include <stdlib.h>

namespace shipnet {

using radiolink::Type;

const uint8_t kShipSizes[kShipCount] = { 5, 4, 3, 3, 2 };

struct Ship {
    bool placed;
    uint8_t row, col;
    bool horizontal;
    uint8_t hitCount;
};

// ---- state --------------------------------------------------------------
static Phase s_phase = Phase::Idle;
static bool s_isHost = false;
static uint8_t s_mySession = 0;
static char s_oppLinkId[7] = {0};
static char s_oppName[17] = {0};
static char s_myName[17] = {0}; // advertised in HostAdv while Hosting
static bool s_firstMoverIsHost = true;

static Ship s_myShips[kShipCount];
static bool s_myFleetGrid[kBoardSize][kBoardSize];
static bool s_myHitGrid[kBoardSize][kBoardSize];
static Cell s_theirGrid[kBoardSize][kBoardSize];
static int s_myRemaining = kFleetCells;
static int s_theirRemaining = kFleetCells;

static uint8_t s_turn = 0;
static uint8_t s_pendingFireCell = 0;
static bool s_myFleetReadySent = false;
static bool s_theirFleetReady = false;

// Turn idempotency cache (defender side) -- see this file's header comment.
static bool s_haveLastResult = false;
static uint8_t s_lastResolvedTurn = 0;
static uint8_t s_lastResultCell = 0;
static radiolink::FireResultCode s_lastResultCode = radiolink::FireResultCode::Miss;
static uint8_t s_lastResultShipId = 0xFF;

static OverResult s_overResult = OverResult::None;

constexpr int kMaxHosts = 8;
constexpr unsigned long kHostStaleMs = 12000;
static HostInfo s_hosts[kMaxHosts];
static int s_hostCount = 0;

constexpr unsigned long kHostAdvIntervalMs = 3000;
static unsigned long s_lastHostAdvMs = 0;
constexpr unsigned long kGameSyncIntervalMs = 6000;
static unsigned long s_lastGameSyncMs = 0;
// Longer than Radio Chat's timeouts: mid-game silence is more disruptive
// to recover from than a missed chat message.
constexpr unsigned long kAbandonMs = 45000;
// FleetReady's sendReliable() can exhaust its retries silently; if every
// attempt is lost both sides stall in WaitFleet forever (GameSync's resync
// doesn't cover WaitFleet). Periodic idempotent re-send closes the gap.
constexpr unsigned long kFleetReadyResendMs = 4000;
static unsigned long s_lastFleetReadySentMs = 0;

// ---- helpers --------------------------------------------------------------

static void resetBoards() {
    memset(s_myShips, 0, sizeof(s_myShips));
    memset(s_myFleetGrid, 0, sizeof(s_myFleetGrid));
    memset(s_myHitGrid, 0, sizeof(s_myHitGrid));
    for (int r = 0; r < kBoardSize; r++)
        for (int c = 0; c < kBoardSize; c++) s_theirGrid[r][c] = Cell::Empty;
    s_myRemaining = kFleetCells;
    s_theirRemaining = kFleetCells;
    s_turn = 0;
    s_myFleetReadySent = false;
    s_theirFleetReady = false;
    s_haveLastResult = false;
    s_overResult = OverResult::None;
}

// Fills outCells (size = kShipSizes[shipIndex]) with the (row,col) pairs a
// ship at this position/orientation would occupy, packed as (row<<4)|col.
// Returns false without writing anything if any cell is out of bounds.
static bool computeShipCells(int shipIndex, uint8_t row, uint8_t col, bool horizontal, uint8_t* outCells) {
    uint8_t size = kShipSizes[shipIndex];
    for (uint8_t i = 0; i < size; i++) {
        int r = horizontal ? row : row + i;
        int c = horizontal ? col + i : col;
        if (r < 0 || r >= kBoardSize || c < 0 || c >= kBoardSize) return false;
        outCells[i] = (uint8_t)((r << 4) | c);
    }
    return true;
}

static int shipIndexAt(uint8_t row, uint8_t col) {
    for (int i = 0; i < kShipCount; i++) {
        if (!s_myShips[i].placed) continue;
        uint8_t cells[5];
        if (!computeShipCells(i, s_myShips[i].row, s_myShips[i].col, s_myShips[i].horizontal, cells)) continue;
        for (uint8_t k = 0; k < kShipSizes[i]; k++) {
            if (((cells[k] >> 4) == row) && ((cells[k] & 0x0F) == col)) return i;
        }
    }
    return -1;
}

static void upsertHost(const char* linkId, uint8_t session, const char* name, uint8_t nameLen, int16_t rssi) {
    for (int i = 0; i < s_hostCount; i++) {
        if (strncmp(s_hosts[i].linkId, linkId, 6) == 0) {
            s_hosts[i].session = session;
            s_hosts[i].rssiDbm = rssi;
            s_hosts[i].lastHeardMs = millis();
            size_t n = nameLen < 16 ? nameLen : 16;
            memcpy(s_hosts[i].name, name, n);
            s_hosts[i].name[n] = 0;
            return;
        }
    }
    if (s_hostCount >= kMaxHosts) return;
    HostInfo& h = s_hosts[s_hostCount++];
    strncpy(h.linkId, linkId, 6); h.linkId[6] = 0;
    h.session = session;
    h.rssiDbm = rssi;
    h.lastHeardMs = millis();
    size_t n = nameLen < 16 ? nameLen : 16;
    memcpy(h.name, name, n);
    h.name[n] = 0;
}

static void pruneStaleHosts() {
    unsigned long now = millis();
    for (int i = 0; i < s_hostCount;) {
        if (now - s_hosts[i].lastHeardMs > kHostStaleMs) {
            s_hosts[i] = s_hosts[s_hostCount - 1];
            s_hostCount--;
        } else {
            i++;
        }
    }
}

static void sendGameOver(bool iWon) {
    // Winner byte is absolute (1=host won), not relative to the sender, so
    // either side can announce it and receivers compute outcome the same way.
    bool hostWon = iWon ? s_isHost : !s_isHost;
    uint8_t pay[2] = { (uint8_t)radiolink::OverReason::FleetDestroyed, (uint8_t)(hostWon ? 1 : 0) };
    radiolink::sendUnreliable(s_oppLinkId, Type::GameOver, s_mySession, pay, 2);
}

static void sendFireResultCached() {
    uint8_t pay[5] = { s_lastResolvedTurn, s_lastResultCell, (uint8_t)s_lastResultCode,
                        s_lastResultShipId, (uint8_t)s_myRemaining };
    radiolink::sendReliable(s_oppLinkId, Type::FireResult, s_mySession, pay, 5, nullptr, nullptr);
}

static void maybeStartTurns() {
    if (s_phase != Phase::WaitFleet) return;
    if (!s_myFleetReadySent || !s_theirFleetReady) return;
    bool myTurnFirst = (s_firstMoverIsHost == s_isHost);
    s_phase = myTurnFirst ? Phase::MyTurn : Phase::TheirTurn;
}

static void handleIncomingFire(uint8_t turn, uint8_t cell) {
    if (s_haveLastResult && turn == s_lastResolvedTurn) {
        // Repeat of a resolved turn (lost result or attacker reboot): resend, never re-resolve.
        sendFireResultCached();
        return;
    }
    uint8_t r = cell >> 4, c = cell & 0x0F;
    radiolink::FireResultCode result = radiolink::FireResultCode::Miss;
    uint8_t shipId = 0xFF;
    if (r < kBoardSize && c < kBoardSize) {
        if (s_myHitGrid[r][c]) {
            result = radiolink::FireResultCode::Repeat; // already-hit cell, nothing new happens
        } else if (s_myFleetGrid[r][c]) {
            s_myHitGrid[r][c] = true;
            int idx = shipIndexAt(r, c);
            if (idx >= 0) {
                shipId = (uint8_t)idx;
                s_myShips[idx].hitCount++;
                s_myRemaining--;
                result = (s_myShips[idx].hitCount >= kShipSizes[idx]) ? radiolink::FireResultCode::Sunk
                                                                        : radiolink::FireResultCode::Hit;
            }
        }
    } else {
        result = radiolink::FireResultCode::Invalid;
    }
    s_lastResolvedTurn = turn;
    s_haveLastResult = true;
    s_lastResultCell = cell;
    s_lastResultCode = result;
    s_lastResultShipId = shipId;
    sendFireResultCached();

    // Turn number comes from the wire message, not a local counter, so both
    // sides stay consistent across a reboot on either end.
    s_turn = (uint8_t)(turn + 1);

    if (s_myRemaining <= 0) {
        s_overResult = OverResult::Lose;
        s_phase = Phase::GameOver;
        sendGameOver(false);
    } else {
        s_phase = Phase::MyTurn;
    }
}

static void applyMyShotResult(uint8_t cell, radiolink::FireResultCode result, uint8_t remaining) {
    uint8_t r = cell >> 4, c = cell & 0x0F;
    if (r >= kBoardSize || c >= kBoardSize) return;
    Cell newCell = Cell::Miss;
    if (result == radiolink::FireResultCode::Hit) newCell = Cell::Hit;
    else if (result == radiolink::FireResultCode::Sunk) newCell = Cell::Sunk;
    s_theirGrid[r][c] = newCell;
    s_theirRemaining = remaining;
    s_turn = (uint8_t)(s_turn + 1);
    if (s_theirRemaining <= 0) {
        s_overResult = OverResult::Win;
        s_phase = Phase::GameOver;
        sendGameOver(true);
    } else {
        s_phase = Phase::TheirTurn;
    }
}

static void onFireSendDone(bool acked, uint8_t /*seq*/, void* /*ctx*/) {
    // If Fire never got through, no FireResult will arrive either. Don't force
    // a phase change: the silent-peer watchdog in update() handles a dead link,
    // and staying in AwaitResult is honest if it was just a transient collision.
    (void)acked;
}

static void onJoinAcceptDone(bool acked, uint8_t /*seq*/, void* /*ctx*/) {
    if (!acked) {
        // Couldn't reach the joiner: release the session instead of being
        // stuck "in a game" with a peer that never got the accept.
        radiolink::closeSession();
        s_phase = Phase::Hosting;
    }
}

static void handleGameSync(const radiolink::Message& m) {
    if (m.payloadLen < 5) return;
    uint8_t theirTurn = m.payload[0];
    uint8_t whoseTurnHost = m.payload[1]; // 1 = it's host's turn, 0 = it's joiner's turn
    uint8_t theirRemaining = m.payload[2];
    // Only adopt if strictly ahead of what we have -- avoids two peers'
    // periodic heartbeats fighting each other when both are already in
    // agreement.
    if ((int8_t)(theirTurn - s_turn) <= 0) return;
    s_turn = theirTurn;
    s_theirRemaining = theirRemaining;
    bool myTurnNow = (whoseTurnHost != 0) == s_isHost;
    if (s_phase == Phase::MyTurn || s_phase == Phase::TheirTurn || s_phase == Phase::AwaitResult) {
        s_phase = myTurnNow ? Phase::MyTurn : Phase::TheirTurn;
    }
}

static void onMessage(const radiolink::Message& m, void* /*ctx*/) {
    // Badge Attack (main/glitch/glitch.h): Ship Battle is a valid target
    // whenever its radio is open, handled like Radio Chat's onMessage().
    // Not a game message, so checked before the switch below.
    if (m.type == Type::Attack) {
        if (storage::isSecureBadgeEnabled()) {
            radiolink::sendUnreliable(m.src, Type::AttackBlocked, 0, nullptr, 0);
        } else {
            radiolink::sendUnreliable(m.src, Type::AttackLanded, 0, nullptr, 0);
            glitch::trigger();
        }
        return;
    }
    switch (m.type) {
        case Type::HostAdv: {
            if (m.payloadLen < 3) return;
            uint8_t session = m.payload[0];
            uint8_t nameLen = m.payload[2];
            if (nameLen > 16 || (size_t)(3 + nameLen) > m.payloadLen) nameLen = 0;
            upsertHost(m.src, session, (const char*)&m.payload[3], nameLen, m.rssiDbm);
            break;
        }
        case Type::JoinReq: {
            if (s_phase != Phase::Hosting) return; // busy -- silently ignore, current game undisturbed
            if (m.payloadLen < 1) return;
            if (m.payload[0] != s_mySession) return;
            strncpy(s_oppLinkId, m.src, 6); s_oppLinkId[6] = 0;
            const radiolink::Peer* p = radiolink::findPeer(m.src);
            if (p && p->name[0]) { strncpy(s_oppName, p->name, 16); s_oppName[16] = 0; }
            else { snprintf(s_oppName, sizeof(s_oppName), "Badge %s", m.src); }
            radiolink::openSession(s_oppLinkId, s_mySession);
            uint8_t pay[9] = { s_mySession, (uint8_t)kBoardSize, (uint8_t)kBoardSize, 0, 1, 0, 0, 0, 0 };
            radiolink::sendReliable(s_oppLinkId, Type::JoinAccept, s_mySession, pay, 9, onJoinAcceptDone, nullptr);
            s_firstMoverIsHost = true;
            resetBoards();
            s_phase = Phase::Deploy;
            break;
        }
        case Type::JoinAccept: {
            if (s_phase != Phase::Joining) return;
            if (strcmp(m.src, s_oppLinkId) != 0) return;
            if (m.payloadLen < 5) return;
            s_mySession = m.payload[0];
            s_firstMoverIsHost = m.payload[4] != 0;
            radiolink::openSession(s_oppLinkId, s_mySession);
            resetBoards();
            s_phase = Phase::Deploy;
            break;
        }
        case Type::JoinReject: {
            if (s_phase != Phase::Joining) return;
            if (strcmp(m.src, s_oppLinkId) != 0) return;
            s_phase = Phase::JoinRejected;
            break;
        }
        case Type::FleetReady: {
            if (strcmp(m.src, s_oppLinkId) != 0) return;
            if (s_phase != Phase::Deploy && s_phase != Phase::WaitFleet) return;
            s_theirFleetReady = true;
            maybeStartTurns();
            break;
        }
        case Type::Fire: {
            if (strcmp(m.src, s_oppLinkId) != 0) return;
            if (m.payloadLen < 2) return;
            if (s_phase != Phase::TheirTurn && s_phase != Phase::MyTurn && s_phase != Phase::AwaitResult) return;
            handleIncomingFire(m.payload[0], m.payload[1]);
            break;
        }
        case Type::FireResult: {
            if (strcmp(m.src, s_oppLinkId) != 0) return;
            if (s_phase != Phase::AwaitResult) return;
            if (m.payloadLen < 5) return;
            if (m.payload[0] != s_turn) return; // stale duplicate -- transport dedup should already catch this, belt-and-suspenders
            applyMyShotResult(m.payload[1], (radiolink::FireResultCode)m.payload[2], m.payload[4]);
            break;
        }
        case Type::GameSync: {
            if (strcmp(m.src, s_oppLinkId) != 0) return;
            handleGameSync(m);
            break;
        }
        case Type::GameOver: {
            if (strcmp(m.src, s_oppLinkId) != 0) return;
            if (m.payloadLen < 2) return;
            bool hostWon = m.payload[1] != 0;
            s_overResult = (hostWon == s_isHost) ? OverResult::Win : OverResult::Lose;
            s_phase = Phase::GameOver;
            break;
        }
        case Type::Forfeit: {
            if (strcmp(m.src, s_oppLinkId) != 0) return;
            s_overResult = OverResult::OpponentForfeited;
            s_phase = Phase::GameOver;
            break;
        }
        default: break;
    }
}

// ---- public API -----------------------------------------------------------

bool begin(const char* displayName) {
    s_phase = Phase::Idle;
    s_hostCount = 0;
    s_oppLinkId[0] = 0;
    s_oppName[0] = 0;
    if (!radiolink::begin(displayName)) return false;
    radiolink::setRxHandler(onMessage, nullptr);
    radiolink::setBeaconEnabled(false); // HostAdv/direct exchange covers presence -- see radio_link.h's own note
    return true;
}

void end() {
    radiolink::closeSession();
    radiolink::end();
    s_phase = Phase::Idle;
    s_hostCount = 0;
}

void update() {
    radiolink::update();
    pruneStaleHosts();

    unsigned long now = millis();
    if (s_phase == Phase::Hosting && now - s_lastHostAdvMs >= kHostAdvIntervalMs) {
        s_lastHostAdvMs = now;
        uint8_t nameLen = (uint8_t)strlen(s_myName);
        uint8_t pay[3 + 16];
        pay[0] = s_mySession;
        pay[1] = 0; // rules, reserved
        pay[2] = nameLen;
        memcpy(&pay[3], s_myName, nameLen);
        radiolink::sendUnreliable(nullptr, Type::HostAdv, 0, pay, (uint8_t)(3 + nameLen));
    }

    if (s_phase == Phase::WaitFleet && now - s_lastFleetReadySentMs >= kFleetReadyResendMs) {
        s_lastFleetReadySentMs = now;
        uint8_t pay[2] = { 0, 0 };
        radiolink::sendReliable(s_oppLinkId, Type::FleetReady, s_mySession, pay, 2, nullptr, nullptr);
    }

    bool inGame = (s_phase == Phase::WaitFleet || s_phase == Phase::MyTurn ||
                   s_phase == Phase::AwaitResult || s_phase == Phase::TheirTurn);
    if (inGame) {
        if (now - s_lastGameSyncMs >= kGameSyncIntervalMs) {
            s_lastGameSyncMs = now;
            bool hostTurnNow = (s_phase == Phase::MyTurn) == s_isHost;
            uint8_t pay[5] = { s_turn, (uint8_t)(hostTurnNow ? 1 : 0), (uint8_t)s_theirRemaining, 0, 0 };
            radiolink::sendUnreliable(s_oppLinkId, Type::GameSync, s_mySession, pay, 5);
        }
        if (radiolink::sessionSilentMs() >= kAbandonMs) {
            s_overResult = OverResult::OpponentLost;
            s_phase = Phase::Aborted;
        }
    }
}

int hostCount() { return s_hostCount; }
const HostInfo& hostAt(int i) { return s_hosts[i]; }

void startHosting(const char* myName) {
    s_isHost = true;
    s_mySession = (uint8_t)((esp_random() % 254) + 1); // avoid 0 (sessionless)
    strncpy(s_myName, myName ? myName : "", 16); s_myName[16] = 0;
    s_oppName[0] = 0; // filled in once a JoinReq actually names an opponent
    s_lastHostAdvMs = 0; // fire the first advert immediately
    s_phase = Phase::Hosting;
}

void joinHost(const char* peerLinkId, uint8_t session, const char* myName) {
    s_isHost = false;
    s_mySession = session;
    strncpy(s_oppLinkId, peerLinkId, 6); s_oppLinkId[6] = 0;
    strncpy(s_myName, myName ? myName : "", 16); s_myName[16] = 0;
    // Best-effort opponent name from the already-scanned host list.
    for (int i = 0; i < s_hostCount; i++) {
        if (strncmp(s_hosts[i].linkId, peerLinkId, 6) == 0) {
            strncpy(s_oppName, s_hosts[i].name, 16); s_oppName[16] = 0;
            break;
        }
    }
    uint8_t pay[1] = { session };
    radiolink::sendReliable(s_oppLinkId, Type::JoinReq, session, pay, 1, nullptr, nullptr);
    s_phase = Phase::Joining;
}

void cancelHostOrJoin() {
    radiolink::closeSession();
    s_phase = Phase::Idle;
}

bool placeShip(int shipIndex, uint8_t row, uint8_t col, bool horizontal) {
    if (shipIndex < 0 || shipIndex >= kShipCount) return false;
    uint8_t cells[5];
    if (!computeShipCells(shipIndex, row, col, horizontal, cells)) return false;
    // Temporarily clear this ship's own old footprint so re-placing it
    // doesn't collide with itself.
    bool hadOld = s_myShips[shipIndex].placed;
    uint8_t oldCells[5];
    bool hasOldCells = hadOld && computeShipCells(shipIndex, s_myShips[shipIndex].row, s_myShips[shipIndex].col,
                                                   s_myShips[shipIndex].horizontal, oldCells);
    if (hasOldCells) {
        for (uint8_t k = 0; k < kShipSizes[shipIndex]; k++) {
            s_myFleetGrid[oldCells[k] >> 4][oldCells[k] & 0x0F] = false;
        }
    }
    for (uint8_t k = 0; k < kShipSizes[shipIndex]; k++) {
        if (s_myFleetGrid[cells[k] >> 4][cells[k] & 0x0F]) {
            // Collision -- restore old footprint before failing.
            if (hasOldCells) {
                for (uint8_t j = 0; j < kShipSizes[shipIndex]; j++) {
                    s_myFleetGrid[oldCells[j] >> 4][oldCells[j] & 0x0F] = true;
                }
            }
            return false;
        }
    }
    for (uint8_t k = 0; k < kShipSizes[shipIndex]; k++) {
        s_myFleetGrid[cells[k] >> 4][cells[k] & 0x0F] = true;
    }
    s_myShips[shipIndex] = { true, row, col, horizontal, 0 };
    return true;
}

void autoPlaceFleet() {
    clearFleet();
    for (int i = 0; i < kShipCount; i++) {
        bool ok = false;
        for (int attempt = 0; attempt < 200 && !ok; attempt++) {
            uint8_t row = (uint8_t)(esp_random() % kBoardSize);
            uint8_t col = (uint8_t)(esp_random() % kBoardSize);
            bool horiz = (esp_random() % 2) == 0;
            ok = placeShip(i, row, col, horiz);
        }
    }
}

void clearFleet() {
    memset(s_myShips, 0, sizeof(s_myShips));
    memset(s_myFleetGrid, 0, sizeof(s_myFleetGrid));
}

bool shipPlaced(int shipIndex) {
    if (shipIndex < 0 || shipIndex >= kShipCount) return false;
    return s_myShips[shipIndex].placed;
}

bool fleetComplete() {
    for (int i = 0; i < kShipCount; i++) if (!s_myShips[i].placed) return false;
    return true;
}

void confirmFleetReady() {
    if (!fleetComplete()) return;
    if (s_phase != Phase::Deploy) return;
    s_myFleetReadySent = true;
    uint8_t pay[2] = { 0, 0 }; // fleet hash -- optional integrity nicety, not required for correctness, left zeroed
    radiolink::sendReliable(s_oppLinkId, Type::FleetReady, s_mySession, pay, 2, nullptr, nullptr);
    s_lastFleetReadySentMs = millis(); // see kFleetReadyResendMs's comment
    s_phase = Phase::WaitFleet;
    maybeStartTurns();
}

bool fireAt(uint8_t row, uint8_t col) {
    if (s_phase != Phase::MyTurn) return false;
    if (row >= kBoardSize || col >= kBoardSize) return false;
    if (s_theirGrid[row][col] != Cell::Empty) return false;
    uint8_t cell = (uint8_t)((row << 4) | col);
    uint8_t pay[2] = { s_turn, cell };
    radiolink::SendResult r = radiolink::sendReliable(s_oppLinkId, Type::Fire, s_mySession, pay, 2,
                                                      onFireSendDone, nullptr);
    if (r != radiolink::SendResult::Sent) return false;
    s_pendingFireCell = cell;
    s_phase = Phase::AwaitResult;
    return true;
}

void forfeit() {
    uint8_t pay[1] = { (uint8_t)radiolink::OverReason::Forfeit };
    radiolink::sendUnreliable(s_oppLinkId, Type::Forfeit, s_mySession, pay, 1);
    radiolink::sendUnreliable(s_oppLinkId, Type::Forfeit, s_mySession, pay, 1);
    s_overResult = OverResult::YouForfeited;
    s_phase = Phase::GameOver;
}

Phase phase() { return s_phase; }
bool isHost() { return s_isHost; }
const char* opponentLinkId() { return s_oppLinkId; }
const char* opponentName() { return s_oppName; }

Cell myCell(uint8_t row, uint8_t col) {
    if (row >= kBoardSize || col >= kBoardSize) return Cell::Empty;
    bool hasShip = s_myFleetGrid[row][col];
    bool hit = s_myHitGrid[row][col];
    if (!hasShip) return hit ? Cell::Miss : Cell::Empty;
    if (!hit) return Cell::Ship;
    int idx = shipIndexAt(row, col);
    if (idx >= 0 && s_myShips[idx].hitCount >= kShipSizes[idx]) return Cell::Sunk;
    return Cell::Hit;
}

Cell theirCell(uint8_t row, uint8_t col) {
    if (row >= kBoardSize || col >= kBoardSize) return Cell::Empty;
    return s_theirGrid[row][col];
}

int myShipsRemaining() { return s_myRemaining; }
int theirShipsRemaining() { return s_theirRemaining; }
OverResult overResult() { return s_overResult; }
unsigned long silentMs() { return radiolink::sessionSilentMs(); }
uint8_t pendingFireCell() { return s_pendingFireCell; }

} // namespace shipnet

#include "radio_link.h"
#include "lora.h"
#include "frame_codec.h"
#include "../settings/badge_id.h"
#include "../storage/storage.h"
#include <esp_random.h>
#include <cstring>

namespace radiolink {

// ---- addressing helpers (this file only; framecodec never decodes callsigns back out) ----

static void addrToCallsign(const uint8_t addr[framecodec::kAddrBytes], char out[7]) {
    for (int i = 0; i < 6; i++) out[i] = (char)(addr[i] >> 1);
    out[6] = 0;
    for (int i = 5; i >= 0 && out[i] == ' '; i--) out[i] = 0; // trim the callsign's own space padding
}

static uint8_t addrSsid(const uint8_t addr[framecodec::kAddrBytes]) {
    return (addr[6] >> 1) & 0x0F;
}

// ---- state --------------------------------------------------------------

static bool   s_active = false;
static char   s_myLinkId[7] = {0};
static String s_myName;
static uint8_t s_caps = 0;
static uint8_t s_txSeq = 0;

enum class TxState : uint8_t { Idle, OnAir };
static TxState s_txState = TxState::Idle;
static unsigned long s_txStartMs = 0;
static unsigned long s_txDeadlineMs = 0;

struct PendingSend {
    bool active;
    char destLinkId[7];
    Type type;
    uint8_t seq;
    uint8_t session;
    uint8_t payload[kMaxPayload];
    uint8_t payloadLen;
    int attempt;
    unsigned long deadlineMs;
    SendDoneFn done;
    void* ctx;
};
static PendingSend s_pending = {};

struct AckPending {
    bool active;
    char peerId[7];
    uint8_t seq;
    unsigned long dueAtMs;
};
static AckPending s_ackPending = {};

constexpr unsigned long kAckDelayMs    = 150;  // piggyback window before falling back to a bare Ack
constexpr unsigned long kAckTimeoutMs  = 800;  // base retry deadline (SF7 airtime + delay + reply + slack)
constexpr int           kMaxTxAttempts = 4;    // 1 original + 3 retries
constexpr unsigned long kRetryJitterMs = 250;  // linear backoff + jitter -- see radio_link.h's header comment

constexpr int kMaxPeers = 24;
static Peer s_peers[kMaxPeers];
static int  s_peerCount = 0;

constexpr int kDedupeWindow = 48;
static uint32_t s_seen[kDedupeWindow];
static bool     s_seenValid[kDedupeWindow];
static uint8_t  s_seenHead = 0;

constexpr unsigned long kPeerStaleMs           = 45000;
constexpr unsigned long kBeaconIntervalIdleMs  = 18000;
constexpr unsigned long kBeaconIntervalFastMs  = 5000;
constexpr unsigned long kIdlePingMs            = 8000;

static bool s_beaconEnabled = false;
static unsigned long s_beaconIntervalMs = kBeaconIntervalIdleMs;
static unsigned long s_nextBeaconAtMs = 0;

struct SessionState {
    bool open;
    char peerId[7];
    uint8_t id;
    unsigned long lastHeardMs;
    unsigned long lastSentMs;
};
static SessionState s_session = {};

static RxHandler   s_rxHandler = nullptr;
static void*       s_rxCtx = nullptr;
static TxIndicator s_txIndicatorFn = nullptr;
static void*       s_txIndicatorCtx = nullptr;

// ---- small helpers --------------------------------------------------

static void touchSessionSent(const char* destLinkId) {
    if (destLinkId && s_session.open && strcmp(destLinkId, s_session.peerId) == 0) {
        s_session.lastSentMs = millis();
    }
}

static bool startFrameTx(const String& frame, const char* destLinkId = nullptr) {
    if (!rf::beginTransmit(frame)) return false;
    s_txState = TxState::OnAir;
    s_txStartMs = millis();
    s_txDeadlineMs = rf::airtimeMs(frame.length()) * 3 + 50;
    touchSessionSent(destLinkId);
    return true;
}

// Builds a complete on-air frame: 6-byte sub-header + payload, wrapped in
// framecodec's AX.25-style framing. destLinkId == nullptr broadcasts.
static String buildLinkFrame(const char* destLinkId, Type t, uint8_t seq, uint8_t flags,
                              uint8_t session, uint8_t ackSeq, const uint8_t* pay, uint8_t len) {
    uint8_t info[kHeaderBytes + kMaxPayload];
    info[0] = (uint8_t)((kVersion << 5) | ((uint8_t)t & 0x1F));
    info[1] = seq;
    info[2] = flags;
    info[3] = session;
    info[4] = ackSeq;
    info[5] = len;
    if (len) memcpy(info + kHeaderBytes, pay, len);

    uint8_t destAddr[framecodec::kAddrBytes], srcAddr[framecodec::kAddrBytes];
    framecodec::buildAddress(destLinkId ? destLinkId : kBroadcastId, kSsidBadgeLink, /*last=*/false, destAddr);
    framecodec::buildAddress(s_myLinkId, kSsidBadgeLink, /*last=*/true, srcAddr);
    return framecodec::buildFrame(destAddr, srcAddr, kPid, info, (size_t)kHeaderBytes + len);
}

static uint8_t piggybackFlagsFor(const char* peerId, uint8_t& ackSeqOut) {
    if (s_ackPending.active && peerId && strcmp(s_ackPending.peerId, peerId) == 0) {
        ackSeqOut = s_ackPending.seq;
        return kFlagIsAck;
    }
    return 0;
}

// (Re)transmits s_pending. Shared by the initial attempt and retries so they can't drift apart.
static bool transmitPending(bool isRetry) {
    uint8_t ackSeq = 0;
    uint8_t flags = kFlagAckReq | piggybackFlagsFor(s_pending.destLinkId, ackSeq);
    if (isRetry) flags |= kFlagRetry;
    String frame = buildLinkFrame(s_pending.destLinkId, s_pending.type, s_pending.seq, flags,
                                   s_pending.session, ackSeq, s_pending.payload, s_pending.payloadLen);
    if (!startFrameTx(frame, s_pending.destLinkId)) return false;
    if (flags & kFlagIsAck) s_ackPending.active = false; // consumed
    return true;
}

static void scheduleAck(const char* peerId, uint8_t seq) {
    // Single-slot: a second owed ack overwrites this one; sender's retry will re-request it.
    strncpy(s_ackPending.peerId, peerId, 6); s_ackPending.peerId[6] = 0;
    s_ackPending.seq = seq;
    s_ackPending.dueAtMs = millis() + kAckDelayMs;
    s_ackPending.active = true;
}

static void checkPiggybackAck(const Message& m) {
    if (!s_pending.active) return;
    if (m.ackSeq != s_pending.seq) return;
    if (strcmp(m.src, s_pending.destLinkId) != 0) return;
    SendDoneFn done = s_pending.done;
    void* ctx = s_pending.ctx;
    uint8_t seq = s_pending.seq;
    s_pending.active = false;
    if (done) done(true, seq, ctx);
}

static uint32_t linkIdToU24(const char* id) {
    uint32_t v = 0;
    for (int i = 0; i < 6; i++) {
        char c = id[i];
        uint8_t nib;
        if (c >= '0' && c <= '9') nib = (uint8_t)(c - '0');
        else if (c >= 'A' && c <= 'F') nib = (uint8_t)(c - 'A' + 10);
        else nib = 0; // malformed id -- degrades to a weaker (but still safe) dedupe key, never a crash
        v = (v << 4) | nib;
    }
    return v & 0xFFFFFF;
}

static bool seenBefore(uint32_t key) {
    for (int i = 0; i < kDedupeWindow; i++) {
        if (s_seenValid[i] && s_seen[i] == key) return true;
    }
    s_seen[s_seenHead] = key;
    s_seenValid[s_seenHead] = true;
    s_seenHead = (uint8_t)((s_seenHead + 1) % kDedupeWindow);
    return false;
}

// Matches a discovered link ID against PeerDrop's stored contacts by MAC --
// getLinkId() is the last 3 octets of this badge's BT MAC.
static bool contactMacMatchesLinkId(const String& contactMac, const char* linkId) {
    if (contactMac.length() != 17) return false; // not a well-formed "aa:bb:cc:dd:ee:ff"
    String last3 = contactMac.substring(9);       // "dd:ee:ff"
    String stripped;
    for (size_t i = 0; i < last3.length(); i++) {
        if (last3[i] != ':') stripped += last3[i];
    }
    stripped.toUpperCase();
    return stripped == String(linkId);
}

static bool lookupContactName(const char* linkId, String& outName) {
    int n = storage::contactCount();
    for (int i = 0; i < n; i++) {
        storage::Contact c = storage::loadContact(i);
        if (contactMacMatchesLinkId(c.mac, linkId)) {
            outName = c.identity.name;
            return true;
        }
    }
    return false;
}

static Peer* touchPeer(const char* linkId, int16_t rssi) {
    if (strcmp(linkId, s_myLinkId) == 0) return nullptr; // never file ourselves as a peer
    for (int i = 0; i < s_peerCount; i++) {
        if (strcmp(s_peers[i].linkId, linkId) == 0) {
            s_peers[i].rssiDbm = rssi;
            s_peers[i].lastHeardMs = millis();
            return &s_peers[i];
        }
    }
    int idx;
    if (s_peerCount < kMaxPeers) {
        idx = s_peerCount++;
    } else {
        // LRU-evict the stalest entry rather than refuse new peers.
        idx = 0;
        for (int i = 1; i < kMaxPeers; i++) {
            if (s_peers[i].lastHeardMs < s_peers[idx].lastHeardMs) idx = i;
        }
    }
    Peer& p = s_peers[idx];
    strncpy(p.linkId, linkId, 6); p.linkId[6] = 0;
    p.name[0] = 0;
    p.rssiDbm = rssi;
    p.lastHeardMs = millis();
    p.caps = 0;
    p.hostSession = 0;
    String contactName;
    if (lookupContactName(linkId, contactName)) {
        p.knownContact = true;
        strncpy(p.name, contactName.c_str(), 16); p.name[16] = 0;
    } else {
        p.knownContact = false;
    }
    return &p;
}

static void handleBeacon(const Message& m) {
    // payload: [nameLen:1][name:0..16][caps:1]
    if (m.payloadLen < 2) return;
    uint8_t nameLen = m.payload[0];
    if (nameLen > 16 || (size_t)(1 + nameLen + 1) > m.payloadLen) return;
    Peer* p = touchPeer(m.src, m.rssiDbm);
    if (!p) return;
    if (nameLen > 0 && !p->knownContact) { // a saved-contact name (if any) wins over a self-reported one
        memcpy(p->name, m.payload + 1, nameLen);
        p->name[nameLen] = 0;
    }
    p->caps = m.payload[1 + nameLen];
}

static void sendBeacon() {
    if (s_txState != TxState::Idle || s_pending.active) return; // routine presence never preempts a real send
    uint8_t pay[1 + 16 + 1];
    uint8_t nameLen = (uint8_t)(s_myName.length() > 16 ? 16 : s_myName.length());
    pay[0] = nameLen;
    memcpy(pay + 1, s_myName.c_str(), nameLen);
    pay[1 + nameLen] = s_caps;
    String frame = buildLinkFrame(nullptr, Type::Beacon, s_txSeq++, 0, 0, 0, pay, (uint8_t)(1 + nameLen + 1));
    startFrameTx(frame);
}

static void handleBeaconReq() {
    if (s_beaconEnabled && !s_session.open) {
        s_nextBeaconAtMs = millis() + (esp_random() % 300); // small jitter so a poke doesn't make every listener answer in lockstep
    }
}

static void handleReceived(const String& raw) {
    uint8_t destAddr[framecodec::kAddrBytes], srcAddr[framecodec::kAddrBytes], pid;
    uint8_t info[kHeaderBytes + kMaxPayload + 8];
    size_t infoLen = 0;
    if (!framecodec::parseFrame(raw, destAddr, srcAddr, pid, info, sizeof(info), infoLen)) return;
    if (pid != kPid) return; // not badge-link traffic (missions/satellite use framecodec::kPid)
    if (addrSsid(destAddr) != kSsidBadgeLink || addrSsid(srcAddr) != kSsidBadgeLink) return;
    if (!framecodec::verifyFcs(raw)) return; // corrupt/truncated
    if (infoLen < kHeaderBytes) return;

    uint8_t verType = info[0];
    if ((verType >> 5) != kVersion) return; // future protocol version -- drop quietly, don't misparse

    uint8_t typeVal  = verType & 0x1F;
    uint8_t seq      = info[1];
    uint8_t flags    = info[2];
    uint8_t session  = info[3];
    uint8_t ackSeq   = info[4];
    uint8_t payLen   = info[5];
    if ((size_t)payLen != infoLen - kHeaderBytes) return; // self-consistency -- catches parseFrame's silent truncation
    if (payLen > kMaxPayload) return;

    char srcId[7], destId[7];
    addrToCallsign(srcAddr, srcId);
    addrToCallsign(destAddr, destId);
    bool broadcast = (strcmp(destId, kBroadcastId) == 0);
    if (!broadcast && strcmp(destId, s_myLinkId) != 0) return; // not addressed to us

    Message m;
    memcpy(m.src, srcId, 7);
    memcpy(m.dest, destId, 7);
    m.broadcast = broadcast;
    m.type = (Type)typeVal;
    m.seq = seq; m.flags = flags; m.session = session; m.ackSeq = ackSeq;
    m.payloadLen = payLen;
    if (payLen) memcpy(m.payload, info + kHeaderBytes, payLen);
    m.rssiDbm = rf::lastRssiDbm();
    m.rxAtMs = millis();

    // Session isolation: a nonzero session must match (peer, session)
    // exactly, so unrelated games sharing a session id don't cross-talk.
    if (s_session.open && session != 0) {
        if (strcmp(m.src, s_session.peerId) != 0 || session != s_session.id) return;
    }
    if (s_session.open && strcmp(m.src, s_session.peerId) == 0) {
        s_session.lastHeardMs = millis();
    }

    // Any peer heard from gets RSSI/last-heard refreshed, not just beacon
    // senders, so a session peer (which suppresses beaconing) never looks stale.
    touchPeer(m.src, m.rssiDbm);

    // Owe an ack BEFORE the dedupe check: a duplicate usually means our
    // previous ack was lost, so it's still owed even without re-delivering.
    if (flags & kFlagAckReq) scheduleAck(m.src, m.seq);

    uint32_t key = (linkIdToU24(m.src) << 8) | m.seq;
    if (seenBefore(key)) return; // duplicate -- already (re-)acked above, not delivered again

    if (flags & kFlagIsAck) checkPiggybackAck(m);

    switch (m.type) {
        case Type::Beacon:    handleBeacon(m); return;
        case Type::BeaconReq: handleBeaconReq(); return;
        case Type::Ack:       return; // already handled via checkPiggybackAck above (every Ack we send carries kFlagIsAck)
        case Type::Ping:      return; // pure liveness -- peer/session already touched above
        default: break;
    }

    if (s_rxHandler) s_rxHandler(m, s_rxCtx);
}

// ---- public API -----------------------------------------------------

bool begin(const char* displayName) {
    if (!rf::selfTest()) return false;
    if (!rf::applyProfile(rf::kBadgeLinkProfile)) return false;

    s_peerCount = 0;
    memset(s_seenValid, 0, sizeof(s_seenValid));
    s_seenHead = 0;
    s_pending = PendingSend{};
    s_ackPending = AckPending{};
    s_session = SessionState{};
    s_txState = TxState::Idle;

    strncpy(s_myLinkId, badge_id::getLinkId().c_str(), 6);
    s_myLinkId[6] = 0;

    s_myName = (displayName && displayName[0]) ? String(displayName) : (String("Badge ") + s_myLinkId);
    if (s_myName.length() > 16) s_myName = s_myName.substring(0, 16);
    for (size_t i = 0; i < s_myName.length(); i++) {
        char c = s_myName[i];
        if (c < 0x20 || c > 0x7E) s_myName.setCharAt(i, '?');
    }

    s_caps = 0;
    s_beaconEnabled = true;
    s_beaconIntervalMs = kBeaconIntervalIdleMs;
    s_nextBeaconAtMs = millis() + (esp_random() % 1000);
    s_active = true;
    return true;
}

void end() {
    if (!s_active) return;
    s_session.open = false;
    s_pending.active = false;
    s_ackPending.active = false;
    s_beaconEnabled = false;
    if (s_txState == TxState::OnAir) {
        rf::endTransmit();
        s_txState = TxState::Idle;
    }
    s_active = false;
    rf::applyProfile(rf::kMissionProfile);
}

bool isActive() { return s_active; }

void update() {
    if (!s_active) return;

    // 1. drain RX -- one pollReceive() per tick, matching rf::'s own contract.
    String raw;
    if (rf::pollReceive(raw)) handleReceived(raw);

    // 2. TX pump.
    if (s_txState == TxState::OnAir) {
        if (rf::txComplete() || millis() - s_txStartMs > s_txDeadlineMs) {
            rf::endTransmit();
            s_txState = TxState::Idle;
        }
    }
    if (s_txIndicatorFn) s_txIndicatorFn(s_txState == TxState::OnAir, s_txIndicatorCtx);

    // 3. delayed/bare ack.
    if (s_ackPending.active && millis() >= s_ackPending.dueAtMs &&
        s_txState == TxState::Idle && !s_pending.active) {
        uint8_t pay[1] = { (uint8_t)AckStatus::Ok };
        String frame = buildLinkFrame(s_ackPending.peerId, Type::Ack, s_txSeq++, kFlagIsAck, 0,
                                       s_ackPending.seq, pay, 1);
        if (startFrameTx(frame, s_ackPending.peerId)) s_ackPending.active = false;
    }

    // 4. reliable-send retry/timeout.
    if (s_pending.active && s_txState == TxState::Idle && millis() >= s_pending.deadlineMs) {
        s_pending.attempt++;
        if (s_pending.attempt >= kMaxTxAttempts) {
            SendDoneFn done = s_pending.done;
            void* ctx = s_pending.ctx;
            uint8_t seq = s_pending.seq;
            s_pending.active = false;
            if (done) done(false, seq, ctx);
        } else if (transmitPending(true)) {
            s_pending.deadlineMs = millis() + kAckTimeoutMs * (s_pending.attempt + 1) +
                                    (esp_random() % kRetryJitterMs);
        } else {
            s_pending.attempt--; // radio was busy this tick -- don't burn a retry, just try again next tick
        }
    }

    // 5. session idle ping.
    if (s_session.open && s_txState == TxState::Idle && !s_pending.active &&
        millis() - s_session.lastSentMs > kIdlePingMs) {
        sendUnreliable(s_session.peerId, Type::Ping, s_session.id, nullptr, 0);
    }

    // 6. beacon (adaptive interval -- see setBeaconIntervalMs()).
    if (s_beaconEnabled && !s_session.open && s_txState == TxState::Idle && !s_pending.active &&
        millis() >= s_nextBeaconAtMs) {
        sendBeacon();
        s_nextBeaconAtMs = millis() + s_beaconIntervalMs + (esp_random() % 2000);
    }

    // 7. peer aging -- gated to ~1/s; the table is tiny but there's no
    // reason to walk it every single tick.
    static unsigned long s_lastAgeMs = 0;
    if (millis() - s_lastAgeMs > 1000) {
        s_lastAgeMs = millis();
        for (int i = 0; i < s_peerCount; ) {
            if (millis() - s_peers[i].lastHeardMs > kPeerStaleMs) {
                for (int j = i; j < s_peerCount - 1; j++) s_peers[j] = s_peers[j + 1];
                s_peerCount--;
            } else {
                i++;
            }
        }
    }
}

void setRxHandler(RxHandler fn, void* ctx) { s_rxHandler = fn; s_rxCtx = ctx; }

SendResult sendUnreliable(const char* destLinkId, Type t, uint8_t session,
                          const uint8_t* payload, uint8_t len) {
    if (!s_active) return SendResult::Inactive;
    if (len > kMaxPayload) return SendResult::TooLong;
    if (s_txState != TxState::Idle) return SendResult::Busy;
    uint8_t ackSeq = 0;
    uint8_t flags = piggybackFlagsFor(destLinkId, ackSeq);
    String frame = buildLinkFrame(destLinkId, t, s_txSeq, flags, session, ackSeq, payload, len);
    if (!startFrameTx(frame, destLinkId)) return SendResult::Busy;
    s_txSeq++;
    if (flags & kFlagIsAck) s_ackPending.active = false;
    return SendResult::Sent;
}

SendResult sendReliable(const char* destLinkId, Type t, uint8_t session,
                        const uint8_t* payload, uint8_t len,
                        SendDoneFn done, void* ctx) {
    if (!s_active) return SendResult::Inactive;
    if (!destLinkId || len > kMaxPayload) return SendResult::TooLong;
    if (s_txState != TxState::Idle || s_pending.active) return SendResult::Busy;

    s_pending.active = true;
    strncpy(s_pending.destLinkId, destLinkId, 6); s_pending.destLinkId[6] = 0;
    s_pending.type = t;
    s_pending.seq = s_txSeq++;
    s_pending.session = session;
    s_pending.payloadLen = len;
    if (len) memcpy(s_pending.payload, payload, len);
    s_pending.attempt = 0;
    s_pending.done = done;
    s_pending.ctx = ctx;

    if (!transmitPending(false)) {
        s_pending.active = false;
        return SendResult::Busy;
    }
    s_pending.deadlineMs = millis() + kAckTimeoutMs + (esp_random() % kRetryJitterMs);
    return SendResult::Sent;
}

bool    txBusy()         { return s_txState == TxState::OnAir || s_pending.active; }
uint8_t pendingSeq()     { return s_pending.seq; }
int     pendingAttempt() { return s_pending.attempt; }

void setTxIndicator(TxIndicator fn, void* ctx) { s_txIndicatorFn = fn; s_txIndicatorCtx = ctx; }

void setBeaconEnabled(bool on) {
    s_beaconEnabled = on;
    if (on) s_nextBeaconAtMs = millis() + (esp_random() % 1000);
}

void setBeaconIntervalMs(unsigned long ms) { s_beaconIntervalMs = ms; }
void setCaps(uint8_t caps) { s_caps = caps; }

void requestScan() {
    if (!s_active) return;
    if (s_txState == TxState::Idle && !s_pending.active) {
        String frame = buildLinkFrame(nullptr, Type::BeaconReq, s_txSeq++, 0, 0, 0, nullptr, 0);
        startFrameTx(frame);
    }
    s_beaconIntervalMs = kBeaconIntervalFastMs; // caller restores the idle interval when its picker screen closes
}

int peerCount() {
    // Sorted by RSSI desc, then name -- table is tiny, insertion sort each call is negligible.
    for (int i = 1; i < s_peerCount; i++) {
        Peer key = s_peers[i];
        int j = i - 1;
        while (j >= 0 && (s_peers[j].rssiDbm < key.rssiDbm ||
                          (s_peers[j].rssiDbm == key.rssiDbm && strcmp(s_peers[j].name, key.name) > 0))) {
            s_peers[j + 1] = s_peers[j];
            j--;
        }
        s_peers[j + 1] = key;
    }
    return s_peerCount;
}

const Peer& peerAt(int i) {
    if (i < 0) i = 0;
    if (s_peerCount > 0 && i >= s_peerCount) i = s_peerCount - 1;
    return s_peers[i];
}

const Peer* findPeer(const char* linkId) {
    for (int i = 0; i < s_peerCount; i++) {
        if (strcmp(s_peers[i].linkId, linkId) == 0) return &s_peers[i];
    }
    return nullptr;
}

void openSession(const char* peerLinkId, uint8_t sessionId) {
    s_session.open = true;
    strncpy(s_session.peerId, peerLinkId, 6); s_session.peerId[6] = 0;
    s_session.id = sessionId;
    s_session.lastHeardMs = millis();
    s_session.lastSentMs = millis();
    s_beaconEnabled = false; // a HostAdv/direct exchange already announces presence -- no need to also beacon
}

void closeSession() {
    s_session.open = false;
    s_beaconEnabled = true;
    s_nextBeaconAtMs = millis() + (esp_random() % 1000);
}

bool sessionOpen() { return s_session.open; }
const char* sessionPeer() { return s_session.peerId; }
uint8_t sessionId() { return s_session.id; }
unsigned long sessionSilentMs() { return millis() - s_session.lastHeardMs; }

const char* myLinkId() { return s_myLinkId; }

} // namespace radiolink

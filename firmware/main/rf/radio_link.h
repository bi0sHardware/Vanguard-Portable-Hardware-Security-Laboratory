#pragma once
#include <Arduino.h>
#include "radio_link_types.h"

// Shared badge-to-badge link layer for Radio Chat and Ship Battle. Owns the
// LoRa radio while active (switches to kBadgeLinkProfile in begin(),
// restores kMissionProfile in end()), and gives callers a validated,
// deduplicated, optionally-acknowledged message stream keyed on 6-hex-char
// badge link IDs, plus a discovered-peer table from passive beaconing.
//
// Callers know nothing about LoRa/SF/AX.25/hex encoding. Exactly one caller
// active at a time -- main.cpp's enterState() calls end() on every
// transition away from RadioChat/ShipBattle.
//
// Radio Chat and Ship Battle are independent consumers -- this header must
// never depend on either app, and neither may reach into the other's state.
namespace radiolink {

struct Message {
    char    src[7];        // sender link ID, NUL-terminated
    char    dest[7];        // "ALLBDG" for broadcast
    bool    broadcast;
    Type    type;
    uint8_t seq, flags, session, ackSeq;
    uint8_t payload[kMaxPayload];
    uint8_t payloadLen;
    int16_t rssiDbm;
    unsigned long rxAtMs;
};

struct Peer {
    char    linkId[7];
    char    name[17];       // from its BEACON, or a matched PeerDrop contact's name
    int16_t rssiDbm;
    unsigned long lastHeardMs;
    bool    knownContact;   // linkId matched a stored PeerDrop contact's MAC
    uint8_t caps;           // app-defined bits from setCaps()
    uint8_t hostSession;    // non-zero if peer is advertising a game (see HostAdv)
};

enum class SendResult : uint8_t { Sent, Busy, TooLong, Inactive };

using RxHandler   = void (*)(const Message& m, void* ctx);
using SendDoneFn  = void (*)(bool acked, uint8_t seq, void* ctx);
using TxIndicator = void (*)(bool onAir, void* ctx);

// ---- lifecycle -----------------------------------------------------------

// Switches radio to the badge channel and starts beaconing. Returns false
// (radio left unchanged) if unavailable or the profile switch fails --
// callers must show "Radio Offline" and not proceed. `displayName`
// truncated to 16 chars, non-printables replaced; empty falls back to "Badge <linkId>".
bool begin(const char* displayName);
void end();          // idempotent; stops beaconing, closes any session, restores kMissionProfile
bool isActive();

// MUST be called first in the owning screen's frame(), before drawing --
// drives RX polling, TX/ack/retry state machines, beaconing, peer aging,
// and session liveness.
void update();

// ---- receive ---------------------------------------------------------
// Invoked only for messages passing full validation (parses, correct
// PID/SSID, FCS OK, self-consistent header, known version, addressed to us
// or broadcast, matching session if one is open). Link-internal types
// (Ack, Ping, Beacon, BeaconReq) are consumed here, never delivered.
void setRxHandler(RxHandler fn, void* ctx);

// ---- transmit ----------------------------------------------------------
// Fire-and-forget: no ACK, no retries. Used for beacons and broadcast
// messages. destLinkId = nullptr broadcasts.
SendResult sendUnreliable(const char* destLinkId, Type t, uint8_t session,
                          const uint8_t* payload, uint8_t len);

// ACK-tracked, single-slot (Busy if one already outstanding). Retries
// reuse the same seq with kFlagRetry set. `done` fires exactly once, on
// ACK or once the attempt budget is exhausted.
SendResult sendReliable(const char* destLinkId, Type t, uint8_t session,
                        const uint8_t* payload, uint8_t len,
                        SendDoneFn done, void* ctx);

bool    txBusy();          // a frame is on air, or a reliable send is awaiting its ACK
uint8_t pendingSeq();
int     pendingAttempt();  // 0-based, for a "Retrying (2/4)" UI

// Called every tick while active, so the owning screen can drive a
// "sending" indicator without knowing anything about the radio.
void setTxIndicator(TxIndicator fn, void* ctx);

// ---- discovery -----------------------------------------------------------
void setBeaconEnabled(bool on);      // forced off automatically while a session is open
// Adaptive beacon pacing -- slow while idle, speed up while a contact
// picker is open. Not persisted; resets to idle default on every begin().
void setBeaconIntervalMs(unsigned long ms);
void setCaps(uint8_t caps);
void requestScan();                  // one BeaconReq + a temporary fast beacon interval
int         peerCount();             // also (re-)sorts by RSSI desc, then name -- call before peerAt()
const Peer& peerAt(int i);
const Peer* findPeer(const char* linkId); // nullptr if not currently known

// ---- sessions (Ship Battle uses these; Radio Chat passes session 0 to
// sendUnreliable/sendReliable directly and never opens a session) ----------
void openSession(const char* peerLinkId, uint8_t sessionId);
void closeSession();
bool sessionOpen();
const char* sessionPeer();
uint8_t sessionId();
unsigned long sessionSilentMs();     // ms since anything was heard from the session peer

// ---- identity --------------------------------------------------------
const char* myLinkId();

} // namespace radiolink

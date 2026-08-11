#include "peerdrop.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../audio/audio_manager.h"
#include "../leds/led_manager.h"
#include "../storage/storage.h"
#include "../settings/badge_id.h"
#include "../ui/widgets.h"
#include "../ui/animation.h"
#include "../ui/theme.h"
#include <NimBLEDevice.h>
#include <vector>
#include <math.h>
#include <string.h>

// LED chain has no blue group (red/green/white only); white (led::kMaskWhite)
// substitutes wherever "blue" is called for.
// downloadAndAck() blocks on NimBLEClient::connect(); the transfer visual is
// a bounded pre-roll animation via pump() shown before the real call, not a
// live progress readout.

namespace peerdrop {

static const char* SVC_UUID     = "5ee72e01-0001-4000-8000-00805f9b34fb";
static const char* IDENTITY_UUID = "5ee72e01-0002-4000-8000-00805f9b34fb";
static const char* ACK_UUID      = "5ee72e01-0003-4000-8000-00805f9b34fb";

// Player explicitly picks a role per exchange on the Confirm screen instead
// of both badges racing to auto-detect mutual selection over BLE advertising:
//   - Receive: this badge connects out, reads the peer's identity, acks.
//   - Send: this badge waits passively for the chosen peer to read+ack us.
// Two people exchange contacts by doing this twice, once from each side.
enum class Phase { Scanning, Confirm, Receiving, Sending, Success, Failure };

// Why Phase::Failure was entered, so the screen can show a specific reason.
enum class FailureReason { Timeout, PeerMovedAway, ConnectFailed, TransferInterrupted };
static FailureReason s_failureReason = FailureReason::Timeout;

// Split so Phase::Failure can distinguish connect() failing from a connected
// peer's GATT read/write failing.
enum class ExchangeResult { Ok, ConnectFailed, DataFailed };

struct Peer {
    NimBLEAddress addr;
    String username;
    int rssi;
    unsigned long lastSeenMs;
};

static std::vector<Peer> s_peers;
static int s_selected = -1;
static Phase s_phase;
static unsigned long s_phaseStartMs;
// Set by AckCallbacks::onWrite() whenever any peer acks us; Phase::Sending
// additionally checks s_peerAckedFromAddr matches the chosen peer.
static bool s_peerAckedUs = false;
static unsigned long s_peerAckedAtMs = 0;
static NimBLEAddress s_peerAckedFromAddr;
static storage::Identity s_myIdentity;
static String s_myMac;
static String s_lastFocusedMac; // which peer we last played the "found" ping/sweep for

// Exchange target, captured once a role is chosen (beginReceiving()/
// beginSending()) instead of relying on s_peers[s_selected] afterward, since
// pruneStalePeers() can shift/invalidate that index mid-exchange.
static NimBLEAddress s_targetAddr;
static String s_targetUsername;
// true = "Receive" selected, false = "Send".
static bool s_confirmReceive = true;

// Auto-connect proximity tracking (cfg::PEERDROP_AUTO_CONNECT_RSSI_DBM/HOLD_MS):
// how long the focused peer has held at/above the auto-connect RSSI.
static String s_autoConnectMac;
static unsigned long s_autoConnectSinceMs = 0;
static bool s_lastConnectWasAuto = false; // read once by Waiting's fresh-scene draw, then consumed

// "Discovery reward" pause before a confirm dialog/auto-connect can trigger,
// letting the peer-found moment register first.
static unsigned long s_peerDiscoveredAtMs = 0;
constexpr unsigned long kDiscoveryPauseMs = 700;

// Rate gate for animating phases to avoid redrawing every loop() tick
// (flicker). 30fps is plenty for these animations.
static unsigned long s_lastAnimFrameMs = 0;
constexpr unsigned long kAnimFrameIntervalMs = 33;

static bool shouldDrawAnimFrame() {
    unsigned long now = millis();
    if (now - s_lastAnimFrameMs < kAnimFrameIntervalMs) return false;
    s_lastAnimFrameMs = now;
    return true;
}

// Rate gate for scanTick(): the radio scan itself only produces new results
// ~every 100ms, so polling faster is pure overhead.
static unsigned long s_lastScanTickMs = 0;
constexpr unsigned long kScanTickIntervalMs = 50;

static bool shouldScanTick() {
    unsigned long now = millis();
    if (now - s_lastScanTickMs < kScanTickIntervalMs) return false;
    s_lastScanTickMs = now;
    return true;
}

static NimBLEServer* s_server = nullptr;
static NimBLECharacteristic* s_identityChar = nullptr;
static NimBLECharacteristic* s_ackChar = nullptr;
static NimBLEAdvertising* s_advertising = nullptr;
static NimBLEScan* s_scan = nullptr;

// ---- Protocol (unchanged) ----

static String packCsv() {
    return s_myIdentity.name + "," + s_myIdentity.email + "," +
           s_myIdentity.phone + "," + s_myIdentity.org;
}

static storage::Identity unpackCsv(const String& csv) {
    storage::Identity id;
    int p1 = csv.indexOf(',');
    int p2 = csv.indexOf(',', p1 + 1);
    int p3 = csv.indexOf(',', p2 + 1);
    id.name = csv.substring(0, p1);
    id.email = csv.substring(p1 + 1, p2);
    id.phone = csv.substring(p2 + 1, p3);
    id.org = csv.substring(p3 + 1);
    return id;
}

class AckCallbacks : public NimBLECharacteristicCallbacks {
    // esp-nimble-cpp 2.x adds a NimBLEConnInfo& param — see docs/architecture/ble-subsystem.md.
    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& connInfo) override {
        if (c->getValue() == "ACK") {
            s_peerAckedUs = true;
            s_peerAckedAtMs = millis();
            s_peerAckedFromAddr = connInfo.getAddress();
            Serial.printf("[PEERDROP] ACK characteristic write received from %s\n",
                          s_peerAckedFromAddr.toString().c_str());
        }
    }
};
static AckCallbacks s_ackCallbacks;

// Advertises plain "VAJRA-<name>" only — no manufacturer-data payload, which
// used to eat 10 of the 31-byte legacy-advertising budget.
static void startAdvertising() {
    NimBLEAdvertisementData advData;
    String uname = s_myIdentity.name.length() ? s_myIdentity.name : badge_id::getBadgeId();
    constexpr size_t kMaxUnameLen = 8;
    if (uname.length() > kMaxUnameLen) uname = uname.substring(0, kMaxUnameLen);
    advData.setName(("VAJRA-" + uname).c_str());
    s_advertising->setAdvertisementData(advData);
    s_advertising->start();
}

// Whole-session one-time init flag — see startBleStack().
static bool s_bleStackInitialized = false;

static void startBleStack() {
    s_myIdentity = storage::loadMyIdentity();

    if (!s_bleStackInitialized) {
        // One-time setup, never torn down: repeated NimBLEDevice::deinit(true)
        // + init() cycles are a known NimBLE-Arduino instability source (the
        // controller doesn't reliably come back up clean). Entering/exiting
        // PeerDrop only starts/stops scanning and advertising instead.
        NimBLEDevice::init(("VAJRA-" + s_myIdentity.name).c_str());
        s_myMac = NimBLEDevice::getAddress().toString().c_str();
        s_myMac.toUpperCase();

        s_server = NimBLEDevice::createServer();
        NimBLEService* svc = s_server->createService(SVC_UUID);
        s_identityChar = svc->createCharacteristic(IDENTITY_UUID, NIMBLE_PROPERTY::READ);
        s_ackChar = svc->createCharacteristic(ACK_UUID, NIMBLE_PROPERTY::WRITE);
        s_ackChar->setCallbacks(&s_ackCallbacks);
        // esp-nimble-cpp 2.x: NimBLEService::start() is now a no-op — services
        // auto-start with the server. See docs/architecture/ble-subsystem.md.

        s_advertising = NimBLEDevice::getAdvertising();
        // Deliberately no addServiceUUID(SVC_UUID): a 128-bit UUID costs 18
        // of the 31 advertisement bytes and isn't needed — scanTick() matches
        // by "VAJRA-" name prefix, and GATT discovery happens post-connect.
        // Including it caused "Advertisement data length exceeded" failures.
        s_scan = NimBLEDevice::getScan();
        s_scan->setActiveScan(true);
        s_scan->setInterval(100);
        s_scan->setWindow(80);
        // Duplicate filter off: peer list needs to track live RSSI/proximity.
        s_scan->setDuplicateFilter(0);

        s_bleStackInitialized = true;
    }

    // Refresh identity (may have changed via Settings > WiFi Setup) and
    // (re)start advertising/scanning every entry, not just the first.
    s_identityChar->setValue(packCsv().c_str());
    startAdvertising();

    // duration=0 = scan continuously, non-blocking; scanTick() polls
    // accumulated results rather than starting/stopping per call.
    s_scan->start(0, false);
}

// Pauses BLE activity without tearing down the whole stack.
static void stopBleActivity() {
    if (s_scan) s_scan->stop();
    if (s_advertising) s_advertising->stop();
}

void stopIfActive() {
    stopBleActivity();
}

// Restarts scanning when returning to Phase::Scanning from the Failure
// retry path: a connect() attempt cancels the in-progress scan at the
// controller level, and nothing else restarts it afterward.
static void restartScan() {
    if (!s_scan) return;
    s_scan->stop();
    s_scan->clearResults();
    s_scan->start(0, false);
}

static Peer* findPeerByMac(const String& mac) {
    for (auto& p : s_peers) {
        if (String(p.addr.toString().c_str()) == mac) return &p;
    }
    return nullptr;
}

// Reuses the contacts NVS store as source of truth (survives reboots, matches Contacts).
static bool isKnownContact(const String& mac) {
    int n = storage::contactCount();
    for (int i = 0; i < n; i++) {
        if (storage::loadContact(i).mac == mac) return true;
    }
    return false;
}

// User-facing proximity label, never a raw dBm number.
static const char* rssiLabel(int rssi) {
    if (rssi >= cfg::PEERDROP_AUTO_CONNECT_RSSI_DBM) return "Very Close";
    if (rssi >= -55) return "Excellent";
    if (rssi >= -65) return "Good";
    return "Fair";
}

static void pruneStalePeers() {
    unsigned long now = millis();
    for (size_t i = 0; i < s_peers.size();) {
        if (now - s_peers[i].lastSeenMs > 5000) s_peers.erase(s_peers.begin() + i);
        else i++;
    }
}

static void scanTick() {
    NimBLEScanResults results = s_scan->getResults();
    for (int i = 0; i < results.getCount(); i++) {
        // esp-nimble-cpp 2.x's getDevice() returns a pointer, not a copy.
        const NimBLEAdvertisedDevice* dev = results.getDevice(i);
        if (!dev->haveName()) continue;
        std::string name = dev->getName();
        if (name.rfind("VAJRA-", 0) != 0) continue;
        if (dev->getRSSI() < cfg::PEERDROP_RSSI_THRESHOLD_DBM) continue;

        String mac = dev->getAddress().toString().c_str();
        String username = name.substr(6).c_str();

        Peer* existing = findPeerByMac(mac);
        if (existing) {
            existing->username = username;
            existing->rssi = dev->getRSSI();
            existing->lastSeenMs = millis();
        } else {
            Serial.printf("[PEERDROP] -> new peer added: %s (%s) rssi=%d\n",
                          username.c_str(), mac.c_str(), dev->getRSSI());
            s_peers.push_back({ dev->getAddress(), username, dev->getRSSI(), millis() });
            if (s_selected < 0) s_selected = 0;
        }
    }
    pruneStalePeers();
    if (s_selected >= (int)s_peers.size()) s_selected = s_peers.empty() ? -1 : (int)s_peers.size() - 1;
}

static ExchangeResult downloadAndAck(const Peer& peer, bool& alreadyKnown) {
    alreadyKnown = false;
    Serial.printf("[PEERDROP] downloadAndAck: connecting to %s...\n", peer.addr.toString().c_str());
    NimBLEClient* client = NimBLEDevice::createClient();
    // connect() blocks the whole cooperative single-threaded firmware (even
    // input::update()); NimBLE's default 30s timeout could freeze the badge
    // that long, so cap it via PEERDROP_CONNECT_TIMEOUT_MS.
    client->setConnectTimeout(cfg::PEERDROP_CONNECT_TIMEOUT_MS);
    if (!client->connect(peer.addr)) {
        Serial.println("[PEERDROP] downloadAndAck: connect() FAILED");
        NimBLEDevice::deleteClient(client);
        return ExchangeResult::ConnectFailed;
    }
    Serial.println("[PEERDROP] downloadAndAck: connected, looking up service...");
    NimBLERemoteService* svc = client->getService(SVC_UUID);
    Serial.printf("[PEERDROP] downloadAndAck: service %s\n", svc ? "found" : "NOT FOUND");
    bool ok = false;
    if (svc) {
        NimBLERemoteCharacteristic* idChar = svc->getCharacteristic(IDENTITY_UUID);
        NimBLERemoteCharacteristic* ackChar = svc->getCharacteristic(ACK_UUID);
        Serial.printf("[PEERDROP] downloadAndAck: idChar=%s (canRead=%d) ackChar=%s (canWrite=%d)\n",
                      idChar ? "found" : "null", idChar && idChar->canRead(),
                      ackChar ? "found" : "null", ackChar && ackChar->canWrite());
        if (idChar && idChar->canRead()) {
            std::string csv = idChar->readValue();
            Serial.printf("[PEERDROP] downloadAndAck: read identity csv (%d bytes): %s\n",
                          (int)csv.size(), csv.c_str());
            String peerMac = peer.addr.toString().c_str();
            // Dedup only at storage time, not before connecting — the
            // handshake always runs end-to-end so the peer still gets its
            // read/ACK even if we already have their contact.
            if (!isKnownContact(peerMac)) {
                storage::Contact contact;
                contact.mac = peerMac;
                contact.identity = unpackCsv(csv.c_str());
                storage::appendContact(contact);
            } else {
                alreadyKnown = true;
                Serial.println("[PEERDROP] downloadAndAck: already have this contact, skipping duplicate save");
            }
            ok = true;
        }
        if (ackChar && ackChar->canWrite()) {
            bool wrote = ackChar->writeValue("ACK", true);
            Serial.printf("[PEERDROP] downloadAndAck: wrote ACK, success=%d\n", wrote);
        }
    }
    client->disconnect();
    NimBLEDevice::deleteClient(client);
    Serial.printf("[PEERDROP] downloadAndAck: done, ok=%d\n", ok);
    return ok ? ExchangeResult::Ok : ExchangeResult::DataFailed;
}

// ---- Drawing helpers ----

static uint16_t lerp565(uint16_t a, uint16_t b, float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    uint8_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    uint8_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    uint8_t rr = ar + (uint8_t)((br - ar) * t);
    uint8_t rg = ag + (uint8_t)((bg - ag) * t);
    uint8_t rb = ab + (uint8_t)((bb - ab) * t);
    return (rr << 11) | (rg << 5) | rb;
}

constexpr int kBodyTop = 40;
// cy=95 (not screen-center 118) keeps the Searching ripple's max radius clear
// of the "Searching for nearby badges" label below it (label is drawn once,
// not redrawn, so overlap would permanently corrupt it).
constexpr int kBadgeCy = 95;
constexpr int kSearchRippleMaxR = 45;

// Clears the whole body incl. footer. Called only on scene change, not per
// frame — everything else below is delta-only to avoid full-redraw flicker.
static void clearBody(Adafruit_ST7789& tft) {
    tft.fillRect(0, kBodyTop, cfg::DISPLAY_WIDTH, cfg::DISPLAY_HEIGHT - kBodyTop, theme::COLOR_BG);
}

static void drawBadgeIcon(Adafruit_ST7789& tft, int cx, int cy, uint16_t color) {
    tft.fillRoundRect(cx - 16, cy - 22, 32, 44, 5, color);
    tft.fillRoundRect(cx - 12, cy - 17, 24, 28, 3, theme::COLOR_BG);
    tft.fillCircle(cx, cy + 14, 3, color);
}

static void eraseBadgeIcon(Adafruit_ST7789& tft, int cx, int cy) {
    tft.fillRect(cx - 18, cy - 24, 36, 48, theme::COLOR_BG);
}

// Shared "last drawn radius" per ring — safe since only one screen using it
// is ever active at a time, reset on each fresh scene.
static int s_ringR[3] = {-1, -1, -1};

// Erase-old/draw-new delta rings. Hidden below minVisibleRadius so a
// growing ring never overlaps the solid badge icon at center.
static void drawRipplesDelta(Adafruit_ST7789& tft, int cx, int cy, unsigned long elapsedMs, int maxRadius) {
    constexpr unsigned long period = 1600;
    constexpr int rings = 3;
    constexpr int minVisibleRadius = 24;
    for (int i = 0; i < rings; i++) {
        unsigned long phase = (elapsedMs + (period / rings) * i) % period;
        float t = (float)phase / (float)period;
        int radius = (int)(t * maxRadius);
        int drawRadius = (radius >= minVisibleRadius) ? radius : -1;
        if (drawRadius == s_ringR[i]) continue;
        if (s_ringR[i] >= 0) tft.drawCircle(cx, cy, s_ringR[i], theme::COLOR_BG);
        if (drawRadius >= 0) tft.drawCircle(cx, cy, drawRadius, lerp565(theme::COLOR_ACCENT, theme::COLOR_BG, t));
        s_ringR[i] = drawRadius;
    }
}

static int s_waveR[3] = {-1, -1, -1};

// Delta radio-wave pulse dots — same erase-old/draw-new-if-changed pattern.
static void drawRadioWavesDelta(Adafruit_ST7789& tft, int x1, int x2, int y, unsigned long elapsedMs, uint16_t color) {
    for (int i = 0; i < 3; i++) {
        float frac = (i + 1) / 4.0f;
        int x = x1 + (int)((x2 - x1) * frac);
        unsigned long phase = (elapsedMs + i * 200) % 600;
        float t = (float)phase / 600.0f;
        float pulse = (sinf(t * 2.0f * PI) + 1.0f) / 2.0f;
        int r = 2 + (int)(pulse * 3);
        if (r == s_waveR[i]) continue;
        if (s_waveR[i] >= 0) tft.fillCircle(x, y, s_waveR[i], theme::COLOR_BG);
        tft.fillCircle(x, y, r, color);
        s_waveR[i] = r;
    }
}

static void resetWaveState() { s_waveR[0] = s_waveR[1] = s_waveR[2] = -1; }
static void resetRingState() { s_ringR[0] = s_ringR[1] = s_ringR[2] = -1; }

// Redraws every call (delta-tracking 3 fading trails isn't worth it for a
// <1s screen); caller clears only the thin packet lane, not the whole body.
static void drawPackets(Adafruit_ST7789& tft, int x1, int x2, int y, unsigned long elapsedMs, uint16_t color) {
    constexpr unsigned long travelMs = 550;
    constexpr int kPackets = 3;
    for (int i = 0; i < kPackets; i++) {
        unsigned long phase = (elapsedMs + i * (travelMs / kPackets)) % travelMs;
        float t = (float)phase / (float)travelMs;
        for (int trail = 0; trail < 4; trail++) {
            float tt = t - trail * 0.05f;
            if (tt < 0) continue;
            int tx = x1 + (int)((x2 - x1) * tt);
            uint16_t c = lerp565(color, theme::COLOR_BG, trail / 4.0f);
            int r = 4 - trail / 2;
            tft.fillCircle(tx, y, r, c);
        }
    }
}

static int s_autoProgressDrawnPct = -1;

// Fill bar shown while the focused peer is within auto-connect range, as
// feedback for why the badge may jump straight to Waiting with no prompt.
static void drawAutoConnectProgress(Adafruit_ST7789& tft, unsigned long holdElapsedMs, bool inRange) {
    int pct = inRange ? (int)((holdElapsedMs * 100) / cfg::PEERDROP_AUTO_CONNECT_HOLD_MS) : -1;
    if (pct > 100) pct = 100;
    if (pct == s_autoProgressDrawnPct) return;
    s_autoProgressDrawnPct = pct;
    constexpr int barW = 120, barH = 7;
    int barX = cfg::DISPLAY_WIDTH / 2 - barW / 2, barY = 204;
    tft.fillRect(barX - 2, barY - 2, barW + 4, barH + 4, theme::COLOR_BG);
    if (pct >= 0) {
        tft.drawRoundRect(barX, barY, barW, barH, 3, theme::COLOR_ACCENT_DARK);
        int fillW = (barW - 2) * pct / 100;
        if (fillW > 0) tft.fillRoundRect(barX + 1, barY + 1, fillW, barH - 2, 2, theme::COLOR_SUCCESS);
    }
}

static int s_prevDotCount = -1;

// Redraws the "..." substring only when the dot count changes (every 400ms).
static void drawDotsDelta(Adafruit_ST7789& tft, int x, int y, unsigned long elapsedMs) {
    int count = (elapsedMs / 400) % 4;
    if (count == s_prevDotCount) return;
    s_prevDotCount = count;
    tft.fillRect(x, y, 24, 10, theme::COLOR_BG);
    char buf[4] = {0};
    for (int i = 0; i < count; i++) buf[i] = '.';
    tft.setTextColor(theme::COLOR_TEXT);
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    tft.setCursor(x, y);
    tft.print(buf);
}

// Own identity (from Settings > WiFi Setup), shown at bottom of scan screen.
static void drawMyCardFooter(Adafruit_ST7789& tft) {
    storage::Identity me = storage::loadMyIdentity();
    tft.fillRect(0, cfg::DISPLAY_HEIGHT - 26, cfg::DISPLAY_WIDTH, 26, theme::COLOR_BG);
    tft.drawFastHLine(0, cfg::DISPLAY_HEIGHT - 26, cfg::DISPLAY_WIDTH, theme::COLOR_ACCENT);
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    tft.setCursor(theme::MARGIN_X, cfg::DISPLAY_HEIGHT - 14);
    if (me.name.length() == 0) {
        tft.setTextColor(theme::COLOR_TEXT);
        tft.print("No user yet - set one up in Settings > WiFi Setup");
    } else {
        tft.setTextColor(theme::COLOR_ACCENT_DARK);
        tft.print("Me: ");
        tft.setTextColor(theme::COLOR_TEXT);
        tft.print(me.name);
        if (me.org.length()) { tft.print(" ("); tft.print(me.org); tft.print(")"); }
    }
}

// ---- Screens ----
//
// Each animated screen: a "scene" identity triggers one full clearBody()+
// static draw on change, then per-element delta updates each other tick.

enum class Scene { None, Searching, PeerFound, Confirm, Waiting };
static Scene s_scene = Scene::None;
static int s_sceneKey = -2; // extra identity bit (e.g. which peer) beyond Scene alone
static bool s_footerDrawn = false;
static int s_hintReadyDrawn = -1; // -1=unset, 0="Within Range", 1="OK=Connect..." — see drawPeerFoundHintDelta()

// True exactly once per scene change; callers redraw static elements then.
static bool enterSceneIfChanged(Adafruit_ST7789& tft, Scene wanted, int key) {
    if (s_scene == wanted && s_sceneKey == key) return false;
    s_scene = wanted;
    s_sceneKey = key;
    clearBody(tft);
    resetRingState();
    resetWaveState();
    s_prevDotCount = -1;
    s_autoProgressDrawnPct = -1;
    s_hintReadyDrawn = -1;
    s_footerDrawn = false;
    return true;
}

// Swaps hint line between "Within Range" (during kDiscoveryPauseMs) and
// "OK=Connect" once elapsed; redrawn only on that one state flip.
static void drawPeerFoundHintDelta(Adafruit_ST7789& tft, bool ready) {
    int val = ready ? 1 : 0;
    if (val == s_hintReadyDrawn) return;
    s_hintReadyDrawn = val;
    tft.fillRect(0, 190, cfg::DISPLAY_WIDTH, 10, theme::COLOR_BG);
    theme::drawCentered(tft, ready ? "OK=Connect  UP/DOWN=switch" : "Within Range",
                         190, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
}

static void drawScanningFrame(Adafruit_ST7789& tft, unsigned long elapsedMs) {
    bool hasPeer = (s_selected >= 0 && !s_peers.empty());
    bool fresh = enterSceneIfChanged(tft, hasPeer ? Scene::PeerFound : Scene::Searching,
                                      hasPeer ? s_selected : -1);

    if (!hasPeer) {
        drawRipplesDelta(tft, cfg::DISPLAY_WIDTH / 2, kBadgeCy, elapsedMs, kSearchRippleMaxR);
        if (fresh) {
            drawBadgeIcon(tft, cfg::DISPLAY_WIDTH / 2, kBadgeCy, theme::COLOR_ACCENT_DARK);
            theme::drawCentered(tft, "Searching for nearby badges", 172, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
        }
        drawDotsDelta(tft, cfg::DISPLAY_WIDTH / 2 + 92, 172, elapsedMs);
    } else {
        int x1 = 90, x2 = 230;
        drawRadioWavesDelta(tft, x1, x2, kBadgeCy, elapsedMs, theme::COLOR_ACCENT);
        const Peer& sel = s_peers[s_selected];
        if (fresh) {
            drawBadgeIcon(tft, x1, kBadgeCy, theme::COLOR_ACCENT_DARK);
            drawBadgeIcon(tft, x2, kBadgeCy, theme::COLOR_ACCENT);
            theme::drawCentered(tft, sel.username.c_str(), 156, theme::HEADER_TEXT_SIZE, theme::COLOR_TEXT);
            char sigBuf[24];
            snprintf(sigBuf, sizeof(sigBuf), "Signal: %s", rssiLabel(sel.rssi));
            theme::drawCentered(tft, sigBuf, 176, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT);
        }
        bool ready = (millis() - s_peerDiscoveredAtMs) >= kDiscoveryPauseMs;
        drawPeerFoundHintDelta(tft, ready);

        bool inAutoRange = sel.rssi >= cfg::PEERDROP_AUTO_CONNECT_RSSI_DBM;
        String selMac = sel.addr.toString().c_str();
        unsigned long holdElapsed = (inAutoRange && s_autoConnectMac == selMac) ? millis() - s_autoConnectSinceMs : 0;
        drawAutoConnectProgress(tft, holdElapsed, inAutoRange);
    }

    if (!s_footerDrawn) {
        drawMyCardFooter(tft); // draw once per scene, not every tick
        s_footerDrawn = true;
    }
}

static bool s_confirmReceiveDrawn = true; // last value the RECEIVE/SEND buttons were drawn for

// Pill buttons with a thin top "sheen" highlight on the selected one.
static void drawPillButton(Adafruit_ST7789& tft, int x, int y, int w, int h, const char* label,
                            bool selected, uint16_t selectedColor) {
    tft.fillRoundRect(x, y, w, h, h / 2, selected ? selectedColor : theme::COLOR_BG);
    tft.drawRoundRect(x, y, w, h, h / 2, theme::COLOR_ACCENT_DARK);
    if (selected) {
        uint16_t sheen = lerp565(selectedColor, theme::COLOR_BG, 0.35f);
        tft.drawFastHLine(x + h / 2, y + 2, w - h, sheen);
    }
    tft.setTextColor(selected ? theme::COLOR_BG : theme::COLOR_TEXT);
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    int textW = strlen(label) * 6 * theme::BODY_TEXT_SIZE;
    tft.setCursor(x + (w - textW) / 2, y + h / 2 - 3);
    tft.print(label);
}

static void drawConfirmButtons(Adafruit_ST7789& tft) {
    constexpr int w = 64, h = 22; // wide enough for "RECEIVE" at BODY_TEXT_SIZE
    int receiveX = cfg::DISPLAY_WIDTH / 2 - w - 3, sendX = cfg::DISPLAY_WIDTH / 2 + 3;
    int y = 199;
    drawPillButton(tft, receiveX, y, w, h, "RECEIVE", s_confirmReceive, theme::COLOR_SUCCESS);
    drawPillButton(tft, sendX, y, w, h, "SEND", !s_confirmReceive, theme::COLOR_ACCENT);
}

static void drawConfirmFrame(Adafruit_ST7789& tft, unsigned long elapsedMs) {
    bool fresh = enterSceneIfChanged(tft, Scene::Confirm, s_selected);
    int x1 = 90, x2 = 230;
    drawRadioWavesDelta(tft, x1, x2, kBadgeCy, elapsedMs, theme::COLOR_ACCENT);

    if (fresh) {
        drawBadgeIcon(tft, x1, kBadgeCy, theme::COLOR_ACCENT_DARK);
        drawBadgeIcon(tft, x2, kBadgeCy, theme::COLOR_ACCENT);
        theme::drawCentered(tft, s_peers[s_selected].username.c_str(), 156, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
        theme::drawCentered(tft, "Receive or Send?", 174, theme::HEADER_TEXT_SIZE, theme::COLOR_TEXT);
        drawConfirmButtons(tft);
        s_confirmReceiveDrawn = s_confirmReceive;
    } else if (s_confirmReceive != s_confirmReceiveDrawn) {
        s_confirmReceiveDrawn = s_confirmReceive;
        drawConfirmButtons(tft);
    }
}

static int s_badgeGapX = 140;   // animated by anim::startSlide during Waiting
static int s_badgeGapXDrawn = -1; // gap the badges were last actually drawn at

// anim::startSlide only tweens ui::Rect; reused here with just `w` carrying
// the badge gap in pixels (x/y/h unused) since there's no scalar tween primitive.
static void onWaitingGapStep(ui::Rect r, void* /*ctx*/) {
    s_badgeGapX = r.w;
}

// Label for the shared Waiting-style scene, set by whichever of
// beginReceiving()/beginSending() enters it.
static const char* s_waitingLabel = "";

// isAuto only affects the label shown.
static void beginReceiving(const Peer& peer, bool isAuto) {
    s_targetAddr = peer.addr;
    s_targetUsername = peer.username;
    s_lastConnectWasAuto = isAuto;
    s_waitingLabel = isAuto ? "Close range - sharing directly" : "Establishing secure connection";
    s_phase = Phase::Receiving;
    s_phaseStartMs = millis();
    s_badgeGapX = 140;
    anim::startSlide(ui::Rect{0, 0, 140, 0}, ui::Rect{0, 0, 100, 0}, 400, onWaitingGapStep, nullptr);
    led::playEffect(led::EffectId::ConnectionPulse, led::EffectParams{0, 0, 900, led::kMaskAll});
}

// s_peerAckedUs reset here so a stale ack from a previous exchange can't
// falsely complete a new one.
static void beginSending(const Peer& peer) {
    s_targetAddr = peer.addr;
    s_targetUsername = peer.username;
    s_waitingLabel = "Waiting for them to receive";
    s_peerAckedUs = false;
    s_phase = Phase::Sending;
    s_phaseStartMs = millis();
    s_badgeGapX = 140;
    anim::startSlide(ui::Rect{0, 0, 140, 0}, ui::Rect{0, 0, 100, 0}, 400, onWaitingGapStep, nullptr);
    led::playEffect(led::EffectId::ConnectionPulse, led::EffectParams{0, 0, 900, led::kMaskAll});
}

static void drawWaitingFrame(Adafruit_ST7789& tft, unsigned long elapsedMs) {
    bool fresh = enterSceneIfChanged(tft, Scene::Waiting, -1);
    if (fresh) {
        s_badgeGapXDrawn = -1;
        theme::drawCentered(tft, s_waitingLabel, 168, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
    }

    int cx = cfg::DISPLAY_WIDTH / 2;
    int x1 = cx - s_badgeGapX / 2, x2 = cx + s_badgeGapX / 2;
    drawRadioWavesDelta(tft, x1, x2, kBadgeCy, elapsedMs, theme::COLOR_SUCCESS);

    if (s_badgeGapX != s_badgeGapXDrawn) {
        if (s_badgeGapXDrawn >= 0) {
            int ox1 = cx - s_badgeGapXDrawn / 2, ox2 = cx + s_badgeGapXDrawn / 2;
            eraseBadgeIcon(tft, ox1, kBadgeCy);
            eraseBadgeIcon(tft, ox2, kBadgeCy);
        }
        drawBadgeIcon(tft, x1, kBadgeCy, theme::COLOR_ACCENT_DARK);
        drawBadgeIcon(tft, x2, kBadgeCy, theme::COLOR_ACCENT);
        s_badgeGapXDrawn = s_badgeGapX;
    }

    drawDotsDelta(tft, cx + 100, 168, elapsedMs);

    static unsigned long s_prevRemainingSec = 0xFFFFFFFF;
    if (fresh) s_prevRemainingSec = 0xFFFFFFFF;
    unsigned long remaining = cfg::PEERDROP_CONSENT_TIMEOUT_MS - (millis() - s_phaseStartMs);
    unsigned long remainingSec = remaining / 1000;
    if (remainingSec != s_prevRemainingSec) {
        s_prevRemainingSec = remainingSec;
        tft.fillRect(0, 192 - 2, cfg::DISPLAY_WIDTH, 12, theme::COLOR_BG); // text width varies 1 vs 2 digits
        char buf[32]; // sized for %lu's worst case rather than suppressing the truncation check
        snprintf(buf, sizeof(buf), "%lus (BACK to cancel)", remainingSec);
        theme::drawCentered(tft, buf, 192, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
    }
}

// Packets redraw every tick (brief ~650ms screen, not worth delta-tracking);
// static chrome drawn once, only the packet lane cleared each tick.
static void drawTransferFrame(unsigned long elapsedMs) {
    auto& tft = display::tft();
    int x1 = 90, x2 = 230;
    if (elapsedMs < 20) {
        clearBody(tft);
        drawBadgeIcon(tft, x1, kBadgeCy, theme::COLOR_ACCENT_DARK);
        drawBadgeIcon(tft, x2, kBadgeCy, theme::COLOR_ACCENT);
        tft.drawFastHLine(x1 + 20, kBadgeCy, x2 - x1 - 40, theme::COLOR_ACCENT);
        theme::drawCentered(tft, "Transferring contact", 172, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
    } else {
        tft.fillRect(x1 + 18, kBadgeCy - 6, (x2 - 20) - (x1 + 18), 12, theme::COLOR_BG);
        tft.drawFastHLine(x1 + 20, kBadgeCy, x2 - x1 - 40, theme::COLOR_ACCENT);
    }
    drawPackets(tft, x1 + 20, x2 - 20, kBadgeCy, elapsedMs, theme::COLOR_SUCCESS);
}

// Own layout (not kBadgeCy/kSearchRippleMaxR): ripple center 181/maxR 32 keeps
// the ring clear of the name line above (y=130) and the footer boundary (214).
constexpr int kSuccessCircleCy = 76;
constexpr int kSuccessCircleMaxR = 20;
constexpr int kSuccessTitleY = 108;
constexpr int kSuccessNameY = 130;
constexpr int kSuccessRippleCy = 181;
constexpr int kSuccessRippleMaxR = 32;

// Staged reveal: checkmark circle grows, then title, then peer name. Set by
// the caller right before pump()'ing this frame.
static String s_successUsername;
// True when the peer was already a saved contact (downloadAndAck() skipped
// re-saving); title reads "Already Connected" instead of "Contact Added".
// Only meaningful when s_successMode == Received.
static bool s_successAlreadyKnown = false;
// Which of Phase::Receiving/Sending completed — see drawSuccessFrame()'s title logic.
enum class SuccessMode { Received, Shared };
static SuccessMode s_successMode = SuccessMode::Received;
static int s_successCircleRDrawn = -1;
static bool s_successTitleShown = false;
static bool s_successNameShown = false;

static void drawSuccessFrame(unsigned long elapsedMs) {
    auto& tft = display::tft();
    if (elapsedMs < 20) {
        clearBody(tft);
        resetRingState();
        s_successCircleRDrawn = -1;
        s_successTitleShown = false;
        s_successNameShown = false;
    }

    constexpr unsigned long kCircleGrowMs = 220;
    int cx = cfg::DISPLAY_WIDTH / 2;
    int r = (int)((elapsedMs < kCircleGrowMs ? elapsedMs : kCircleGrowMs) * kSuccessCircleMaxR / kCircleGrowMs);
    if (r != s_successCircleRDrawn) {
        s_successCircleRDrawn = r;
        tft.fillCircle(cx, kSuccessCircleCy, kSuccessCircleMaxR + 2, theme::COLOR_BG); // clear previous ring size
        tft.fillCircle(cx, kSuccessCircleCy, r, theme::COLOR_SUCCESS);
        if (r >= kSuccessCircleMaxR) {
            // 0xFB is CP437's square-root glyph, used as a checkmark
            // stand-in (no true check in this font); drawn in bg color as a cut-out.
            tft.setTextColor(theme::COLOR_BG);
            tft.setTextSize(theme::HEADER_TEXT_SIZE);
            // Glyph is 6x8px at size 1; center exactly on cx/cy.
            tft.setCursor(cx - 3 * theme::HEADER_TEXT_SIZE, kSuccessCircleCy - 4 * theme::HEADER_TEXT_SIZE);
            tft.print("\xFB");
        }
    }

    if (!s_successTitleShown && elapsedMs >= kCircleGrowMs) {
        s_successTitleShown = true;
        const char* title = s_successMode == SuccessMode::Shared
                                ? "Contact Shared"
                                : (s_successAlreadyKnown ? "Already Connected" : "Contact Added");
        theme::drawCentered(tft, title, kSuccessTitleY, theme::HEADER_TEXT_SIZE, theme::COLOR_TEXT);
    }
    if (!s_successNameShown && elapsedMs >= kCircleGrowMs + 180) {
        s_successNameShown = true;
        theme::drawCentered(tft, s_successUsername.c_str(), kSuccessNameY, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
    }

    drawRipplesDelta(tft, cx, kSuccessRippleCy, elapsedMs, kSuccessRippleMaxR);
}

static int s_failDriftDrawn = -1;

// Specific title per FailureReason; drift animation/retry hint same for all.
static const char* failureTitle(FailureReason reason) {
    switch (reason) {
        case FailureReason::PeerMovedAway:      return "Badge Moved Away";
        case FailureReason::ConnectFailed:      return "Connection Failed";
        case FailureReason::TransferInterrupted: return "Transfer Interrupted";
        case FailureReason::Timeout:
        default:                                 return "Connection Timed Out";
    }
}

static void drawFailureFrame(unsigned long elapsedMs) {
    auto& tft = display::tft();
    int drift = (int)(elapsedMs / 8);
    if (drift > 40) drift = 40;

    if (s_failDriftDrawn < 0) {
        clearBody(tft);
        theme::drawCentered(tft, failureTitle(s_failureReason), 168, theme::HEADER_TEXT_SIZE, theme::COLOR_DANGER);
        theme::drawCentered(tft, "OK to retry, BACK to exit", 196, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
    }

    if (drift != s_failDriftDrawn) {
        if (s_failDriftDrawn >= 0) {
            eraseBadgeIcon(tft, 90 - s_failDriftDrawn, kBadgeCy);
            eraseBadgeIcon(tft, 230 + s_failDriftDrawn, kBadgeCy);
        }
        drawBadgeIcon(tft, 90 - drift, kBadgeCy, theme::COLOR_ACCENT_DARK);
        drawBadgeIcon(tft, 230 + drift, kBadgeCy, theme::COLOR_DANGER);
        s_failDriftDrawn = drift;
    }
}

// Bounded, self-pumping animation loop for moments that must stay on screen
// for a minimum duration around a blocking call or final result.
static void pump(unsigned long durationMs, void (*drawFrame)(unsigned long elapsedMs)) {
    unsigned long start = millis();
    while (millis() - start < durationMs) {
        unsigned long elapsed = millis() - start;
        anim::update();
        audio::update();
        led::update();
        drawFrame(elapsed);
        delay(16);
    }
}

// ---- Audio ----
static const audio::Note kSfxPing[]        = { { 1568, 60 } };
static const audio::Note kChimeConnect[]   = { { 1046, 80 }, { 1318, 80 }, { 1568, 120 } }; // ascending
static const audio::Note kTickTransfer[]   = { { 2093, 15 } };
static const audio::Note kMelodySuccess[]  = { { 1318, 100 }, { 1568, 100 }, { 2093, 200 } };
static const audio::Note kToneFailure[]    = { { 400, 80 }, { 250, 160 } }; // descending

void enter() {
    s_peers.clear();
    s_selected = -1;
    s_phase = Phase::Scanning;
    s_phaseStartMs = millis();
    s_peerAckedUs = false;
    s_lastFocusedMac = "";
    s_confirmReceive = true;
    s_autoConnectMac = "";
    s_autoConnectSinceMs = 0;
    s_lastConnectWasAuto = false;
    s_peerDiscoveredAtMs = 0;
    s_lastAnimFrameMs = 0;
    // Reset explicitly: s_scene persists across sessions, so a matching
    // scene on re-entry would skip redrawing badge/labels after clearBody().
    s_scene = Scene::None;
    startBleStack();
    led::playEffect(led::EffectId::Breathing, led::EffectParams{0, 0, 0, led::kMaskWhite}); // "blue" substitute

    // Not Renderer-based, so chrome must be drawn on entry manually.
    auto& tft = display::tft();
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)kBodyTop}, "PeerDrop");
    clearBody(tft);
}

AppState frame() {
    auto& tft = display::tft();
    unsigned long elapsed = millis() - s_phaseStartMs;

    // BACK exits from any phase except Receiving: interrupting a live
    // connect()/read/write mid-flight risks a dangling client object.
    if (input::wasPressed(input::Button::Back) && s_phase != Phase::Receiving) {
        stopBleActivity();
        led::stop();
        return AppState::MainMenu;
    }

    switch (s_phase) {
        case Phase::Scanning: {
            if (shouldScanTick()) scanTick();

            // One-shot ping + LED sweep when a peer is newly focused; also
            // starts the discovery-reward pause gating confirm/auto-connect below.
            String focusedMac = (s_selected >= 0) ? String(s_peers[s_selected].addr.toString().c_str()) : String("");
            if (focusedMac.length() && focusedMac != s_lastFocusedMac) {
                s_lastFocusedMac = focusedMac;
                s_peerDiscoveredAtMs = millis();
                audio::playSfx(kSfxPing, 1);
                led::playEffect(led::EffectId::Sweep, led::EffectParams{0, 0, 60, led::kMaskWhite});
            } else if (!focusedMac.length() && s_lastFocusedMac.length()) {
                s_lastFocusedMac = "";
                led::playEffect(led::EffectId::Breathing, led::EffectParams{0, 0, 0, led::kMaskWhite});
            }

            if (input::wasPressed(input::Button::JoyUp) && !s_peers.empty()) {
                s_selected = (s_selected - 1 + (int)s_peers.size()) % (int)s_peers.size();
            }
            if (input::wasPressed(input::Button::JoyDown) && !s_peers.empty()) {
                s_selected = (s_selected + 1) % (int)s_peers.size();
            }

            bool ready = (millis() - s_peerDiscoveredAtMs) >= kDiscoveryPauseMs;

            // Near-contact auto-share: badges held at/above the auto-connect
            // RSSI for the hold time skip manual Confirm entirely. Already
            // being a known contact does not skip the connection — still
            // shows "Already Connected" instead of "Contact Added".
            bool autoFired = false;
            if (s_selected >= 0 && s_selected < (int)s_peers.size()) {
                const Peer& focused = s_peers[s_selected];
                if (focused.rssi >= cfg::PEERDROP_AUTO_CONNECT_RSSI_DBM) {
                    if (focusedMac != s_autoConnectMac) {
                        s_autoConnectMac = focusedMac;
                        s_autoConnectSinceMs = millis();
                    }
                    // Physically touching badges skip the hold timer (still gated on `ready`).
                    bool touching = focused.rssi >= cfg::PEERDROP_TOUCH_RSSI_DBM;
                    unsigned long requiredHold = touching ? 0 : cfg::PEERDROP_AUTO_CONNECT_HOLD_MS;
                    if (ready && millis() - s_autoConnectSinceMs >= requiredHold) {
                        s_autoConnectMac = "";
                        autoFired = true;
                        // Full handshake always runs even if already a known
                        // contact — dedup happens at storage time only.
                        // Auto-connect always means Receive.
                        audio::playMelody(kChimeConnect, 3, false);
                        beginReceiving(focused, /*isAuto=*/true);
                    }
                } else {
                    s_autoConnectMac = "";
                }
            } else {
                s_autoConnectMac = "";
            }

            bool confirm = !autoFired && ready && (input::wasPressed(input::Button::Ok) ||
                           input::wasPressed(input::Button::JoySelect));
            if (confirm && s_selected >= 0 && s_selected < (int)s_peers.size()) {
                s_phase = Phase::Confirm;
                s_phaseStartMs = millis();
                s_confirmReceive = true;
                audio::playMelody(kChimeConnect, 3, false);
            }
            if (!autoFired && shouldDrawAnimFrame()) drawScanningFrame(tft, elapsed); // footer is drawn inside, once per scene
            break;
        }
        case Phase::Confirm: {
            if (input::wasPressed(input::Button::JoyLeft) || input::wasPressed(input::Button::JoyRight)) {
                s_confirmReceive = !s_confirmReceive;
            }
            bool confirm = input::wasPressed(input::Button::Ok) ||
                           input::wasPressed(input::Button::JoySelect);
            if (confirm) {
                if (s_confirmReceive) {
                    beginReceiving(s_peers[s_selected], /*isAuto=*/false);
                } else {
                    beginSending(s_peers[s_selected]);
                }
            }
            if (shouldDrawAnimFrame()) drawConfirmFrame(tft, elapsed);
            break;
        }
        case Phase::Receiving: {
            // Connects using the address captured at beginReceiving() time,
            // not a fresh scan lookup (scan may be paused while connecting;
            // single-radio BLE limitation).
            led::playEffect(led::EffectId::PacketFlow, led::EffectParams{0, 0, 70, led::kMaskAll});
            pump(650, [](unsigned long e) {
                if ((e / 90) % 2 == 0) audio::playSfx(kTickTransfer, 1);
                drawTransferFrame(e);
            });
            Peer target{};
            target.addr = s_targetAddr;
            target.username = s_targetUsername;
            bool alreadyKnown = false;
            ExchangeResult result = downloadAndAck(target, alreadyKnown);
            if (result == ExchangeResult::Ok) {
                s_successUsername = target.username;
                s_successAlreadyKnown = alreadyKnown;
                s_successMode = SuccessMode::Received;
                s_phase = Phase::Success;
                s_phaseStartMs = millis();
                led::playEffect(led::EffectId::SuccessFlash);
                audio::playMelody(kMelodySuccess, 3, false);
                pump(1100, drawSuccessFrame);
            } else {
                s_failureReason = (result == ExchangeResult::ConnectFailed)
                                       ? FailureReason::ConnectFailed
                                       : FailureReason::TransferInterrupted;
                s_phase = Phase::Failure;
                s_phaseStartMs = millis();
                s_failDriftDrawn = -1; // force the Failure screen to redraw its static text fresh
                led::playEffect(led::EffectId::FailurePulse);
                audio::playMelody(kToneFailure, 2, false);
            }
            break;
        }
        case Phase::Sending: {
            // Watches for the specific chosen peer to ack; an ack from an
            // unrelated third badge does not complete this wait.
            if (s_peerAckedUs && s_peerAckedFromAddr == s_targetAddr) {
                Serial.println("[PEERDROP] Sending: chosen peer read us and acked");
                s_successUsername = s_targetUsername;
                s_successMode = SuccessMode::Shared;
                s_phase = Phase::Success;
                s_phaseStartMs = millis();
                led::playEffect(led::EffectId::SuccessFlash);
                audio::playMelody(kMelodySuccess, 3, false);
                pump(1100, drawSuccessFrame);
                break;
            }
            if (millis() - s_phaseStartMs >= cfg::PEERDROP_CONSENT_TIMEOUT_MS) {
                Serial.println("[PEERDROP] Sending: timed out waiting for the peer to receive");
                s_failureReason = FailureReason::Timeout;
                s_phase = Phase::Failure;
                s_phaseStartMs = millis();
                s_failDriftDrawn = -1;
                led::playEffect(led::EffectId::FailurePulse);
                audio::playMelody(kToneFailure, 2, false);
                break;
            }
            if (shouldDrawAnimFrame()) drawWaitingFrame(tft, elapsed);
            break;
        }
        case Phase::Success: {
            // Result already shown by the pump() call above; auto-return.
            s_peers.clear();
            s_selected = -1;
            s_lastFocusedMac = "";
            stopBleActivity();
            led::stop();
            return AppState::Contacts;
        }
        case Phase::Failure: {
            drawFailureFrame(elapsed);
            bool retry = input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::JoySelect);
            if (retry) {
                restartScan(); // fixes the "stuck searching until full reset" bug — see restartScan()
                s_peers.clear();
                s_selected = -1;
                s_lastFocusedMac = "";
                s_phase = Phase::Scanning;
                s_phaseStartMs = millis();
                led::playEffect(led::EffectId::Breathing, led::EffectParams{0, 0, 0, led::kMaskWhite});
            }
            break;
        }
    }

    return AppState::Peerdrop;
}

} // namespace peerdrop

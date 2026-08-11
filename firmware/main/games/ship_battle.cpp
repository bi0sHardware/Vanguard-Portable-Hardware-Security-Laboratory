#include "ship_battle.h"
#include "ship_battle_net.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../leds/led_manager.h"
#include "../audio/audio_manager.h"
#include "../storage/storage.h"
#include "../ui/widgets.h"
#include "../ui/theme.h"
#include <Arduino.h>

// Presentation layer only; protocol/turn logic lives in ship_battle_net
// (namespace shipnet). Hand-rolled Screen enum + switch dispatch with
// delta-tracked drawing (like radio_chat.cpp), not the Region-based
// renderer, since state arrives asynchronously between draws here.
namespace games::ship_battle {

enum class Screen {
    Startup, Menu, HostWaiting, JoinList, Joining, JoinRejected,
    Deploy, Battle, GameOver, RadioOffline
};

static Screen s_screen = Screen::Startup;
static unsigned long s_screenAtMs = 0;
static bool s_dirty = true;
static bool s_radioOffline = false;

static void setScreen(Screen s) {
    s_screen = s;
    s_screenAtMs = millis();
    s_dirty = true;
}

// Declared early since menuFrame()/deployFrame() (defined before the Join/
// Battle sections) call these; codebase convention is strict top-to-bottom
// order with no header forward declarations.
static int s_joinSelected = 0;

static void enterJoinList() {
    s_joinSelected = 0;
    setScreen(Screen::JoinList);
}

static uint8_t s_fireRow = 0, s_fireCol = 0;
static bool s_battleChromeDrawn = false;
static shipnet::Phase s_battleLastPhase = shipnet::Phase::Idle;
static int s_battleLastMyRemaining = -1, s_battleLastTheirRemaining = -1;

static void enterBattle() {
    s_fireRow = 0;
    s_fireCol = 0;
    s_battleChromeDrawn = false;
    s_battleLastPhase = shipnet::Phase::Idle;
    setScreen(Screen::Battle);
}

// Ship Battle is a GAME, exempt from Radio Chat's white-only LED rule:
// red=hit, white=fire, multi-flash=victory.
static const audio::Note kSfxFire[]  = { { 1400, 30 } };
static const audio::Note kSfxMiss[]  = { { 400, 60 } };
static const audio::Note kSfxHit[]   = { { 1000, 50 }, { 1300, 80 } };
static const audio::Note kSfxSunk[]  = { { 800, 60 }, { 600, 60 }, { 400, 100 } };
static const audio::Note kSfxWin[]   = { { 1046, 100 }, { 1318, 100 }, { 1568, 100 }, { 2093, 220 } };
static const audio::Note kSfxLose[]  = { { 600, 120 }, { 400, 120 }, { 250, 220 } };

// ---------------------------------------------------------------------
// Startup (brief, matches Radio Chat's own ~1s ceiling)
// ---------------------------------------------------------------------
constexpr unsigned long kStartupMs = 700;

static void drawStartup() {
    auto& tft = display::tft();
    tft.fillScreen(ST77XX_BLACK);
    theme::drawCentered(tft, "SHIP BATTLE", 90, theme::HEADER_TEXT_SIZE, ST77XX_WHITE);
    theme::drawCentered(tft, "LoRa multiplayer", 120, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT);
}

// ---------------------------------------------------------------------
// Menu (Host / Join / Back)
// ---------------------------------------------------------------------
static const char* kMenuItems[] = { "Host Game", "Join Game", "Back" };
constexpr int kMenuCount = 3;
static int s_menuSelected = 0;
static bool s_menuChromeDrawn = false;
static int s_menuLastSelected = -1;
constexpr int16_t kMenuListTop = (int16_t)(theme::LIST_START_Y + 20);

static void drawMenuRow(Adafruit_ST7789& tft, int i, int16_t y) {
    ui::Rect row{0, y, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_ITEM_HEIGHT};
    tft.fillRect(row.x, row.y, row.w, row.h, theme::COLOR_BG);
    ui::widgets::listItem(tft, row, i, kMenuItems[i], i == s_menuSelected);
}

static void drawMenu() {
    auto& tft = display::tft();
    bool fresh = !s_menuChromeDrawn;
    if (fresh) {
        ui::widgets::clearScreen(tft);
        ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y},
                            "Ship Battle");
        theme::drawCentered(tft, s_radioOffline ? "Radio Offline" : "Channel 07", theme::LIST_START_Y + 6,
                            theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
        s_menuChromeDrawn = true;
        s_menuLastSelected = -1;
    }
    for (int i = 0; i < kMenuCount; i++) {
        if (fresh || i == s_menuSelected || i == s_menuLastSelected) {
            drawMenuRow(tft, i, (int16_t)(kMenuListTop + i * theme::LIST_ITEM_HEIGHT));
        }
    }
    s_menuLastSelected = s_menuSelected;
}

static void enterMenu() {
    s_menuSelected = 0;
    s_menuChromeDrawn = false;
    setScreen(Screen::Menu);
}

static void menuFrame(AppState* exitTo) {
    *exitTo = AppState::ShipBattle;
    if (input::wasPressed(input::Button::JoyUp)) { s_menuSelected = (s_menuSelected - 1 + kMenuCount) % kMenuCount; ui::widgets::navSfx(); s_dirty = true; }
    if (input::wasPressed(input::Button::JoyDown)) { s_menuSelected = (s_menuSelected + 1) % kMenuCount; ui::widgets::navSfx(); s_dirty = true; }
    bool confirm = input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::JoySelect);
    if (confirm) {
        if (s_menuSelected == 0) {
            String name = storage::loadMyIdentity().name;
            shipnet::startHosting(name.length() ? name.c_str() : "Badge");
            setScreen(Screen::HostWaiting);
        } else if (s_menuSelected == 1) {
            enterJoinList();
        } else {
            *exitTo = AppState::GamesMenu;
        }
    }
}

// ---------------------------------------------------------------------
// Host: waiting for a joiner
// ---------------------------------------------------------------------
static void drawHostWaiting() {
    auto& tft = display::tft();
    ui::widgets::clearScreen(tft);
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y}, "Hosting");
    theme::drawCentered(tft, "Waiting for opponent...", 100, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
    theme::drawCentered(tft, "Have them open Join Game", 130, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
    theme::drawCentered(tft, "BACK to cancel", cfg::DISPLAY_HEIGHT - 16, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
}

// ---------------------------------------------------------------------
// Join: scanning host list
// ---------------------------------------------------------------------

static void drawJoinList() {
    auto& tft = display::tft();
    ui::widgets::clearScreen(tft);
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y}, "Join Game");
    int count = shipnet::hostCount();
    if (count == 0) {
        theme::drawCentered(tft, "Searching for hosts...", 110, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
    } else {
        if (s_joinSelected >= count) s_joinSelected = count - 1;
        int16_t y = theme::LIST_START_Y + 4;
        for (int i = 0; i < count && i < 6; i++) {
            const shipnet::HostInfo& h = shipnet::hostAt(i);
            char label[24];
            snprintf(label, sizeof(label), "%s", h.name[0] ? h.name : h.linkId);
            char value[12];
            snprintf(value, sizeof(value), "%ddBm", h.rssiDbm);
            ui::Rect row{0, y, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_ITEM_HEIGHT};
            ui::widgets::listItem(tft, row, i, label, i == s_joinSelected, value);
            y += theme::LIST_ITEM_HEIGHT;
        }
    }
    theme::drawCentered(tft, "OK=join  BACK=cancel", cfg::DISPLAY_HEIGHT - 16, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
}

static void joinListFrame() {
    int count = shipnet::hostCount();
    // Redraw on a throttled cadence as hosts appear/disappear (HostAdv beacons).
    static unsigned long s_lastUpdateMs = 0;
    if (millis() - s_lastUpdateMs >= 500) { s_lastUpdateMs = millis(); s_dirty = true; }
    if (count > 0) {
        if (input::wasPressed(input::Button::JoyUp)) { s_joinSelected = (s_joinSelected - 1 + count) % count; ui::widgets::navSfx(); s_dirty = true; }
        if (input::wasPressed(input::Button::JoyDown)) { s_joinSelected = (s_joinSelected + 1) % count; ui::widgets::navSfx(); s_dirty = true; }
    }
    bool confirm = input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::JoySelect);
    if (confirm && count > 0) {
        const shipnet::HostInfo& h = shipnet::hostAt(s_joinSelected);
        String name = storage::loadMyIdentity().name;
        shipnet::joinHost(h.linkId, h.session, name.length() ? name.c_str() : "Badge");
        setScreen(Screen::Joining);
    }
}

static void drawJoining() {
    auto& tft = display::tft();
    ui::widgets::clearScreen(tft);
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y}, "Join Game");
    theme::drawCentered(tft, "Connecting...", 110, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
    theme::drawCentered(tft, "BACK to cancel", cfg::DISPLAY_HEIGHT - 16, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
}

static void drawJoinRejected() {
    auto& tft = display::tft();
    ui::widgets::clearScreen(tft);
    ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y}, "Join Game");
    theme::drawCentered(tft, "Host is busy", 110, theme::HEADER_TEXT_SIZE, theme::COLOR_DANGER);
    theme::drawCentered(tft, "OK to go back", 150, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
}

// ---------------------------------------------------------------------
// Deploy -- local ship placement, no radio traffic until confirmed
// ---------------------------------------------------------------------
static const char* kShipNames[shipnet::kShipCount] = { "Carrier", "Battleship", "Cruiser", "Submarine", "Destroyer" };
static int s_deployShip = 0;       // which ship is currently being placed
static uint8_t s_deployRow = 0, s_deployCol = 0;
static bool s_deployHorizontal = true;
static bool s_deployChromeDrawn = false;

constexpr int16_t kDeployGridX = 26, kDeployGridY = 58; // shifted from (20,50) to leave room for coordinate labels
constexpr int16_t kDeployCellSize = 18;

// Coordinate labels (A-H, 1-8), drawn once as static chrome, not per-tick.
static void drawGridLabels(Adafruit_ST7789& tft, int16_t gridX, int16_t gridY, int16_t cellSize) {
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    tft.setTextColor(theme::COLOR_ACCENT_DARK);
    for (int c = 0; c < shipnet::kBoardSize; c++) {
        char label[2] = { (char)('A' + c), 0 };
        tft.setCursor((int16_t)(gridX + c * cellSize + cellSize / 2 - 3), (int16_t)(gridY - 12));
        tft.print(label);
    }
    for (int r = 0; r < shipnet::kBoardSize; r++) {
        char label[2] = { (char)('1' + r), 0 };
        tft.setCursor((int16_t)(gridX - 12), (int16_t)(gridY + r * cellSize + cellSize / 2 - 4));
        tft.print(label);
    }
}

static void enterDeploy() {
    shipnet::clearFleet();
    s_deployShip = 0;
    s_deployRow = 0;
    s_deployCol = 0;
    s_deployHorizontal = true;
    s_deployChromeDrawn = false;
    setScreen(Screen::Deploy);
}

// Advances to the next not-yet-placed ship; fleetComplete() gates confirm.
static void advanceToNextUnplacedShip() {
    for (int i = 0; i < shipnet::kShipCount; i++) {
        if (!shipnet::shipPlaced(i)) { s_deployShip = i; return; }
    }
}

static uint16_t deployCellColor(int row, int col) {
    shipnet::Cell c = shipnet::myCell((uint8_t)row, (uint8_t)col);
    if (c == shipnet::Cell::Ship) return theme::COLOR_ACCENT;
    return theme::COLOR_BG;
}

static void drawDeployGrid(Adafruit_ST7789& tft) {
    for (int r = 0; r < shipnet::kBoardSize; r++) {
        for (int c = 0; c < shipnet::kBoardSize; c++) {
            int16_t x = (int16_t)(kDeployGridX + c * kDeployCellSize);
            int16_t y = (int16_t)(kDeployGridY + r * kDeployCellSize);
            tft.fillRect(x + 1, y + 1, kDeployCellSize - 2, kDeployCellSize - 2, deployCellColor(r, c));
            tft.drawRect(x, y, kDeployCellSize, kDeployCellSize, theme::COLOR_ACCENT_DARK);
        }
    }
    // Preview: outline cells the current ship would occupy at the cursor.
    uint8_t size = shipnet::kShipSizes[s_deployShip];
    for (uint8_t i = 0; i < size; i++) {
        int r = s_deployHorizontal ? s_deployRow : s_deployRow + i;
        int c = s_deployHorizontal ? s_deployCol + i : s_deployCol;
        if (r >= shipnet::kBoardSize || c >= shipnet::kBoardSize) continue;
        int16_t x = (int16_t)(kDeployGridX + c * kDeployCellSize);
        int16_t y = (int16_t)(kDeployGridY + r * kDeployCellSize);
        tft.drawRect(x, y, kDeployCellSize, kDeployCellSize, ST77XX_WHITE);
        tft.drawRect(x + 1, y + 1, kDeployCellSize - 2, kDeployCellSize - 2, ST77XX_WHITE);
    }
}

constexpr int16_t kDeployPanelX = (int16_t)(kDeployGridX + shipnet::kBoardSize * kDeployCellSize + 14);

static void drawDeployPanel(Adafruit_ST7789& tft) {
    tft.fillRect(kDeployPanelX, kDeployGridY - 10, cfg::DISPLAY_WIDTH - kDeployPanelX, 190, theme::COLOR_BG);
    int16_t y = kDeployGridY;
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    for (int i = 0; i < shipnet::kShipCount; i++) {
        bool placed = shipnet::shipPlaced(i);
        bool current = (i == s_deployShip);
        tft.setTextColor(current ? ST77XX_WHITE : (placed ? theme::COLOR_ACCENT : theme::COLOR_ACCENT_DARK));
        tft.setCursor(kDeployPanelX, y);
        tft.print(current ? "> " : "  ");
        tft.print(kShipNames[i]);
        if (placed) tft.print(" *");
        y += theme::BODY_LINE_HEIGHT;
    }
    y += 8;
    tft.setTextColor(theme::COLOR_TEXT);
    tft.setCursor(kDeployPanelX, y);
    tft.print(s_deployHorizontal ? "Horiz" : "Vert");
}

static void drawDeploy() {
    auto& tft = display::tft();
    if (!s_deployChromeDrawn) {
        ui::widgets::clearScreen(tft);
        ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y}, "Deploy Fleet");
        theme::drawCentered(tft, "PAUSE=rotate  START=auto  OK=place", cfg::DISPLAY_HEIGHT - 16,
                            theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
        drawGridLabels(tft, kDeployGridX, kDeployGridY, kDeployCellSize);
        s_deployChromeDrawn = true;
    }
    drawDeployGrid(tft);
    drawDeployPanel(tft);
    if (shipnet::fleetComplete()) {
        tft.fillRect(0, cfg::DISPLAY_HEIGHT - 32, cfg::DISPLAY_WIDTH, 14, theme::COLOR_BG);
        theme::drawCentered(tft, "All placed -- OK to confirm", cfg::DISPLAY_HEIGHT - 32,
                            theme::BODY_TEXT_SIZE, theme::COLOR_SUCCESS);
    }
}

static void deployFrame() {
    if (input::wasPressed(input::Button::JoyUp) && s_deployRow > 0) { s_deployRow--; s_dirty = true; }
    if (input::wasPressed(input::Button::JoyDown) && s_deployRow < shipnet::kBoardSize - 1) { s_deployRow++; s_dirty = true; }
    if (input::wasPressed(input::Button::JoyLeft) && s_deployCol > 0) { s_deployCol--; s_dirty = true; }
    if (input::wasPressed(input::Button::JoyRight) && s_deployCol < shipnet::kBoardSize - 1) { s_deployCol++; s_dirty = true; }
    if (input::wasPressed(input::Button::Pause)) { s_deployHorizontal = !s_deployHorizontal; s_dirty = true; }
    if (input::wasPressed(input::Button::Start)) {
        shipnet::autoPlaceFleet();
        advanceToNextUnplacedShip();
        s_dirty = true;
    }
    bool confirm = input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::JoySelect);
    if (confirm) {
        if (shipnet::fleetComplete()) {
            shipnet::confirmFleetReady();
            enterBattle();
        } else if (shipnet::placeShip(s_deployShip, s_deployRow, s_deployCol, s_deployHorizontal)) {
            ui::widgets::navSfx();
            advanceToNextUnplacedShip();
            s_dirty = true;
        }
    }
}

// ---------------------------------------------------------------------
// Battle -- covers shipnet::Phase::WaitFleet/MyTurn/AwaitResult/TheirTurn
// ---------------------------------------------------------------------
constexpr int16_t kBattleGridX = 116, kBattleGridY = 58; // shifted from 46 to leave room for coordinate labels
constexpr int16_t kBattleCellSize = 16;
constexpr int16_t kMiniGridX = 8, kMiniGridY = 58;
constexpr int16_t kMiniCellSize = 8;

static uint16_t theirCellColor(shipnet::Cell c) {
    switch (c) {
        case shipnet::Cell::Miss: return theme::COLOR_ACCENT_DARK;
        case shipnet::Cell::Hit:  return theme::COLOR_DANGER;
        case shipnet::Cell::Sunk: return ST77XX_RED;
        default: return theme::COLOR_BG;
    }
}

static void drawBattleTargetGrid(Adafruit_ST7789& tft) {
    for (int r = 0; r < shipnet::kBoardSize; r++) {
        for (int c = 0; c < shipnet::kBoardSize; c++) {
            int16_t x = (int16_t)(kBattleGridX + c * kBattleCellSize);
            int16_t y = (int16_t)(kBattleGridY + r * kBattleCellSize);
            tft.fillRect(x + 1, y + 1, kBattleCellSize - 2, kBattleCellSize - 2,
                        theirCellColor(shipnet::theirCell((uint8_t)r, (uint8_t)c)));
            tft.drawRect(x, y, kBattleCellSize, kBattleCellSize, theme::COLOR_ACCENT_DARK);
        }
    }
    if (shipnet::phase() == shipnet::Phase::MyTurn) {
        int16_t x = (int16_t)(kBattleGridX + s_fireCol * kBattleCellSize);
        int16_t y = (int16_t)(kBattleGridY + s_fireRow * kBattleCellSize);
        tft.drawRect(x, y, kBattleCellSize, kBattleCellSize, ST77XX_WHITE);
    }
    if (shipnet::phase() == shipnet::Phase::AwaitResult) {
        uint8_t cell = shipnet::pendingFireCell();
        int16_t x = (int16_t)(kBattleGridX + (cell & 0x0F) * kBattleCellSize);
        int16_t y = (int16_t)(kBattleGridY + (cell >> 4) * kBattleCellSize);
        tft.drawRect(x, y, kBattleCellSize, kBattleCellSize, ST77XX_WHITE);
    }
}

static uint16_t myCellColor(shipnet::Cell c) {
    switch (c) {
        case shipnet::Cell::Ship: return theme::COLOR_ACCENT;
        case shipnet::Cell::Hit:  return theme::COLOR_DANGER;
        case shipnet::Cell::Sunk: return ST77XX_RED;
        case shipnet::Cell::Miss: return theme::COLOR_ACCENT_DARK;
        default: return theme::COLOR_BG;
    }
}

static void drawBattleMiniGrid(Adafruit_ST7789& tft) {
    for (int r = 0; r < shipnet::kBoardSize; r++) {
        for (int c = 0; c < shipnet::kBoardSize; c++) {
            int16_t x = (int16_t)(kMiniGridX + c * kMiniCellSize);
            int16_t y = (int16_t)(kMiniGridY + r * kMiniCellSize);
            tft.fillRect(x, y, kMiniCellSize - 1, kMiniCellSize - 1, myCellColor(shipnet::myCell((uint8_t)r, (uint8_t)c)));
        }
    }
}

static const char* battleStatusLine() {
    switch (shipnet::phase()) {
        case shipnet::Phase::WaitFleet:    return "Waiting for opponent's fleet...";
        case shipnet::Phase::MyTurn:       return "YOUR TURN -- pick a cell";
        case shipnet::Phase::AwaitResult:  return "Firing...";
        case shipnet::Phase::TheirTurn:    return "Opponent's turn...";
        default: return "";
    }
}

static void drawBattle() {
    auto& tft = display::tft();
    shipnet::Phase phase = shipnet::phase();
    if (!s_battleChromeDrawn) {
        ui::widgets::clearScreen(tft);
        ui::widgets::header(tft, ui::Rect{0, 0, (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_START_Y},
                            shipnet::opponentName()[0] ? shipnet::opponentName() : "Ship Battle");
        drawGridLabels(tft, kBattleGridX, kBattleGridY, kBattleCellSize);
        s_battleChromeDrawn = true;
        s_battleLastMyRemaining = -1;
        s_battleLastTheirRemaining = -1;
    }
    drawBattleMiniGrid(tft);
    drawBattleTargetGrid(tft);

    if (phase != s_battleLastPhase) {
        tft.fillRect(0, 22, cfg::DISPLAY_WIDTH, 12, theme::COLOR_BG);
        theme::drawCentered(tft, battleStatusLine(), 24, theme::BODY_TEXT_SIZE,
                            phase == shipnet::Phase::MyTurn ? theme::COLOR_SUCCESS : theme::COLOR_ACCENT_DARK);
        s_battleLastPhase = phase;
    }
    int myRem = shipnet::myShipsRemaining(), theirRem = shipnet::theirShipsRemaining();
    if (myRem != s_battleLastMyRemaining || theirRem != s_battleLastTheirRemaining) {
        tft.fillRect(0, cfg::DISPLAY_HEIGHT - 16, cfg::DISPLAY_WIDTH, 14, theme::COLOR_BG);
        char buf[32];
        snprintf(buf, sizeof(buf), "You:%d  Opp:%d  BACK=forfeit", myRem, theirRem);
        theme::drawCentered(tft, buf, cfg::DISPLAY_HEIGHT - 16, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
        s_battleLastMyRemaining = myRem;
        s_battleLastTheirRemaining = theirRem;
    }
    if (shipnet::silentMs() > 10000 && (phase == shipnet::Phase::MyTurn || phase == shipnet::Phase::AwaitResult ||
                                        phase == shipnet::Phase::TheirTurn)) {
        tft.fillRect(0, 34, cfg::DISPLAY_WIDTH, 10, theme::COLOR_BG);
        theme::drawCentered(tft, "Reconnecting...", 34, theme::BODY_TEXT_SIZE, theme::COLOR_DANGER);
    }
}

// Fires once per phase transition; battleFrame() detects the transition.
static void onBattlePhaseEnter(shipnet::Phase newPhase) {
    switch (newPhase) {
        case shipnet::Phase::TheirTurn:
            // Hit/miss/sunk sting already played at resolution time below;
            // this transition itself needs no sound.
            break;
        default: break;
    }
}

static shipnet::Cell s_lastTheirCellSeen[shipnet::kBoardSize][shipnet::kBoardSize];
static int s_lastMyRemainingSeen = shipnet::kFleetCells;

static void battleFrame() {
    shipnet::Phase phase = shipnet::phase();
    if (phase != s_battleLastPhase) { s_dirty = true; onBattlePhaseEnter(phase); }
    else { s_dirty = true; } // grids can change from RX activity between our own input ticks

    if (phase == shipnet::Phase::MyTurn) {
        if (input::wasPressed(input::Button::JoyUp) && s_fireRow > 0) { s_fireRow--; s_dirty = true; }
        if (input::wasPressed(input::Button::JoyDown) && s_fireRow < shipnet::kBoardSize - 1) { s_fireRow++; s_dirty = true; }
        if (input::wasPressed(input::Button::JoyLeft) && s_fireCol > 0) { s_fireCol--; s_dirty = true; }
        if (input::wasPressed(input::Button::JoyRight) && s_fireCol < shipnet::kBoardSize - 1) { s_fireCol++; s_dirty = true; }
        bool confirm = input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::JoySelect);
        if (confirm) {
            if (shipnet::fireAt(s_fireRow, s_fireCol)) {
                audio::playSfx(kSfxFire, 1);
                led::playEffect(led::EffectId::ChallengeComplete, led::EffectParams{0, 1, 80, led::kMaskWhite});
            }
        }
    }

    // Detect a just-arrived FireResult to fire the matching sting once.
    for (int r = 0; r < shipnet::kBoardSize; r++) {
        for (int c = 0; c < shipnet::kBoardSize; c++) {
            shipnet::Cell now = shipnet::theirCell((uint8_t)r, (uint8_t)c);
            if (now != s_lastTheirCellSeen[r][c]) {
                s_lastTheirCellSeen[r][c] = now;
                if (now == shipnet::Cell::Hit) {
                    audio::playSfx(kSfxHit, 2);
                    led::playEffect(led::EffectId::ChallengeComplete, led::EffectParams{0, 1, 100, led::kMaskRed});
                } else if (now == shipnet::Cell::Sunk) {
                    audio::playSfx(kSfxSunk, 3);
                    led::playEffect(led::EffectId::ChallengeComplete, led::EffectParams{0, 2, 120, led::kMaskRed});
                } else if (now == shipnet::Cell::Miss) {
                    audio::playSfx(kSfxMiss, 1);
                }
            }
        }
    }
    // Detect an incoming hit against MY fleet the same way.
    int myRemNow = shipnet::myShipsRemaining();
    if (myRemNow < s_lastMyRemainingSeen) {
        audio::playSfx(kSfxHit, 2);
        led::playEffect(led::EffectId::ChallengeComplete, led::EffectParams{0, 1, 100, led::kMaskRed});
    }
    s_lastMyRemainingSeen = myRemNow;

    if (input::wasPressed(input::Button::Back)) {
        shipnet::forfeit();
        setScreen(Screen::GameOver);
    }
    if (phase == shipnet::Phase::GameOver || shipnet::phase() == shipnet::Phase::Aborted) {
        setScreen(Screen::GameOver);
    }
}

// ---------------------------------------------------------------------
// GameOver
// ---------------------------------------------------------------------
static bool s_gameOverAnnounced = false;

static const char* overTitle(shipnet::OverResult r) {
    switch (r) {
        case shipnet::OverResult::Win:               return "VICTORY";
        case shipnet::OverResult::Lose:               return "DEFEAT";
        case shipnet::OverResult::OpponentLost:       return "Opponent Lost";
        case shipnet::OverResult::YouForfeited:       return "You Forfeited";
        case shipnet::OverResult::OpponentForfeited:  return "Opponent Forfeited";
        default:                                       return "Game Over";
    }
}

static void drawGameOver() {
    auto& tft = display::tft();
    ui::widgets::clearScreen(tft);
    shipnet::OverResult r = shipnet::overResult();
    uint16_t color = (r == shipnet::OverResult::Win || r == shipnet::OverResult::OpponentForfeited)
                         ? theme::COLOR_SUCCESS
                         : (r == shipnet::OverResult::Lose ? theme::COLOR_DANGER : theme::COLOR_ACCENT_DARK);
    theme::drawCentered(tft, overTitle(r), 100, theme::HEADER_TEXT_SIZE, color);
    // A peer going silent (out of range) must not read as a win.
    if (r == shipnet::OverResult::OpponentLost) {
        theme::drawCentered(tft, "Connection lost -- not a win", 130, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
    }
    theme::drawCentered(tft, "OK to continue", 170, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
}

static void gameOverFrame(AppState* exitTo) {
    *exitTo = AppState::ShipBattle;
    if (!s_gameOverAnnounced) {
        s_gameOverAnnounced = true;
        shipnet::OverResult r = shipnet::overResult();
        if (r == shipnet::OverResult::Win || r == shipnet::OverResult::OpponentForfeited) {
            audio::playMelody(kSfxWin, 4, false);
            led::playEffect(led::EffectId::ChallengeComplete, led::EffectParams{0, 4, 130, led::kMaskAll});
        } else {
            audio::playMelody(kSfxLose, 3, false);
            led::playEffect(led::EffectId::FailurePulse);
        }
    }
    bool confirm = input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::Back) ||
                   input::wasPressed(input::Button::JoySelect);
    if (confirm) {
        s_gameOverAnnounced = false;
        led::stop();
        shipnet::cancelHostOrJoin();
        enterMenu();
    }
}

// ---------------------------------------------------------------------

void enter() {
    memset(s_lastTheirCellSeen, 0, sizeof(s_lastTheirCellSeen));
    s_lastMyRemainingSeen = shipnet::kFleetCells;
    String name = storage::loadMyIdentity().name;
    s_radioOffline = !shipnet::begin(name.length() ? name.c_str() : nullptr);
    if (s_radioOffline) { setScreen(Screen::RadioOffline); return; }
    setScreen(Screen::Startup);
}

AppState frame() {
    if (s_radioOffline) {
        if (input::wasPressed(input::Button::Back) || input::wasPressed(input::Button::Ok)) {
            shipnet::end();
            return AppState::GamesMenu;
        }
        if (s_dirty) {
            auto& tft = display::tft();
            ui::widgets::clearScreen(tft);
            theme::drawCentered(tft, "Radio Offline", 100, theme::HEADER_TEXT_SIZE, theme::COLOR_DANGER);
            theme::drawCentered(tft, "BACK to exit", cfg::DISPLAY_HEIGHT - 16, theme::BODY_TEXT_SIZE, theme::COLOR_ACCENT_DARK);
            s_dirty = false;
        }
        return AppState::ShipBattle;
    }

    shipnet::update(); // must run before drawing each tick

    // Back-to-exit only from Menu; deeper screens back out one level or forfeit.
    if (s_screen == Screen::Menu && input::wasPressed(input::Button::Back)) {
        shipnet::end();
        led::stop(); // in case a self-terminating flash from the last battle is still mid-flight
        return AppState::GamesMenu;
    }

    AppState exitTo = AppState::ShipBattle;
    switch (s_screen) {
        case Screen::Startup:
            if (millis() - s_screenAtMs >= kStartupMs) { enterMenu(); break; }
            if (s_dirty) { drawStartup(); s_dirty = false; }
            return AppState::ShipBattle;
        case Screen::Menu: menuFrame(&exitTo); break;
        case Screen::HostWaiting:
            if (input::wasPressed(input::Button::Back)) { shipnet::cancelHostOrJoin(); enterMenu(); }
            else if (shipnet::phase() == shipnet::Phase::Deploy) { enterDeploy(); }
            break;
        case Screen::JoinList:
            if (input::wasPressed(input::Button::Back)) { enterMenu(); }
            else joinListFrame();
            break;
        case Screen::Joining:
            if (input::wasPressed(input::Button::Back)) { shipnet::cancelHostOrJoin(); enterMenu(); }
            else if (shipnet::phase() == shipnet::Phase::Deploy) { enterDeploy(); }
            else if (shipnet::phase() == shipnet::Phase::JoinRejected) { setScreen(Screen::JoinRejected); }
            break;
        case Screen::JoinRejected:
            if (input::wasPressed(input::Button::Ok) || input::wasPressed(input::Button::Back)) {
                shipnet::cancelHostOrJoin();
                enterMenu();
            }
            break;
        case Screen::Deploy:
            if (input::wasPressed(input::Button::Back)) { shipnet::forfeit(); setScreen(Screen::GameOver); }
            else deployFrame();
            break;
        case Screen::Battle: battleFrame(); break;
        case Screen::GameOver: gameOverFrame(&exitTo); break;
        case Screen::RadioOffline: break; // handled above
    }

    if (s_dirty) {
        switch (s_screen) {
            case Screen::Startup: break;
            case Screen::Menu: drawMenu(); break;
            case Screen::HostWaiting: drawHostWaiting(); break;
            case Screen::JoinList: drawJoinList(); break;
            case Screen::Joining: drawJoining(); break;
            case Screen::JoinRejected: drawJoinRejected(); break;
            case Screen::Deploy: drawDeploy(); break;
            case Screen::Battle: drawBattle(); break;
            case Screen::GameOver: drawGameOver(); break;
            case Screen::RadioOffline: break;
        }
        s_dirty = false;
    }
    return exitTo;
}

} // namespace games::ship_battle

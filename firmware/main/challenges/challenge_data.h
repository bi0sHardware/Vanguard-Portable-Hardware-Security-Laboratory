#pragma once
#include <cstdint>

// Challenge content — the only file needed to add/remove/reword a stage;
// challenges.cpp/challenge_engine.cpp dispatch on it generically.
namespace challenges {

// Extend with new kinds as new stage *behaviors* are needed, not per stage.
enum class StageKind {
    Info,      // static description/hint panel only, no interactive or background logic
    UartLeak,  // Level 1: always-on hidden UART broadcast; no on-device interactive screen
    Telemetry, // Level 2: LoRa receive-and-forward + Transmit Setup verification (mission2.*)
    Uplink,    // Level 3: USB-Serial-driven authenticated uplink (mission3.*)
    Payload,   // Level 4: LoRa payload receive + USB download (mission4.*)
    FlagDesk,  // serial-driven flag submission/validation utility (spec: outside the exploit itself)
};

struct ChallengeStage {
    const char* id;          // stable key for NVS completion lookup and requiresId, must stay unique
    const char* name;
    const char* category;
    const char* description;
    const char* hint;
    // SHA-256 of the flag, not the flag itself — a flash dump can't recover it directly (see flagdesk.cpp checkFlag()).
    const uint8_t* flagSha256;
    const char* reward;      // flavor text shown on the completion screen; nullptr if none
    const char* requiresId;  // nullptr = always unlocked; else `id` of the stage that must be completed first
    StageKind kind;
    uint8_t difficulty;      // 1-5 (stars per the spec doc), 0 = n/a
};

static const uint8_t kFlagHashLvl1[32] = {
    0xbf, 0x1e, 0x2c, 0x7b, 0x76, 0x51, 0xed, 0x68, 0x14, 0x0e, 0x35, 0xf3, 0x7b, 0x5a, 0x77, 0x41,
    0x80, 0x4b, 0xeb, 0x50, 0xb4, 0x5c, 0xa6, 0x8b, 0x39, 0x3b, 0x46, 0x6a, 0x87, 0xaa, 0x3e, 0x87,
};
static const uint8_t kFlagHashLvl2[32] = {
    0x99, 0x68, 0x5a, 0xe6, 0x88, 0xb1, 0x8e, 0x76, 0x58, 0x35, 0x8b, 0xb2, 0x6d, 0xd8, 0x97, 0x26,
    0xfb, 0xae, 0x1d, 0x8d, 0x31, 0x14, 0xc9, 0x9f, 0xda, 0xc7, 0xaa, 0x31, 0x07, 0x10, 0x2a, 0x3e,
};
static const uint8_t kFlagHashLvl3[32] = {
    0xe6, 0x2a, 0x82, 0xc1, 0xd5, 0x2d, 0xae, 0x57, 0x91, 0xe5, 0x63, 0x83, 0xba, 0x09, 0x61, 0x86,
    0x7d, 0x00, 0x1b, 0x55, 0x6e, 0xe4, 0x22, 0x75, 0x8c, 0x92, 0x30, 0x90, 0x14, 0xd3, 0xd5, 0x5b,
};
static const uint8_t kFlagHashLvl4[32] = {
    0x2e, 0x32, 0x07, 0xe6, 0xc2, 0xfe, 0x52, 0xd2, 0xd7, 0x4e, 0xf7, 0x38, 0x42, 0x29, 0xa8, 0x2d,
    0xfc, 0x88, 0x45, 0x20, 0xc4, 0x06, 0xb8, 0xb4, 0xde, 0x17, 0x8a, 0x71, 0xcb, 0xa3, 0xa2, 0xef,
};

static const ChallengeStage kRegistry[] = {
    { "lvl1", "Level 1: UART Recon", "Embedded Hardware / UART",
      "An undocumented debug interface is leaking diagnostic data over a hidden UART channel.",
      "Try common UART baud rates against the unlabeled TX pin with a logic analyzer.",
      kFlagHashLvl1,
      "Vanguard Ground Terminal restored - RF receiver access unlocked.",
      nullptr, StageKind::UartLeak, 1 },

    { "lvl2", "Level 2: Satellite Recon", "RF / LoRa / Satellite Comms",
      "Vanguard telemetry is inbound over LoRa. Recover the protocol's frame fields from the raw hexadecimal.",
      "The protocol uses UI (Unnumbered Information) frames.",
      kFlagHashLvl2,
      "Frame structure and authentication command recovered.",
      "lvl1", StageKind::Telemetry, 3 },

    { "lvl3", "Level 3: Establishing the Uplink", "RF / Protocol Implementation",
      "The satellite is also broadcasting encrypted mission traffic. Recover it, then authenticate the uplink using what the badge itself is trying to tell you.",
      "Every flag you've found so far already follows one convention - the badge's own inscription just needs the same treatment once you've read what it says.",
      kFlagHashLvl3,
      "Authenticated satellite uplink established.",
      "lvl2", StageKind::Uplink, 3 },

    { "lvl4", "Level 4: Operation Vanguard", "File Recovery / Reverse Engineering",
      "The satellite is transmitting a classified payload. Recover the hidden image and extract the final flag.",
      "Check the downloaded file's first bytes against well-known compressed-format magic numbers.",
      kFlagHashLvl4,
      "Vanguard mission complete.",
      "lvl3", StageKind::Payload, 4 },

    { "flagdesk", "Submit Flag", "Utility",
      "Enter a recovered flag over PuTTY/serial for a pass/fail check against this badge's known flags.",
      nullptr, nullptr, nullptr,
      nullptr, StageKind::FlagDesk, 0 },
};
static const int kStageCount = sizeof(kRegistry) / sizeof(kRegistry[0]);

} // namespace challenges

#pragma once

// Firmware state machine — spec §9.
enum class AppState {
    Boot,
    Screensaver, // permanent home screen — boot lands here, major sections Back here
    MainMenu,
    Challenges,
    Peerdrop,
    GamesMenu,
    Tetris,
    Snake,
    SpaceShooter,
    Game2048,
    MusicPlayer,
    Settings,
    // Screensaver's PROFILE shortcut: distinct state (not a bool flag) so entering
    // Settings normally doesn't re-run enter() and clobber the WifiSetup screen back to List.
    ProfileSetup,
    Contacts,
    RadioChat,   // radiolink-based field-radio app (see main/radiochat/, docs/protocols/radio-chat.md)
    ShipBattle,  // LoRa multiplayer game, lives under the Games menu (see main/games/ship_battle.*)
    MissionComplete, // full-screen takeover once all four Challenge levels are done
    Glitched, // "Badge Attack" takeover -- see main/glitch/glitch.h; returns to whatever state it interrupted, unlike MissionComplete
};

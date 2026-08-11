#include "input.h"
#include "../../include/pins.h"
#include "../../include/config.h"
#include <Arduino.h>

namespace input {

struct ButtonCfg {
    int gpio;
    bool activeLow;
};

static const ButtonCfg kCfg[(int)Button::Count] = {
    { pins::JOY_UP,     true  },
    { pins::JOY_DOWN,   true  },
    { pins::JOY_LEFT,   true  },
    { pins::JOY_RIGHT,  true  },
    { pins::JOY_SELECT, true  },
    { pins::BTN_PAUSE,  false },
    { pins::BTN_DISP,   false },
    { pins::BTN_START,  false },
    { pins::BTN_BACK,   false },
    { pins::BTN_OK,     false },
};

static bool s_stable[(int)Button::Count];
static bool s_lastRaw[(int)Button::Count];
static unsigned long s_lastChangeMs[(int)Button::Count];
static bool s_pressedEdge[(int)Button::Count];
static bool s_releasedEdge[(int)Button::Count];

void init() {
    for (int i = 0; i < (int)Button::Count; i++) {
        if (kCfg[i].activeLow) {
            pinMode(kCfg[i].gpio, INPUT_PULLUP);
        } else {
            pinMode(kCfg[i].gpio, INPUT); // external pull-down on board
        }
        bool raw = digitalRead(kCfg[i].gpio);
        bool logical = kCfg[i].activeLow ? !raw : raw;
        s_stable[i] = logical;
        s_lastRaw[i] = logical;
        s_lastChangeMs[i] = millis();
        s_pressedEdge[i] = false;
        s_releasedEdge[i] = false;
    }
}

void update() {
    unsigned long now = millis();
    for (int i = 0; i < (int)Button::Count; i++) {
        bool raw = digitalRead(kCfg[i].gpio);
        bool logical = kCfg[i].activeLow ? !raw : raw;

        s_pressedEdge[i] = false;
        s_releasedEdge[i] = false;

        if (logical != s_lastRaw[i]) {
            s_lastChangeMs[i] = now;
            s_lastRaw[i] = logical;
        }

        if (logical != s_stable[i] && (now - s_lastChangeMs[i]) >= cfg::DEBOUNCE_MS) {
            s_stable[i] = logical;
            if (logical) s_pressedEdge[i] = true;
            else s_releasedEdge[i] = true;
        }
    }
}

bool isDown(Button b) { return s_stable[(int)b]; }
bool wasPressed(Button b) { return s_pressedEdge[(int)b]; }
bool wasReleased(Button b) { return s_releasedEdge[(int)b]; }

} // namespace input

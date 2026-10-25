#include "state.h"

#include <string.h>

static const char* const MODE_NAMES[] = {"auto", "manual", "effects"};
static const char* const COLOR_NAMES[] = {"none", "red", "yellow", "green"};
static const uint16_t SPEED_PERCENT[SPEED_LEVELS] = {40, 55, 75, 100, 135, 180, 250};
static constexpr uint8_t MODE_COUNT = sizeof(MODE_NAMES) / sizeof(MODE_NAMES[0]);
static constexpr uint8_t COLOR_COUNT = sizeof(COLOR_NAMES) / sizeof(COLOR_NAMES[0]);

const char* modeName(Mode mode) { return MODE_NAMES[(uint8_t)mode]; }
const char* colorName(Color color) { return COLOR_NAMES[(uint8_t)color]; }
uint16_t speedPercent(uint8_t level) { return SPEED_PERCENT[level < SPEED_LEVELS ? level : SPEED_NORMAL]; }

bool parseMode(const char* text, Mode& mode) {
    for (uint8_t i = 0; i < MODE_COUNT; ++i) {
        if (strcmp(text, MODE_NAMES[i]) == 0) { mode = (Mode)i; return true; }
    }
    return false;
}

bool parseColor(const char* text, Color& color) {
    for (uint8_t i = 1; i < COLOR_COUNT; ++i) {
        if (strcmp(text, COLOR_NAMES[i]) == 0) { color = (Color)i; return true; }
    }
    return false;
}

bool parseBrightness(const char* text, uint8_t& percent) {
    if (text[0] == '\0') return false;
    uint16_t parsed = 0;
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9') return false;
        parsed = (uint16_t)(parsed * 10 + (*cursor - '0'));
        if (parsed > 100) return false;
    }
    percent = (uint8_t)parsed;
    return true;
}

Command buttonCommand(ButtonId button, Gesture gesture) {
    return {CommandType::ButtonEvent, (uint8_t)(((uint8_t)button << 2) | (uint8_t)gesture)};
}

static void selectColor(LightState& state, Color color) {
    state.mode = Mode::Manual;
    state.manualColor = color;
}

static void stepSpeed(uint8_t& level, bool faster) {
    if (faster && level + 1 < SPEED_LEVELS) ++level;
    else if (!faster && level > 0) --level;
}

static void stepEffect(LightState& state, bool forward, uint8_t effectCount) {
    if (effectCount == 0) return;
    state.effectIndex = (uint8_t)((state.effectIndex + (forward ? 1 : effectCount - 1)) % effectCount);
}

// The color buttons mean different things per mode:
//   Manual:  press lights that chamber.
//   Auto:    GREEN faster, RED slower (press, repeating while held), YELLOW normal speed.
//   Effects: GREEN tap next / hold faster, RED tap previous / hold slower, YELLOW normal speed.
static void applyButton(LightState& state, ButtonId button, Gesture gesture, uint8_t effectCount) {
    if (button == ButtonId::Mode) {
        if (gesture == Gesture::Tap) state.mode = (Mode)(((uint8_t)state.mode + 1) % MODE_COUNT);
        else if (gesture == Gesture::Hold) state.mode = Mode::Auto;
        return;
    }
    const Color color = button == ButtonId::Red ? Color::Red : button == ButtonId::Yellow ? Color::Yellow : Color::Green;
    const bool up = button == ButtonId::Green;
    switch (state.mode) {
        case Mode::Manual:
            if (gesture == Gesture::Press) selectColor(state, color);
            break;
        case Mode::Auto:
            if (button == ButtonId::Yellow) {
                if (gesture == Gesture::Press) state.trafficSpeed = SPEED_NORMAL;
            } else if (gesture != Gesture::Tap) {
                stepSpeed(state.trafficSpeed, up);
            }
            break;
        case Mode::Effects:
            if (button == ButtonId::Yellow) {
                if (gesture == Gesture::Press) state.effectSpeed = SPEED_NORMAL;
            } else if (gesture == Gesture::Tap) {
                stepEffect(state, up, effectCount);
            } else if (gesture == Gesture::Hold) {
                stepSpeed(state.effectSpeed, up);
            }
            break;
    }
}

bool applyCommand(LightState& state, const Command& command, uint8_t effectCount) {
    const LightState before = state;
    switch (command.type) {
        case CommandType::SetMode:
            if (command.value < MODE_COUNT) state.mode = (Mode)command.value;
            break;
        case CommandType::SelectColor:
            if (command.value == 0 || command.value >= COLOR_COUNT) break;
            selectColor(state, (Color)command.value);
            break;
        case CommandType::SetBrightness:
            if (command.value <= 100) state.brightnessPercent = command.value;
            break;
        case CommandType::SetEffect:
            if (command.value == EFFECT_CYCLE) {
                state.mode = Mode::Effects;
                state.cycleEffects = true;
            } else if (command.value < effectCount) {
                state.mode = Mode::Effects;
                state.effectIndex = command.value;
                state.cycleEffects = false;
            }
            break;
        case CommandType::ButtonEvent: {
            const uint8_t button = command.value >> 2;
            const uint8_t gesture = command.value & 0x03;
            if (button <= (uint8_t)ButtonId::Green && gesture <= (uint8_t)Gesture::Hold) {
                applyButton(state, (ButtonId)button, (Gesture)gesture, effectCount);
            }
            break;
        }
    }
    return memcmp(&before, &state, sizeof(state)) != 0;
}

#include "state.h"

#include <string.h>

static const char* const MODE_NAMES[] = {"auto", "manual", "effects"};
static const char* const COLOR_NAMES[] = {"none", "red", "yellow", "green"};
static constexpr uint8_t MODE_COUNT = sizeof(MODE_NAMES) / sizeof(MODE_NAMES[0]);
static constexpr uint8_t COLOR_COUNT = sizeof(COLOR_NAMES) / sizeof(COLOR_NAMES[0]);

const char* modeName(Mode mode) { return MODE_NAMES[(uint8_t)mode]; }
const char* colorName(Color color) { return COLOR_NAMES[(uint8_t)color]; }

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

bool applyCommand(LightState& state, const Command& command, uint8_t effectCount) {
    const LightState before = state;
    switch (command.type) {
        case CommandType::SetMode:
            if (command.value < MODE_COUNT) state.mode = (Mode)command.value;
            break;
        case CommandType::NextMode:
            state.mode = (Mode)(((uint8_t)state.mode + 1) % MODE_COUNT);
            break;
        case CommandType::SelectColor:
            if (command.value == 0 || command.value >= COLOR_COUNT) break;
            state.mode = Mode::Manual;
            state.manualColor = (Color)command.value;
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
    }
    return memcmp(&before, &state, sizeof(state)) != 0;
}

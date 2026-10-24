// ============================================================================
// Costume state and the commands that change it.
//
// Pure C++ with no Arduino or FreeRTOS dependencies, so it is unit-tested on
// the host (`pio test -e native`). Only the lights core mutates LightState.
// ============================================================================
#pragma once

#include <stdint.h>

enum class Mode : uint8_t { Auto, Manual, Effects };
enum class Color : uint8_t { None, Red, Yellow, Green };

enum class CommandType : uint8_t { SetMode, NextMode, SelectColor, SetBrightness, SetEffect };

// SetEffect value that rotates through every effect instead of holding one.
static constexpr uint8_t EFFECT_CYCLE = 0xFF;

struct Command {
    CommandType type;
    uint8_t value;  // Mode, Color, brightness percent, or effect index depending on type.
};

struct LightState {
    Mode mode;
    Color manualColor;  // Never None: Manual mode always shows a chamber.
    uint8_t effectIndex;
    bool cycleEffects;
    uint8_t brightnessPercent;
};

const char* modeName(Mode mode);
const char* colorName(Color color);

bool parseMode(const char* text, Mode& mode);
// Accepts only the selectable colors red, yellow, and green.
bool parseColor(const char* text, Color& color);
bool parseBrightness(const char* text, uint8_t& percent);

// Returns true when the command changed the state.
bool applyCommand(LightState& state, const Command& command, uint8_t effectCount);

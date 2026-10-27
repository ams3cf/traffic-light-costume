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

// Physical buttons and what the debouncer reports for them. Tap = released
// before the long-press time; Hold = long-press time reached, then repeats.
enum class ButtonId : uint8_t { Mode, Red, Yellow, Green };
enum class Gesture : uint8_t { Press, Tap, Hold };

enum class CommandType : uint8_t { SetMode, SelectColor, SetBrightness, SetEffect, ButtonEvent, SetEffectColor };

// SetEffect value that rotates through every effect instead of holding one.
static constexpr uint8_t EFFECT_CYCLE = 0xFF;

// Speed levels index a multiplier table; SPEED_NORMAL is 1x.
static constexpr uint8_t SPEED_LEVELS = 7;
static constexpr uint8_t SPEED_NORMAL = 3;

// SetEffectColor slot value that restores the effect's default color.
static constexpr uint32_t EFFECT_COLOR_DEFAULT = 0xFF000000;

struct Command {
    CommandType type;
    uint8_t value;  // Mode, Color, brightness percent, effect index, or button event depending on type.
    uint8_t slot;   // SetEffectColor only: which of the effect's colors.
    uint32_t rgb;   // SetEffectColor only: 0xRRGGBB, or EFFECT_COLOR_DEFAULT.
};

struct LightState {
    Mode mode;
    Color manualColor;  // Never None: Manual mode always shows a chamber.
    uint8_t effectIndex;
    bool cycleEffects;
    uint8_t brightnessPercent;
    uint8_t trafficSpeed;  // Speed level of the Auto-mode cycle.
    uint8_t effectSpeed;   // Speed level of Effects-mode animation.
};

const char* modeName(Mode mode);
const char* colorName(Color color);
// Percent of normal speed for a level, e.g. 100 for SPEED_NORMAL.
uint16_t speedPercent(uint8_t level);

bool parseMode(const char* text, Mode& mode);
// Accepts only the selectable colors red, yellow, and green.
bool parseColor(const char* text, Color& color);
bool parseBrightness(const char* text, uint8_t& percent);
// Six hex digits, with or without a leading '#', or "default" (EFFECT_COLOR_DEFAULT).
bool parseRgb(const char* text, uint32_t& rgb);

Command buttonCommand(ButtonId button, Gesture gesture);

// Replaces each out-of-range field of a state restored from flash with the
// fallback's, so one bad value (e.g. an effect index from a longer playlist)
// does not throw away the rest.
void sanitizeState(LightState& state, const LightState& fallback, uint8_t effectCount);

// Returns true when the command changed the state. SetEffectColor changes the
// effect's colors, not LightState, so it is ignored here (see lights.cpp).
bool applyCommand(LightState& state, const Command& command, uint8_t effectCount);

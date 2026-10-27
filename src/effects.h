// ============================================================================
// Effects-mode playlist. Each renderer draws a full frame into the LED buffer;
// renderers may build on the previous frame (fades, fire), so the caller
// clears the buffer when switching effects.
//
// An effect may take up to MAX_EFFECT_COLORS user-chosen colors (the web page
// shows a picker for each); `colors` holds the effect's current colors,
// starting from its defaults.
// ============================================================================
#pragma once

#include <FastLED.h>

static constexpr uint8_t MAX_EFFECT_COLORS = 3;
static constexpr uint8_t MAX_EFFECTS = 40;  // Sizes the lights core's color table.

struct Effect {
    const char* name;  // At most 15 characters: also the NVS key for its saved colors.
    void (*render)(CRGB* leds, uint32_t now, const CRGB* colors);
    uint8_t colorCount;
    CRGB defaults[MAX_EFFECT_COLORS];
};

extern const Effect EFFECTS[];
extern const uint8_t EFFECT_COUNT;

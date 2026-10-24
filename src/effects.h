// ============================================================================
// Effects-mode playlist. Each renderer draws a full frame into the LED buffer;
// renderers may build on the previous frame (fades, fire), so the caller
// clears the buffer when switching effects.
// ============================================================================
#pragma once

#include <FastLED.h>

struct Effect {
    const char* name;
    void (*render)(CRGB* leds, uint32_t now);
};

extern const Effect EFFECTS[];
extern const uint8_t EFFECT_COUNT;

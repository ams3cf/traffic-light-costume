// ============================================================================
// Renderers behind the playlist in effects.cpp. Internal to the effects; the
// rest of the firmware uses effects.h. Signature: see Effect::render.
//
//   effects_classic.cpp  whole-chain fills and flashes
//   effects_motion.cpp   comets and arcs travelling around each lamp
//   effects_fields.cpp   patterns over the whole costume front (rings.h)
// ============================================================================
#pragma once

#include <FastLED.h>

#include "config.h"
#include "rings.h"

static inline CRGB scaled(CRGB color, uint8_t level) { return color.nscale8(level); }

void renderReactor(CRGB* leds, uint32_t now, const CRGB* colors);
void renderHaunted(CRGB* leds, uint32_t now, const CRGB* colors);
void renderRoulette(CRGB* leds, uint32_t now, const CRGB* colors);
void renderCelebration(CRGB* leds, uint32_t now, const CRGB* colors);
void renderColorStorm(CRGB* leds, uint32_t now, const CRGB* colors);
void renderIncandescent(CRGB* leds, uint32_t now, const CRGB* colors);
void renderHalloween(CRGB* leds, uint32_t now, const CRGB* colors);
void renderRainbow(CRGB* leds, uint32_t now, const CRGB* colors);
void renderConfetti(CRGB* leds, uint32_t now, const CRGB* colors);

void renderTripleOrbit(CRGB* leds, uint32_t now, const CRGB* colors);
void renderCyberpunk(CRGB* leds, uint32_t now, const CRGB* colors);
void renderRisingComet(CRGB* leds, uint32_t now, const CRGB* colors);
void renderRadar(CRGB* leds, uint32_t now, const CRGB* colors);
void renderHelix(CRGB* leds, uint32_t now, const CRGB* colors);
void renderPinwheel(CRGB* leds, uint32_t now, const CRGB* colors);
void renderSpinner(CRGB* leds, uint32_t now, const CRGB* colors);

void renderFire(CRGB* leds, uint32_t now, const CRGB* colors);
void renderScanner(CRGB* leds, uint32_t now, const CRGB* colors);
void renderPlasma(CRGB* leds, uint32_t now, const CRGB* colors);
void renderAurora(CRGB* leds, uint32_t now, const CRGB* colors);
void renderLavaLamp(CRGB* leds, uint32_t now, const CRGB* colors);
void renderRainbowRise(CRGB* leds, uint32_t now, const CRGB* colors);
void renderEqualizer(CRGB* leds, uint32_t now, const CRGB* colors);
void renderPolice(CRGB* leds, uint32_t now, const CRGB* colors);
void renderHeartbeat(CRGB* leds, uint32_t now, const CRGB* colors);
void renderTwinkle(CRGB* leds, uint32_t now, const CRGB* colors);
void renderLightning(CRGB* leds, uint32_t now, const CRGB* colors);

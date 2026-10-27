#include "effects.h"

#include "effect_renderers.h"

static constexpr CRGB NONE = CRGB(0, 0, 0);

// Order is the Cycle-all order; ring effects are spread between the whole-chain ones.
const Effect EFFECTS[] = {
    {"fire", renderFire, 1, {CRGB(255, 50, 0), NONE, NONE}},
    {"radar", renderRadar, 1, {CRGB(0, 255, 40), NONE, NONE}},
    {"rising_comet", renderRisingComet, 0, {}},
    {"helix", renderHelix, 2, {CRGB(0, 200, 255), CRGB(255, 0, 160), NONE}},
    {"celebration", renderCelebration, 0, {}},
    {"scanner", renderScanner, 1, {CRGB(255, 0, 0), NONE, NONE}},
    {"cyberpunk", renderCyberpunk, 2, {CRGB(0, 255, 255), CRGB(255, 20, 147), NONE}},
    {"plasma", renderPlasma, 0, {}},
    {"reactor", renderReactor, 1, {CRGB(0, 85, 255), NONE, NONE}},
    {"pinwheel", renderPinwheel, 2, {CRGB(255, 140, 0), CRGB(0, 40, 255), NONE}},
    {"confetti", renderConfetti, 0, {}},
    {"equalizer", renderEqualizer, 0, {}},
    {"roulette", renderRoulette, 3, {CRGB(0, 128, 0), CRGB(255, 255, 0), CRGB(255, 0, 0)}},
    {"aurora", renderAurora, 0, {}},
    {"halloween", renderHalloween, 2, {CRGB(150, 25, 0), CRGB(45, 0, 70), NONE}},
    {"spinner", renderSpinner, 1, {CRGB(0, 180, 255), NONE, NONE}},
    {"triple_orbit", renderTripleOrbit, 3, {CRGB(255, 0, 0), CRGB(0, 255, 0), CRGB(0, 0, 255)}},
    {"lava_lamp", renderLavaLamp, 2, {CRGB(255, 30, 0), CRGB(130, 0, 255), NONE}},
    {"color_storm", renderColorStorm, 0, {}},
    {"police", renderPolice, 2, {CRGB(255, 0, 0), CRGB(0, 0, 255), NONE}},
    {"haunted", renderHaunted, 2, {CRGB(60, 0, 100), CRGB(120, 25, 0), NONE}},
    {"heartbeat", renderHeartbeat, 1, {CRGB(255, 0, 20), NONE, NONE}},
    {"incandescent", renderIncandescent, 0, {}},
    {"twinkle", renderTwinkle, 2, {CRGB(255, 190, 90), CRGB(0, 0, 30), NONE}},
    {"rainbow", renderRainbow, 0, {}},
    {"lightning", renderLightning, 1, {CRGB(170, 190, 255), NONE, NONE}},
    {"rainbow_rise", renderRainbowRise, 0, {}},
};
const uint8_t EFFECT_COUNT = sizeof(EFFECTS) / sizeof(EFFECTS[0]);
static_assert(sizeof(EFFECTS) / sizeof(EFFECTS[0]) <= MAX_EFFECTS, "raise MAX_EFFECTS in effects.h");

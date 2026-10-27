#include "effect_renderers.h"

static constexpr uint16_t THIRD_TURN = 21845;
static constexpr uint16_t QUARTER_TURN = 16384;

// One comet per lamp, each a third of a turn ahead of the one below, one turn
// every 1.3 s. Anti-aliased heads with short tails, so they glide rather than step.
void renderTripleOrbit(CRGB* leds, uint32_t now, const CRGB* colors) {
    const uint16_t head = turnAngle(now, 1300);
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const uint8_t ring = ledRing(i);
        leds[i] = scaled(colors[ring], cometLevel(ledAngle(i), head + ring * THIRD_TURN, 13000, true));
    }
}

// Neon comets over a faint blue glow; colors are the outer lamps and the middle lamp.
void renderCyberpunk(CRGB* leds, uint32_t now, const CRGB* colors) {
    const uint16_t head = turnAngle(now, 2000);
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const uint8_t ring = ledRing(i);
        const uint8_t level = cometLevel(ledAngle(i), head + ring * (uint16_t)(65536UL * 9 / 29), QUARTER_TURN, true);
        leds[i] = scaled(blend(CRGB(0, 20, 60), colors[ring == 1 ? 1 : 0], level), qadd8(level, 24));
    }
}

// A comet climbs the whole chain, lamp by lamp, bottom to top, colored by where it is.
void renderRisingComet(CRGB* leds, uint32_t now, const CRGB*) {
    static constexpr uint32_t LENGTH = (uint32_t)LED_COUNT_TOTAL << 8;  // Positions in 1/256 LED.
    static constexpr uint32_t TAIL = 10 << 8;
    const uint32_t head = (uint32_t)((((uint64_t)now << 8) / 30) % LENGTH);
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const uint32_t behind = (head + LENGTH - ((uint32_t)i << 8)) % LENGTH;
        uint8_t level;
        if (behind <= TAIL) {
            level = (uint8_t)(255 - behind * 255 / TAIL);
            level = scale8(level, level);
        } else {
            const uint32_t ahead = LENGTH - behind;
            level = ahead < 256 ? (uint8_t)(255 - ahead) : 0;
        }
        leds[i] = CHSV((uint8_t)(i * 255 / LED_COUNT_TOTAL), 255, level);
    }
}

// A sweeping beam with a long afterglow, in step on all three lamps, leaving
// brief contacts behind it.
void renderRadar(CRGB* leds, uint32_t now, const CRGB* colors) {
    static uint8_t blips[LED_COUNT_TOTAL];
    const uint16_t beam = turnAngle(now, 2400);
    const CRGB blipColor = blend(colors[0], CRGB::White, 160);
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        blips[i] = qsub8(blips[i], 4);
        leds[i] = scaled(colors[0], cometLevel(ledAngle(i), beam, 3 * QUARTER_TURN, true));
        leds[i] += scaled(blipColor, blips[i]);
    }
    if (random8() < 10) blips[ringLed(random8(RING_COUNT), (uint16_t)(beam / RING_SPACING))] = 255;
}

// Two strands spin around each lamp, offset a little more on each lamp up, so
// the three read as one twisting double helix.
void renderHelix(CRGB* leds, uint32_t now, const CRGB* colors) {
    const uint16_t spin = turnAngle(now, 1800);
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const uint16_t head = spin + ledRing(i) * (THIRD_TURN / 2);
        leds[i] = scaled(colors[0], cometLevel(ledAngle(i), head, 9000, true));
        leds[i] += scaled(colors[1], cometLevel(ledAngle(i), head + 32768, 9000, true));
    }
}

// Six soft-edged sectors of two colors; neighbouring lamps turn opposite ways like gears.
void renderPinwheel(CRGB* leds, uint32_t now, const CRGB* colors) {
    const uint16_t spin = turnAngle(now, 5000);
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const uint16_t angle = ledAngle(i) + (ledRing(i) == 1 ? spin : (uint16_t)(0 - spin));
        const int16_t wave = (int16_t)sin8((uint8_t)(((uint32_t)angle * 3) >> 8)) - 128;
        const int16_t mix = 128 + wave * 3;
        leds[i] = blend(colors[0], colors[1], (uint8_t)(mix < 0 ? 0 : mix > 255 ? 255 : mix));
    }
}

// A loading-spinner arc that stretches and shrinks as it turns; the middle lamp
// turns the other way.
void renderSpinner(CRGB* leds, uint32_t now, const CRGB* colors) {
    const uint16_t rotation = turnAngle(now, 1600);
    const uint8_t stretch = sin8((uint8_t)(turnAngle(now, 2600) >> 8));
    const uint16_t length = (uint16_t)(5000 + stretch * 150);
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const uint8_t ring = ledRing(i);
        const uint16_t center = (ring == 1 ? (uint16_t)(0 - rotation) : rotation) + ring * THIRD_TURN;
        leds[i] = scaled(colors[0], arcLevel(ledAngle(i), center - length / 2, length));
    }
}

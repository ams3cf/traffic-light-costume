#include "effect_renderers.h"

// ---------------------------------------------------------------------------
// Fire: flames climb both sides of each lamp from its bottom, Fire2012-style,
// one heat column per side. The color sets the flame; its hottest parts burn
// toward white.
// ---------------------------------------------------------------------------
static constexpr uint8_t FIRE_CELLS = RING_SIZE / 2 + 1;  // Bottom to top of one side of a lamp.
static constexpr uint8_t FIRE_COOLING = 550 / FIRE_CELLS + 2 > 255 ? 255 : 550 / FIRE_CELLS + 2;

static CRGB flameColor(uint8_t heat, const CRGB& flame) {
    if (heat < 128) return scaled(flame, (uint8_t)(heat * 2));
    return blend(flame, CRGB(255, 220, 120), (uint8_t)((heat - 128) * 2));
}

void renderFire(CRGB* leds, uint32_t, const CRGB* colors) {
    static uint8_t heat[RING_COUNT * 2][FIRE_CELLS];
    for (uint8_t* column : heat) {
        for (uint8_t i = 0; i < FIRE_CELLS; ++i) column[i] = qsub8(column[i], random8(0, FIRE_COOLING));
        for (uint8_t i = FIRE_CELLS - 1; i >= 2; --i) column[i] = (uint8_t)((column[i - 1] + 2 * column[i - 2]) / 3);
        if (random8() < 120) {
            const uint8_t spark = random8(FIRE_CELLS < 3 ? FIRE_CELLS : 3);
            column[spark] = qadd8(column[spark], random8(160, 255));
        }
    }
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const uint8_t side = ledAngle(i) < 32768 ? 0 : 1;
        const uint8_t cell = (uint8_t)((ledHeight(i) * (FIRE_CELLS - 1) + 127) / 255);
        leds[i] = flameColor(heat[ledRing(i) * 2 + side][cell], colors[0]);
    }
}

// A soft horizontal bar sweeps up and down through all three lamps, leaving a trail.
void renderScanner(CRGB* leds, uint32_t now, const CRGB* colors) {
    static constexpr int16_t HALF_WIDTH = 40;
    fadeToBlackBy(leds, LED_COUNT_TOTAL, 28);
    const int16_t bar = 128 + (int16_t)((int32_t)sin16(turnAngle(now, 3200)) * 150 / 32768);
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const int16_t distance = abs((int16_t)ledY(i) - bar);
        if (distance < HALF_WIDTH) leds[i] |= scaled(colors[0], (uint8_t)(255 - distance * 255 / HALF_WIDTH));
    }
}

void renderPlasma(CRGB* leds, uint32_t now, const CRGB*) {
    const uint8_t t1 = (uint8_t)(now / 16), t2 = (uint8_t)(now / 23), t3 = (uint8_t)(now / 37);
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const uint8_t x = ledX(i), y = ledY(i);
        const uint16_t v = sin8((uint8_t)(x / 2 + t1)) + sin8((uint8_t)(y - t2)) + sin8((uint8_t)((x + 2 * y) / 3 + t3));
        leds[i] = ColorFromPalette(PartyColors_p, (uint8_t)(v / 3 + now / 50));
    }
}

DEFINE_GRADIENT_PALETTE(AURORA_GRADIENT){
    0, 0, 0, 0,  70, 0, 50, 10,  130, 0, 210, 70,  180, 0, 170, 150,  220, 70, 0, 160,  255, 150, 0, 120,
};

// Slow curtains of green, teal, and violet drifting across the costume.
void renderAurora(CRGB* leds, uint32_t now, const CRGB*) {
    static const CRGBPalette16 palette = AURORA_GRADIENT;
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const uint16_t x = ledX(i), y = ledY(i);
        const uint8_t shade = inoise8(x * 2, y * 2, (uint16_t)(now / 8));
        const uint8_t curtain = ease8InOutQuad(inoise8(x * 3 + 9000, (uint16_t)(y / 2 + now / 12), (uint16_t)(now / 7)));
        leds[i] = ColorFromPalette(palette, shade, curtain);
    }
}

// Blobs of the first color rise through the second, as in a lava lamp.
void renderLavaLamp(CRGB* leds, uint32_t now, const CRGB* colors) {
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const uint16_t x = ledX(i), y = ledY(i);
        const uint8_t blob = ease8InOutCubic(inoise8(x * 2, (uint16_t)(y * 3 - now / 6), (uint16_t)(now / 10)));
        const uint8_t glow = inoise8(x * 2 + 20000, (uint16_t)(y * 2 - now / 9), (uint16_t)(now / 14));
        leds[i] = scaled(blend(colors[1], colors[0], blob), qadd8(90, glow));
    }
}

// Horizontal rainbow bands rising up through the lamps.
void renderRainbowRise(CRGB* leds, uint32_t now, const CRGB*) {
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) leds[i] = CHSV((uint8_t)(ledY(i) - now / 10), 240, 255);
}

// Each lamp is a level meter filling from its bottom, green to red, with a
// falling peak marker, bouncing to an imaginary beat.
void renderEqualizer(CRGB* leds, uint32_t, const CRGB*) {
    static uint8_t level[RING_COUNT], target[RING_COUNT], peak[RING_COUNT];
    static uint8_t frame = 0;
    for (uint8_t ring = 0; ring < RING_COUNT; ++ring) {
        if (frame % 6 == ring * 2) target[ring] = random8(30, 255);
        level[ring] = level[ring] < target[ring] ? qadd8(level[ring], 28) : qsub8(level[ring], 9);
        if (level[ring] > target[ring] && level[ring] - target[ring] < 28) level[ring] = target[ring];
        peak[ring] = level[ring] > peak[ring] ? level[ring] : qsub8(peak[ring], 2);
    }
    ++frame;
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const uint8_t ring = ledRing(i), height = ledHeight(i);
        if (height <= level[ring]) leds[i] = CHSV((uint8_t)(96 - height * 96 / 255), 255, 255);
        else if (height - peak[ring] < 14 && height >= peak[ring]) leds[i] = CRGB(200, 200, 200);
        else leds[i] = CRGB::Black;
    }
}

// Left and right halves double-flash in turn; the middle lamp is swapped.
void renderPolice(CRGB* leds, uint32_t now, const CRGB* colors) {
    const uint16_t t = (uint16_t)(now % 900);
    const bool first = t < 70 || (t >= 120 && t < 190);
    const bool second = (t >= 450 && t < 520) || (t >= 570 && t < 640);
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const bool left = (ledX(i) < 128) != (ledRing(i) == 1);
        leds[i] = left ? (first ? colors[0] : CRGB::Black) : (second ? colors[1] : CRGB::Black);
    }
}

// Lub-dub: a sharp beat then a softer one, starting in the middle lamp.
static uint8_t heartPulse(uint32_t t) {
    uint8_t level = 0;
    if (t < 40) level = (uint8_t)(t * 255 / 40);
    else if (t < 300) { const uint8_t f = (uint8_t)(255 - (t - 40) * 255 / 260); level = scale8(f, f); }
    if (t >= 280 && t < 320) level = (uint8_t)max<uint32_t>(level, (t - 280) * 170 / 40);
    else if (t >= 320 && t < 600) { const uint8_t f = (uint8_t)(170 - (t - 320) * 170 / 280); level = (uint8_t)max<uint32_t>(level, scale8(f, f) * 3 / 2); }
    return level;
}

void renderHeartbeat(CRGB* leds, uint32_t now, const CRGB* colors) {
    static constexpr uint32_t PERIOD_MS = 1100;
    uint8_t levels[RING_COUNT];
    for (uint8_t ring = 0; ring < RING_COUNT; ++ring) {
        const uint32_t delay = ring == 1 ? 0 : 70;
        levels[ring] = qadd8(10, heartPulse((now + PERIOD_MS - delay) % PERIOD_MS));
    }
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) leds[i] = scaled(colors[0], levels[ledRing(i)]);
}

// Stars fading in and out at their own pace over a night sky.
void renderTwinkle(CRGB* leds, uint32_t now, const CRGB* colors) {
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const uint16_t hash = (uint16_t)(i * 2053 + 13849);
        const uint8_t rate = (uint8_t)(2 + (hash >> 4) % 6);
        const uint8_t wave = sin8((uint8_t)(now * rate / 40 + (hash >> 7)));
        const uint8_t sparkle = (uint8_t)(qsub8(wave, 150) * 255 / 105);
        leds[i] = blend(colors[1], colors[0], ease8InOutQuad(sparkle));
    }
}

// Quiet, then a burst of flickering strikes along an arc of one lamp, with a
// faint flash across the rest.
void renderLightning(CRGB* leds, uint32_t, const CRGB* colors) {
    static uint16_t quietFrames = 40;
    static uint8_t flashes = 0, gapFrames = 0, ring = 0;
    static uint16_t start = 0, length = 0;
    fadeToBlackBy(leds, LED_COUNT_TOTAL, 70);
    if (flashes == 0) {
        if (quietFrames > 0) { --quietFrames; return; }
        flashes = random8(2, 6);
        ring = random8(RING_COUNT);
        start = random16();
        length = random16(16384, 49152);
        gapFrames = 0;
    }
    if (gapFrames > 0) { --gapFrames; return; }
    const uint8_t strength = random8(140, 255);
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) {
        const uint8_t bolt = ledRing(i) == ring ? scale8(arcLevel(ledAngle(i), start, length), strength) : 0;
        leds[i] = scaled(colors[0], max<uint8_t>(bolt, strength / 10));
    }
    gapFrames = random8(1, 5);
    if (--flashes == 0) quietFrames = random16(30, 160);
}

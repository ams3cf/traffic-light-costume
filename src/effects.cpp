#include "effects.h"

#include "config.h"

static void fillSection(CRGB* leds, uint8_t section, const CRGB& color) {
    fill_solid(leds + section * LED_COUNT_PER_SECTION, LED_COUNT_PER_SECTION, color);
}

static void renderReactor(CRGB* leds, uint32_t now) {
    const uint8_t pulse = sin8((uint8_t)(now / 8));
    fill_solid(leds, LED_COUNT_TOTAL, CRGB(0, pulse / 3, pulse));
}

static void renderHaunted(CRGB* leds, uint32_t now) {
    fill_solid(leds, LED_COUNT_TOTAL, ((now / 380) % 2) ? CRGB(60, 0, 100) : CRGB(120, 25, 0));
}

static void renderRoulette(CRGB* leds, uint32_t now) {
    static const CRGB colors[] = {CRGB::Green, CRGB::Yellow, CRGB::Red};
    const uint8_t section = (uint8_t)((now / 280) % 3);
    fill_solid(leds, LED_COUNT_TOTAL, CRGB::Black);
    fillSection(leds, section, colors[section]);
}

static void renderCelebration(CRGB* leds, uint32_t now) {
    fill_rainbow(leds, LED_COUNT_TOTAL, (uint8_t)(now / 8), 10);
    for (uint8_t i = 0; i < 6; ++i) leds[(now / 50 + i * 13) % LED_COUNT_TOTAL] += CRGB::White;
}

static void renderColorStorm(CRGB* leds, uint32_t now) { fill_rainbow(leds, LED_COUNT_TOTAL, (uint8_t)(now / 4), 19); }

static void renderIncandescent(CRGB* leds, uint32_t now) {
    const uint8_t pulse = sin8((uint8_t)(now / 8));
    fill_solid(leds, LED_COUNT_TOTAL, CRGB(180, 55 + pulse / 5, pulse / 12));
}

static void renderTripleOrbit(CRGB* leds, uint32_t now) {
    static const CRGB colors[] = {CRGB::Red, CRGB::Green, CRGB::Blue};
    fill_solid(leds, LED_COUNT_TOTAL, CRGB::Black);
    for (uint8_t section = 0; section < 3; ++section) {
        const uint16_t position = (uint16_t)((now / 45 + section * 10) % LED_COUNT_PER_SECTION);
        leds[section * LED_COUNT_PER_SECTION + position] = colors[section];
    }
}

static void renderHalloween(CRGB* leds, uint32_t now) {
    fill_solid(leds, LED_COUNT_TOTAL, ((now / 650) % 2) ? CRGB(150, 25, 0) : CRGB(45, 0, 70));
}

static void renderCyberpunk(CRGB* leds, uint32_t now) {
    fill_solid(leds, LED_COUNT_TOTAL, CRGB::Black);
    for (uint8_t section = 0; section < 3; ++section) {
        const uint16_t start = section * LED_COUNT_PER_SECTION;
        const uint16_t position = (uint16_t)((now / 70 + section * 9) % LED_COUNT_PER_SECTION);
        leds[start + ((position + LED_COUNT_PER_SECTION - 1) % LED_COUNT_PER_SECTION)] = CRGB(0, 20, 60);
        leds[start + position] = section == 1 ? CRGB::DeepPink : CRGB::Cyan;
    }
}

static void renderRainbow(CRGB* leds, uint32_t now) { fill_rainbow(leds, LED_COUNT_TOTAL, (uint8_t)(now / 18), 8); }

// Fire2012-style: each chamber is an independent column of heat that rises from its first pixel.
static void renderFire(CRGB* leds, uint32_t) {
    static constexpr uint16_t COOLING = 550 / LED_COUNT_PER_SECTION + 2;
    static uint8_t heat[LED_COUNT_TOTAL];
    for (uint8_t section = 0; section < 3; ++section) {
        uint8_t* column = heat + section * LED_COUNT_PER_SECTION;
        for (uint16_t i = 0; i < LED_COUNT_PER_SECTION; ++i) {
            column[i] = qsub8(column[i], random8(0, COOLING > 255 ? 255 : COOLING));
        }
        for (uint16_t i = LED_COUNT_PER_SECTION - 1; i >= 2; --i) {
            column[i] = (uint8_t)((column[i - 1] + 2 * column[i - 2]) / 3);
        }
        if (random8() < 120) {
            const uint8_t spark = random8(LED_COUNT_PER_SECTION < 3 ? LED_COUNT_PER_SECTION : 3);
            column[spark] = qadd8(column[spark], random8(160, 255));
        }
    }
    for (uint16_t i = 0; i < LED_COUNT_TOTAL; ++i) leds[i] = HeatColor(heat[i]);
}

static void renderConfetti(CRGB* leds, uint32_t now) {
    fadeToBlackBy(leds, LED_COUNT_TOTAL, 12);
    if (random8() < 90) leds[random16(LED_COUNT_TOTAL)] += CHSV((uint8_t)(now / 20 + random8(64)), 200, 255);
}

// A comet climbs the whole chain bottom -> middle -> top, shifting hue as it goes.
static void renderRisingComet(CRGB* leds, uint32_t now) {
    fadeToBlackBy(leds, LED_COUNT_TOTAL, 40);
    const uint16_t head = (uint16_t)((now / 30) % LED_COUNT_TOTAL);
    leds[head] = CHSV((uint8_t)(head * 255 / LED_COUNT_TOTAL), 255, 255);
}

const Effect EFFECTS[] = {
    {"fire", renderFire},
    {"rising_comet", renderRisingComet},
    {"celebration", renderCelebration},
    {"cyberpunk", renderCyberpunk},
    {"reactor", renderReactor},
    {"confetti", renderConfetti},
    {"roulette", renderRoulette},
    {"halloween", renderHalloween},
    {"triple_orbit", renderTripleOrbit},
    {"color_storm", renderColorStorm},
    {"haunted", renderHaunted},
    {"incandescent", renderIncandescent},
    {"rainbow", renderRainbow},
};
const uint8_t EFFECT_COUNT = sizeof(EFFECTS) / sizeof(EFFECTS[0]);

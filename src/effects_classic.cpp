#include "effect_renderers.h"

static void fillSection(CRGB* leds, uint8_t section, const CRGB& color) {
    fill_solid(leds + section * LED_COUNT_PER_SECTION, LED_COUNT_PER_SECTION, color);
}

void renderReactor(CRGB* leds, uint32_t now, const CRGB* colors) {
    fill_solid(leds, LED_COUNT_TOTAL, scaled(colors[0], sin8((uint8_t)(now / 8))));
}

void renderHaunted(CRGB* leds, uint32_t now, const CRGB* colors) {
    fill_solid(leds, LED_COUNT_TOTAL, ((now / 380) % 2) ? colors[0] : colors[1]);
}

// Colors are bottom, middle, top.
void renderRoulette(CRGB* leds, uint32_t now, const CRGB* colors) {
    const uint8_t section = (uint8_t)((now / 280) % 3);
    fill_solid(leds, LED_COUNT_TOTAL, CRGB::Black);
    fillSection(leds, section, colors[section]);
}

void renderCelebration(CRGB* leds, uint32_t now, const CRGB*) {
    fill_rainbow(leds, LED_COUNT_TOTAL, (uint8_t)(now / 8), 10);
    for (uint8_t i = 0; i < 6; ++i) leds[(now / 50 + i * 13) % LED_COUNT_TOTAL] += CRGB::White;
}

void renderColorStorm(CRGB* leds, uint32_t now, const CRGB*) { fill_rainbow(leds, LED_COUNT_TOTAL, (uint8_t)(now / 4), 19); }

void renderIncandescent(CRGB* leds, uint32_t now, const CRGB*) {
    const uint8_t pulse = sin8((uint8_t)(now / 8));
    fill_solid(leds, LED_COUNT_TOTAL, CRGB(180, 55 + pulse / 5, pulse / 12));
}

void renderHalloween(CRGB* leds, uint32_t now, const CRGB* colors) {
    fill_solid(leds, LED_COUNT_TOTAL, ((now / 650) % 2) ? colors[0] : colors[1]);
}

void renderRainbow(CRGB* leds, uint32_t now, const CRGB*) { fill_rainbow(leds, LED_COUNT_TOTAL, (uint8_t)(now / 18), 8); }

void renderConfetti(CRGB* leds, uint32_t now, const CRGB*) {
    fadeToBlackBy(leds, LED_COUNT_TOTAL, 12);
    if (random8() < 90) leds[random16(LED_COUNT_TOTAL)] += CHSV((uint8_t)(now / 20 + random8(64)), 200, 255);
}

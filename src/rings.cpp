#include "rings.h"

#include <math.h>

// Each lamp fills a third of the costume's height, with a small gap between lamps.
static constexpr float LAMP_RADIUS_Y = 38.0f;
static constexpr float LAMP_PITCH_Y = 85.0f;

static uint16_t g_angle[LED_COUNT_TOTAL];
static uint8_t g_ring[LED_COUNT_TOTAL];
static uint8_t g_height[LED_COUNT_TOTAL];
static uint8_t g_x[LED_COUNT_TOTAL];
static uint8_t g_y[LED_COUNT_TOTAL];

static uint8_t toByte(float value) { return (uint8_t)(value < 0 ? 0 : value > 255 ? 255 : lroundf(value)); }

uint16_t ringLed(uint8_t ring, uint16_t step) {
    step %= RING_SIZE;
    const uint16_t top = RING_TOP_LED[ring] % RING_SIZE;
    const uint16_t physical = RING_CLOCKWISE[ring] ? (top + step) % RING_SIZE : (top + RING_SIZE - step) % RING_SIZE;
    return (uint16_t)(ring * RING_SIZE + physical);
}

void ringsBegin() {
    for (uint8_t ring = 0; ring < RING_COUNT; ++ring) {
        for (uint16_t step = 0; step < RING_SIZE; ++step) {
            const uint16_t index = ringLed(ring, step);
            const uint16_t angle = (uint16_t)(step * RING_SPACING);
            const float radians = angle * (2.0f * (float)M_PI / 65536.0f);
            g_angle[index] = angle;
            g_ring[index] = ring;
            g_height[index] = toByte(127.5f + 127.5f * cosf(radians));
            g_x[index] = toByte(127.5f + 127.5f * sinf(radians));
            g_y[index] = toByte(LAMP_PITCH_Y * ring + LAMP_PITCH_Y / 2 + LAMP_RADIUS_Y * cosf(radians));
        }
    }
}

uint8_t ledRing(uint16_t index) { return g_ring[index]; }
uint16_t ledAngle(uint16_t index) { return g_angle[index]; }
uint8_t ledHeight(uint16_t index) { return g_height[index]; }
uint8_t ledX(uint16_t index) { return g_x[index]; }
uint8_t ledY(uint16_t index) { return g_y[index]; }

uint16_t turnAngle(uint32_t now, uint32_t periodMs) { return (uint16_t)(((uint64_t)now << 16) / periodMs); }

uint8_t cometLevel(uint16_t angle, uint16_t head, uint16_t tail, bool clockwise) {
    const uint16_t behind = clockwise ? (uint16_t)(head - angle) : (uint16_t)(angle - head);
    if (behind <= tail) {
        const uint8_t level = (uint8_t)(255 - (uint32_t)behind * 255 / tail);
        return scale8(level, level);  // Squared, so the tail fades out softly.
    }
    const uint32_t ahead = 65536UL - behind;
    return ahead < RING_SPACING ? (uint8_t)(255 - ahead * 255 / RING_SPACING) : 0;
}

// Each LED is treated as covering one spacing of the ring; the level is how much
// of that the arc covers, so the arc's ends glide smoothly too.
uint8_t arcLevel(uint16_t angle, uint16_t start, uint16_t length) {
    const uint32_t into = (uint16_t)(angle - start);
    const uint32_t half = RING_SPACING / 2;
    if (into < length) {
        const uint32_t edge = into < length - into ? into : length - into;
        return edge >= half ? 255 : (uint8_t)(128 + edge * 127 / half);
    }
    const uint32_t outside = into - length < 65536UL - into ? into - length : 65536UL - into;
    return outside >= half ? 0 : (uint8_t)(128 - outside * 128 / half);
}

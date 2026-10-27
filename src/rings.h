// ============================================================================
// Ring geometry. Each section is a ring of LEDs inside one lamp, and the
// lamps are stacked bottom (green), middle (yellow), top (red). This maps
// every LED to where it sits on the costume's front, so effects can draw
// motion around a lamp, or shapes that span all three lamps, without caring
// where each ring's chain starts (RING_TOP_LED / RING_CLOCKWISE in config.h).
//
// Angles are a full turn = 65536, clockwise from 12 o'clock seen from the
// front, so uint16_t arithmetic wraps around the ring for free.
// ============================================================================
#pragma once

#include <FastLED.h>

#include "config.h"

static constexpr uint8_t RING_COUNT = 3;
static constexpr uint16_t RING_SIZE = LED_COUNT_PER_SECTION;
// Angle between neighbouring LEDs.
static constexpr uint32_t RING_SPACING = 65536UL / RING_SIZE;

void ringsBegin();

// Buffer index of the LED `step` places clockwise from 12 o'clock on `ring` (0 = bottom).
uint16_t ringLed(uint8_t ring, uint16_t step);

// Geometry by buffer index.
uint8_t ledRing(uint16_t index);
uint16_t ledAngle(uint16_t index);
uint8_t ledHeight(uint16_t index);  // 0 at the bottom of its own lamp, 255 at the top.
uint8_t ledX(uint16_t index);       // 0 left edge of the lamps, 255 right edge.
uint8_t ledY(uint16_t index);       // 0 bottom of the bottom lamp, 255 top of the top lamp.

// Angle reached after `now` ms when one turn takes `periodMs`.
uint16_t turnAngle(uint32_t now, uint32_t periodMs);

// Brightness of an LED at `angle` for a comet with its head at `head` and a tail
// `tail` long (1-65535): full at the head, fading behind it, and anti-aliased in
// front so the head glides between LEDs instead of jumping.
uint8_t cometLevel(uint16_t angle, uint16_t head, uint16_t tail, bool clockwise);

// Brightness of an LED at `angle` inside an arc from `start` running clockwise
// for `length`, with anti-aliased ends.
uint8_t arcLevel(uint16_t angle, uint16_t start, uint16_t length);

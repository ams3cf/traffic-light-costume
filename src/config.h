// ============================================================================
// Traffic Light Costume — Central Configuration
//
// Every hardware-tunable value lives here so you can adapt the firmware to
// your exact wiring WITHOUT touching main.cpp. Nothing in this file is
// validated against physical hardware yet — treat all values as documented
// DEFAULTS, not verified facts.
// ============================================================================
#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// GPIO pin assignments
//
// Defaults target a generic ESP32 DevKit / NodeMCU-32S module. Pins chosen to
// be output-capable and free of boot-strapping behavior:
//
//   RED    = GPIO16
//   YELLOW = GPIO17
//   GREEN  = GPIO18
//
// AVOID for WS2812B data lines:
//   - GPIO 6-11  : connected to the onboard SPI flash (unusable)
//   - GPIO 0, 2  : boot-strapping (must be in a specific state at reset)
//   - GPIO 12, 15: boot-strapping (MTDI / MTDO)
//   - GPIO 34-39 : input-only (no output driver)
//   - GPIO 1, 3  : UART0 TX/RX (serial console — usable, but noisy)
// ---------------------------------------------------------------------------
#define PIN_RED    16
#define PIN_YELLOW 17
#define PIN_GREEN  18

// ---------------------------------------------------------------------------
// LED counts per ring
//
// WS2812B rings commonly ship in 8 / 12 / 16 / 24 / 60 LED variants.
// Default 8 per ring is a conservative starting point. Update to match the
// rings you actually bought — the firmware allocates one CRGB (3 bytes) per
// LED, so ESP32 SRAM is never a constraint here.
// ---------------------------------------------------------------------------
#define LED_COUNT_RED    8
#define LED_COUNT_YELLOW 8
#define LED_COUNT_GREEN  8

// ---------------------------------------------------------------------------
// Brightness
//
// FastLED master brightness, 0-255. Capped conservatively for two reasons:
//   1. A child's costume puts LEDs close to the eyes — full white is blinding.
//   2. Limiting brightness caps total current draw, keeping the power design
//      simple and safe (see README "Power budget" for the math).
// 80 ≈ 31% of full scale.
// ---------------------------------------------------------------------------
#define BRIGHTNESS_MASTER 80

// ---------------------------------------------------------------------------
// Traffic sequence timing (milliseconds)
//
// A simplified US-style cycle: GREEN -> YELLOW -> RED -> GREEN ...
// ---------------------------------------------------------------------------
#define TIMING_GREEN_MS   5000
#define TIMING_YELLOW_MS  2000
#define TIMING_RED_MS     5000

// ---------------------------------------------------------------------------
// Startup self-test (milliseconds per ring)
//
// On boot the firmware lights each ring in sequence so a wiring mistake is
// obvious before the traffic sequence starts. Reduce to ~150ms for a quick
// blink-check, or bump up to inspect each ring closely.
// ---------------------------------------------------------------------------
#define SELFTEST_STEP_MS 600

// ---------------------------------------------------------------------------
// Serial debug
// ---------------------------------------------------------------------------
#define SERIAL_BAUD  115200
#define SERIAL_DEBUG 1   // set to 0 to silence Serial output

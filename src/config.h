// ============================================================================
// Traffic Light Costume — Central Configuration
//
// Every hardware-tunable value lives here so you can adapt the firmware to
// your exact wiring WITHOUT touching the other sources. Values not listed as
// verified in the README are documented DEFAULTS, not verified facts.
// ============================================================================
#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// GPIO pin assignments
//
// Defaults target a generic ESP32 DevKit / NodeMCU-32S module. Pins chosen to
// be output-capable and free of boot-strapping behavior. The full LED chain
// uses one data output from the ESP32.
//
// AVOID for WS2812B data lines:
//   - GPIO 6-11  : connected to the onboard SPI flash (unusable)
//   - GPIO 0, 2  : boot-strapping (must be in a specific state at reset)
//   - GPIO 12, 15: boot-strapping (MTDI / MTDO)
//   - GPIO 34-39 : input-only (no output driver)
//   - GPIO 1, 3  : UART0 TX/RX (serial console — usable, but noisy)
// ---------------------------------------------------------------------------
#define PIN_LED_DATA 16
// Onboard blue LED on most ESP-WROOM-32 dev boards (active high).
#define PIN_STATUS_LED 2
#define PIN_MODE_BUTTON 18
#define PIN_RED_BUTTON 19
#define PIN_YELLOW_BUTTON 21
#define PIN_GREEN_BUTTON 22

// ---------------------------------------------------------------------------
// Daisy-chained LED sections
//
// The chain starts at the bottom section, then continues through the middle
// and top sections. The bench profile drives a 3-pixel test chain, one pixel
// per chamber, so every color is visible on the bench.
// ---------------------------------------------------------------------------
#if defined(BUILD_PROFILE_BENCH)
#define LED_COUNT_PER_SECTION 1
#else
#define LED_COUNT_PER_SECTION 29
#endif
#define LED_COUNT_TOTAL       (LED_COUNT_PER_SECTION * 3)
#define LED_OFFSET_BOTTOM    0
#define LED_OFFSET_MIDDLE    (LED_OFFSET_BOTTOM + LED_COUNT_PER_SECTION)
#define LED_OFFSET_TOP       (LED_OFFSET_MIDDLE + LED_COUNT_PER_SECTION)

// ---------------------------------------------------------------------------
// LED safety, timing, and firmware identity
// ---------------------------------------------------------------------------
#define DEFAULT_BRIGHTNESS_PERCENT 31
#define LED_CURRENT_BUDGET_MA 1500
#define LED_FRAME_INTERVAL_MS 25
#define EFFECT_STEP_MS 15000
#define BUTTON_DEBOUNCE_MS 40
#define BUTTON_LONG_PRESS_MS 1000
#define COMMAND_QUEUE_LENGTH 8
#define FIRMWARE_VERSION "2.0.0"

// ---------------------------------------------------------------------------
// Wi-Fi access point and web UI
//
// The costume runs its own WPA2 network; join it and open http://192.168.4.1.
// WPA2 requires a password of at least 8 characters. Override either value in
// src/secrets.h (git-ignored) if you want something other than the defaults.
// The lights run in loop() on core 1; the web server is pinned to core 0,
// alongside the ESP32 Wi-Fi stack.
// ---------------------------------------------------------------------------
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_AP_SSID
#define WIFI_AP_SSID "TrafficLight"
#endif
#ifndef WIFI_AP_PASSWORD
#define WIFI_AP_PASSWORD "greenlight"
#endif
#define WIFI_AP_CHANNEL 6
#define WIFI_AP_MAX_CLIENTS 4
#define WEB_TASK_CORE 0
#define WEB_TASK_STACK_BYTES 8192

// ---------------------------------------------------------------------------
// Traffic sequence timing (milliseconds)
//
// A simplified US-style cycle: GREEN -> YELLOW -> RED -> GREEN ...
// ---------------------------------------------------------------------------
#define TIMING_GREEN_MS   5000
#define TIMING_YELLOW_MS  2000
#define TIMING_RED_MS     5000

// ---------------------------------------------------------------------------
// Startup self-test (milliseconds per section)
//
// On boot the firmware lights each section in sequence so a wiring mistake is
// obvious before the traffic sequence starts. Reduce to ~150ms for a quick
// blink-check, or bump up to inspect each section closely.
// ---------------------------------------------------------------------------
#define SELFTEST_STEP_MS 600

// ---------------------------------------------------------------------------
// Serial debug
// ---------------------------------------------------------------------------
#define SERIAL_BAUD  115200
#ifndef SERIAL_DEBUG
#define SERIAL_DEBUG 1
#endif

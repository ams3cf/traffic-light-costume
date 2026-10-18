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
#define PIN_MODE_BUTTON 18
#define PIN_ACTION_BUTTON 19

// ---------------------------------------------------------------------------
// Daisy-chained LED sections
//
// The chain starts at the bottom section, then continues through the middle
// and top sections. Each section contains 29 LEDs.
// ---------------------------------------------------------------------------
#define LED_COUNT_PER_SECTION 29
#define LED_COUNT_TOTAL       (LED_COUNT_PER_SECTION * 3)
#define LED_OFFSET_BOTTOM    0
#define LED_OFFSET_MIDDLE    (LED_OFFSET_BOTTOM + LED_COUNT_PER_SECTION)
#define LED_OFFSET_TOP       (LED_OFFSET_MIDDLE + LED_COUNT_PER_SECTION)

// ---------------------------------------------------------------------------
// LED safety and firmware identity
// ---------------------------------------------------------------------------
#define DEFAULT_BRIGHTNESS_PERCENT 31
#define LED_CURRENT_BUDGET_MA 1500
#define POWER_PROFILE_NAME "limit_1500mA"
#define OPTICAL_PROFILE_NAME "diffused"
#define LED_FRAME_INTERVAL_MS 25
#define EFFECT_DURATION_MS 18000
#define SHOWCASE_STEP_MS 3000
#define BUTTON_DEBOUNCE_MS 35
#define BUTTON_LONG_PRESS_MS 1000
#define BLE_TEXT_MAX_LENGTH 23
#define BLE_STATE_MAX_LENGTH 160
#define BLE_DEVICE_INFO_MAX_LENGTH 64
#define BLE_NEGOTIATED_MTU 185
#define FIRMWARE_VERSION "1.0.0"
#define BLE_PROTOCOL_VERSION "1"

#define BLE_SERVICE_UUID       "9d4a0001-5f5b-4c1a-9f1a-7c2b6e8d0001"
#define BLE_COMMAND_UUID       "9d4a0002-5f5b-4c1a-9f1a-7c2b6e8d0002"
#define BLE_MODE_UUID          "9d4a0003-5f5b-4c1a-9f1a-7c2b6e8d0003"
#define BLE_EFFECT_UUID        "9d4a0004-5f5b-4c1a-9f1a-7c2b6e8d0004"
#define BLE_BRIGHTNESS_UUID    "9d4a0005-5f5b-4c1a-9f1a-7c2b6e8d0005"
#define BLE_STATE_UUID         "9d4a0006-5f5b-4c1a-9f1a-7c2b6e8d0006"
#define BLE_DEVICE_INFO_UUID   "9d4a0007-5f5b-4c1a-9f1a-7c2b6e8d0007"

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

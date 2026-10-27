// ============================================================================
// Settings that survive restarts and OTA updates, stored in NVS (namespace
// "costume"). NVS is thread-safe, so any task may call these. Writing an
// unchanged value does not touch flash.
// ============================================================================
#pragma once

#include <stddef.h>
#include <stdint.h>

static const char* const SETTING_BRIGHTNESS = "brightness";
static const char* const SETTING_NETWORK = "net";    // Preferred network when Wi-Fi is on: hotspot or home.
static const char* const SETTING_WIFI_ON = "wifi";
static const char* const SETTING_FLIP_DISPLAY = "flip";
static const char* const SETTING_MODE = "mode";
static const char* const SETTING_MANUAL_COLOR = "mcolor";
static const char* const SETTING_EFFECT = "effect";
static const char* const SETTING_CYCLE = "cycle";
static const char* const SETTING_TRAFFIC_SPEED = "tspeed";
static const char* const SETTING_EFFECT_SPEED = "espeed";

uint8_t settingsGet(const char* key, uint8_t fallback);
// Returns false (and logs) if the value could not be stored.
bool settingsSet(const char* key, uint8_t value);

// Effect colors live in their own namespace, keyed by effect name (NVS keys are
// at most 15 characters), so reordering the playlist keeps them. Get returns
// false when nothing of exactly `size` bytes is saved. Saving nullptr removes them.
bool settingsGetEffectColors(const char* effect, uint8_t* rgb, size_t size);
bool settingsSetEffectColors(const char* effect, const uint8_t* rgb, size_t size);

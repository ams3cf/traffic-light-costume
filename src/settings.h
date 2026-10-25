// ============================================================================
// Settings that survive restarts and OTA updates, stored in NVS (namespace
// "costume"). NVS is thread-safe, so any task may call these. Writing an
// unchanged value does not touch flash.
// ============================================================================
#pragma once

#include <stdint.h>

static const char* const SETTING_BRIGHTNESS = "brightness";
static const char* const SETTING_NETWORK = "net";

uint8_t settingsGet(const char* key, uint8_t fallback);
// Returns false (and logs) if the value could not be stored.
bool settingsSet(const char* key, uint8_t value);

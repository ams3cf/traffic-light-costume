#include "settings.h"

#include <Preferences.h>

static const char* const NAMESPACE = "costume";

uint8_t settingsGet(const char* key, uint8_t fallback) {
    Preferences prefs;
    // Read-write so the namespace is created on first boot instead of logging nvs_open NOT_FOUND.
    if (!prefs.begin(NAMESPACE, false)) return fallback;
    const uint8_t value = prefs.getUChar(key, fallback);
    prefs.end();
    return value;
}

bool settingsSet(const char* key, uint8_t value) {
    Preferences prefs;
    bool saved = prefs.begin(NAMESPACE, false);
    if (saved) {
        saved = prefs.getUChar(key, (uint8_t)~value) == value || prefs.putUChar(key, value) == 1;
        prefs.end();
    }
    if (!saved) log_e("Saving setting %s=%u failed; it will reset on restart", key, value);
    return saved;
}

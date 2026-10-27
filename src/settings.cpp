#include "settings.h"

#include <Preferences.h>
#include <string.h>

static const char* const NAMESPACE = "costume";
static const char* const EFFECT_COLORS_NAMESPACE = "fxcolors";

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

bool settingsGetEffectColors(const char* effect, uint8_t* rgb, size_t size) {
    Preferences prefs;
    if (!prefs.begin(EFFECT_COLORS_NAMESPACE, false)) return false;
    const bool found = prefs.getBytesLength(effect) == size && prefs.getBytes(effect, rgb, size) == size;
    prefs.end();
    return found;
}

bool settingsSetEffectColors(const char* effect, const uint8_t* rgb, size_t size) {
    Preferences prefs;
    bool saved = prefs.begin(EFFECT_COLORS_NAMESPACE, false);
    if (saved) {
        if (rgb == nullptr) {
            saved = !prefs.isKey(effect) || prefs.remove(effect);
        } else {
            uint8_t current[16];
            const bool unchanged = size <= sizeof(current) && prefs.getBytesLength(effect) == size &&
                                   prefs.getBytes(effect, current, size) == size && memcmp(current, rgb, size) == 0;
            saved = unchanged || prefs.putBytes(effect, rgb, size) == size;
        }
        prefs.end();
    }
    if (!saved) log_e("Saving colors for effect %s failed; they will reset on restart", effect);
    return saved;
}

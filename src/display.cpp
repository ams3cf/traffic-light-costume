#include "display.h"

#include <U8g2lib.h>
#include <Wire.h>
#include <ctype.h>
#include <string.h>

#include "config.h"
#include "control.h"
#include "network.h"
#include "settings.h"

// Passing the pins makes U8g2 call Wire.begin(SDA, SCL); without them it would
// claim GPIO21/22, which are the YELLOW and GREEN buttons.
static U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C g_oled(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA);

static const char* const HINTS_AUTO[] = {"tap R/Y/G: light a color", "hold G faster R slower", "hold Y: normal speed"};
static const char* const HINTS_MANUAL[] = {"R / Y / G: light a color"};
static const char* const HINTS_EFFECTS[] = {"tap G next  R previous", "hold G faster R slower", "Y: normal speed"};
static const char* const HINT_MODE = "MODE tap next, hold auto";
static constexpr uint8_t NETWORK_PAGES = 2;
static constexpr uint8_t CHORD_PAGES = 2;

static volatile bool g_flipped = false;

struct Screen {
    char title[20];
    char titleRight[8];
    char detail[26];
    char detailRight[6];
    char hint[26];
};

static bool probeDisplay() {
    Wire.beginTransmission(OLED_I2C_ADDRESS);
    return Wire.endTransmission() == 0;
}

static void upperCopy(char* out, size_t size, const char* text) {
    size_t i = 0;
    for (; text[i] != '\0' && i + 1 < size; ++i) out[i] = (char)toupper((unsigned char)text[i]);
    out[i] = '\0';
}

static void formatSpeed(char* out, size_t size, uint8_t level) {
    const uint16_t percent = speedPercent(level);
    snprintf(out, size, "%u.%02ux", percent / 100, percent % 100);
}

void displaySetFlipped(bool flipped) {
    g_flipped = flipped;
    settingsSet(SETTING_FLIP_DISPLAY, flipped ? 1 : 0);
}

bool displayFlipped() { return g_flipped; }

static void formatNetworkPage(char* out, size_t size, uint8_t page, const NetworkInfo& net) {
    const bool station = net.mode == NetworkMode::Station;
    if (net.mode == NetworkMode::Off) snprintf(out, size, "Wi-Fi is off");
    else if (page == 0 && station) snprintf(out, size, "home %s", net.ssid);
    else if (page == 0) snprintf(out, size, "%s / %s", net.ssid, WIFI_AP_PASSWORD);
    else if (station) snprintf(out, size, "%s OTA %s", net.address, net.otaEnabled ? "on" : "off");
    else snprintf(out, size, "%s  %u phone%s", net.address, net.clients, net.clients == 1 ? "" : "s");
}

// The bottom line: a notice if there is one, otherwise rotating pages of button
// hints for the mode, the Wi-Fi chords, then how to reach the costume.
static void formatHint(char* out, size_t size, Mode mode, const NetworkInfo& net, uint32_t now) {
    if (net.failed) { snprintf(out, size, "WI-FI FAILED: restart"); return; }
    if (net.notice[0] != '\0') { snprintf(out, size, "%s", net.notice); return; }
    const char* const* hints = HINTS_AUTO;
    uint8_t hintCount = sizeof(HINTS_AUTO) / sizeof(HINTS_AUTO[0]);
    if (mode == Mode::Manual) { hints = HINTS_MANUAL; hintCount = 1; }
    else if (mode == Mode::Effects) { hints = HINTS_EFFECTS; hintCount = sizeof(HINTS_EFFECTS) / sizeof(HINTS_EFFECTS[0]); }
    const uint8_t pageCount = hintCount + 1 + CHORD_PAGES + NETWORK_PAGES;
    const uint8_t page = (uint8_t)((now / DISPLAY_HINT_MS) % pageCount);
    const bool off = net.mode == NetworkMode::Off;
    if (page < hintCount) snprintf(out, size, "%s", hints[page]);
    else if (page == hintCount) snprintf(out, size, "%s", HINT_MODE);
    else if (page == hintCount + 1 && !off) snprintf(out, size, "hold MODE+G: %s", networkToggleTarget() == NetworkMode::Station ? "home WiFi" : "hotspot");
    else if (page <= hintCount + CHORD_PAGES) snprintf(out, size, "hold MODE+R: WiFi %s", off ? "on" : "off");
    else formatNetworkPage(out, size, (uint8_t)(page - hintCount - 1 - CHORD_PAGES), net);
}

static void composeScreen(Screen& screen, const Status& status, const NetworkInfo& net, uint32_t now) {
    memset(&screen, 0, sizeof(screen));
    if (net.otaPercent >= 0) {
        snprintf(screen.title, sizeof(screen.title), "UPDATING");
        snprintf(screen.titleRight, sizeof(screen.titleRight), "%d%%", net.otaPercent);
        snprintf(screen.detail, sizeof(screen.detail), "firmware over Wi-Fi");
        snprintf(screen.hint, sizeof(screen.hint), "do not power off");
        return;
    }
    if (net.connecting) {
        snprintf(screen.title, sizeof(screen.title), "JOINING");
        snprintf(screen.detail, sizeof(screen.detail), "%s", net.ssid);
        snprintf(screen.hint, sizeof(screen.hint), "hotspot if not found");
        return;
    }
    const LightState& state = status.state;
    upperCopy(screen.title, sizeof(screen.title), modeName(state.mode));
    switch (state.mode) {
        case Mode::Auto:
            formatSpeed(screen.titleRight, sizeof(screen.titleRight), state.trafficSpeed);
            snprintf(screen.detail, sizeof(screen.detail), "showing %s", colorName(status.lit));
            break;
        case Mode::Manual:
            upperCopy(screen.titleRight, sizeof(screen.titleRight), colorName(state.manualColor));
            snprintf(screen.detail, sizeof(screen.detail), "showing %s", colorName(state.manualColor));
            break;
        case Mode::Effects: {
            formatSpeed(screen.titleRight, sizeof(screen.titleRight), state.effectSpeed);
            snprintf(screen.detail, sizeof(screen.detail), "%s  %s", status.effectName != nullptr ? status.effectName : "",
                     state.cycleEffects ? "cycling" : "held");
            for (char* c = screen.detail; *c != '\0'; ++c) if (*c == '_') *c = ' ';
            break;
        }
    }
    snprintf(screen.detailRight, sizeof(screen.detailRight), "%u%%", state.brightnessPercent);
    formatHint(screen.hint, sizeof(screen.hint), state.mode, net, now);
}

static void drawRight(const char* text, uint8_t baseline) {
    g_oled.drawStr(128 - g_oled.getStrWidth(text), baseline, text);
}

static void drawScreen(const Screen& screen) {
    g_oled.clearBuffer();
    g_oled.setFont(u8g2_font_7x13B_tr);
    g_oled.drawStr(0, 10, screen.title);
    drawRight(screen.titleRight, 10);
    g_oled.setFont(u8g2_font_5x8_tr);
    g_oled.drawStr(0, 21, screen.detail);
    drawRight(screen.detailRight, 21);
    g_oled.drawStr(0, 31, screen.hint);
    g_oled.sendBuffer();
}

// Redraws only when the text or contrast changes. A failed probe marks the display
// gone, so it is re-initialized when plugged back in (it loses its setup on power loss).
static void displayTask(void*) {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL, OLED_I2C_CLOCK_HZ);
    bool ready = false;
    bool flipped = false;
    Screen shown = {};
    Screen screen = {};
    uint8_t contrast = DISPLAY_CONTRAST_FULL;
    uint32_t lastChangeMs = 0;
    for (;;) {
        const uint32_t now = millis();
        if (!probeDisplay()) {
#if SERIAL_DEBUG
            if (ready) Serial.println("OLED disconnected");
#endif
            ready = false;
            vTaskDelay(pdMS_TO_TICKS(DISPLAY_PROBE_MS));
            continue;
        }
        if (!ready) {
            g_oled.setBusClock(OLED_I2C_CLOCK_HZ);
            g_oled.begin();
            g_oled.setDisplayRotation(g_flipped ? U8G2_R2 : U8G2_R0);
            flipped = g_flipped;
            ready = true;
            memset(&shown, 0, sizeof(shown));
            contrast = 0;
            lastChangeMs = now;
#if SERIAL_DEBUG
            Serial.printf("OLED found at 0x%02X\n", OLED_I2C_ADDRESS);
#endif
        }
        if (flipped != g_flipped) {
            flipped = g_flipped;
            g_oled.setDisplayRotation(flipped ? U8G2_R2 : U8G2_R0);
            memset(&shown, 0, sizeof(shown));  // Forces a redraw the new way up.
            lastChangeMs = now;
        }
        composeScreen(screen, controlSnapshot(), networkInfo(), now);
        // Hint rotation alone does not count as activity, so the screen still dims.
        if (strcmp(screen.title, shown.title) != 0 || strcmp(screen.titleRight, shown.titleRight) != 0 ||
            strcmp(screen.detail, shown.detail) != 0 || strcmp(screen.detailRight, shown.detailRight) != 0) {
            lastChangeMs = now;
        }
        const uint8_t wanted = now - lastChangeMs >= DISPLAY_DIM_AFTER_MS ? DISPLAY_CONTRAST_DIM : DISPLAY_CONTRAST_FULL;
        if (wanted != contrast) {
            contrast = wanted;
            g_oled.setContrast(contrast);
        }
        if (memcmp(&screen, &shown, sizeof(screen)) != 0) {
            drawScreen(screen);
            shown = screen;
        }
        vTaskDelay(pdMS_TO_TICKS(DISPLAY_REFRESH_MS));
    }
}

void displayBegin() {
    g_flipped = settingsGet(SETTING_FLIP_DISPLAY, 0) != 0;
    xTaskCreatePinnedToCore(displayTask, "display", DISPLAY_TASK_STACK_BYTES, nullptr, 1, nullptr, WEB_TASK_CORE);
}

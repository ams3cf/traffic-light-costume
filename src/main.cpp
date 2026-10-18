// ============================================================================
// Traffic Light Costume — ESP32 firmware
// ============================================================================

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef OTA_WIFI_SSID
#define OTA_WIFI_SSID ""
#endif
#ifndef OTA_WIFI_PASSWORD
#define OTA_WIFI_PASSWORD ""
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

CRGB g_leds[LED_COUNT_TOTAL];

enum Mode : uint8_t { MODE_TRAFFIC, MODE_HALLOWEEN, MODE_CYBERPUNK, MODE_PARTY, MODE_DEMO, MODE_OFF };
enum Effect : uint8_t { EFFECT_NONE, EFFECT_REACTOR, EFFECT_HAUNTED, EFFECT_ROULETTE, EFFECT_CELEBRATION, EFFECT_COLOR_STORM, EFFECT_INCANDESCENT, EFFECT_TRIPLE_ORBIT, EFFECT_SHOWCASE };
enum Action : uint8_t { ACTION_MODE_WRITE, ACTION_EFFECT_WRITE, ACTION_BRIGHTNESS_WRITE, ACTION_COMMAND_WRITE, ACTION_NEXT_MODE, ACTION_RETURN_TRAFFIC, ACTION_SIGNATURE, ACTION_SHOWCASE, ACTION_INVALID };

struct PendingAction {
    Action action;
    char text[BLE_TEXT_MAX_LENGTH + 1];
};

struct ButtonState {
    uint8_t pin;
    bool stableHigh;
    bool rawHigh;
    bool longPressSent;
    uint32_t lastChangeMs;
    uint32_t pressedAtMs;
};

static const char* const MODE_NAMES[] = {"traffic", "halloween", "cyberpunk", "party", "demo", "off"};
static const char* const EFFECT_NAMES[] = {"none", "reactor", "haunted", "roulette", "celebration", "color_storm", "incandescent", "triple_orbit", "showcase"};
static const uint8_t SHOWCASE_EFFECTS[] = {EFFECT_REACTOR, EFFECT_HAUNTED, EFFECT_ROULETTE, EFFECT_CELEBRATION, EFFECT_COLOR_STORM, EFFECT_INCANDESCENT, EFFECT_TRIPLE_ORBIT};
static const uint32_t PHASE_DURATION_MS[] = {TIMING_GREEN_MS, TIMING_YELLOW_MS, TIMING_RED_MS};

static Mode g_mode = MODE_TRAFFIC;
static Mode g_resumeMode = MODE_TRAFFIC;
static Effect g_effect = EFFECT_NONE;
static uint8_t g_brightnessPercent = DEFAULT_BRIGHTNESS_PERCENT;
static uint8_t g_trafficPhase = 0;
static uint8_t g_showcaseIndex = 0;
static uint32_t g_phaseStartedMs = 0;
static uint32_t g_effectStartedMs = 0;
static uint32_t g_lastFrameMs = 0;
static bool g_paused = false;
static bool g_selfTestDone = false;
static bool g_otaStarted = false;
static char g_result[24] = "ok";

static NimBLECharacteristic* g_commandCharacteristic = nullptr;
static NimBLECharacteristic* g_modeCharacteristic = nullptr;
static NimBLECharacteristic* g_effectCharacteristic = nullptr;
static NimBLECharacteristic* g_brightnessCharacteristic = nullptr;
static NimBLECharacteristic* g_stateCharacteristic = nullptr;
static NimBLECharacteristic* g_infoCharacteristic = nullptr;

static constexpr uint8_t ACTION_QUEUE_SIZE = 8;
static PendingAction g_actionQueue[ACTION_QUEUE_SIZE];
static uint8_t g_queueHead = 0;
static uint8_t g_queueTail = 0;
static portMUX_TYPE g_queueMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool g_notifyOnConnect = false;
static char g_previousState[160] = "";

static ButtonState g_modeButton = {PIN_MODE_BUTTON, true, true, false, 0, 0};
static ButtonState g_actionButton = {PIN_ACTION_BUTTON, true, true, false, 0, 0};

static const char* modeName(Mode mode) { return MODE_NAMES[mode]; }
static const char* effectName(Effect effect) { return EFFECT_NAMES[effect]; }

static bool enqueueAction(Action action, const char* text, size_t length) {
    bool queued = false;
    portENTER_CRITICAL(&g_queueMux);
    const uint8_t next = (uint8_t)((g_queueHead + 1) % ACTION_QUEUE_SIZE);
    if (next != g_queueTail) {
        PendingAction& item = g_actionQueue[g_queueHead];
        item.action = action;
        if (length > BLE_TEXT_MAX_LENGTH) {
            item.action = ACTION_INVALID;
            item.text[0] = '\0';
        } else {
            if (length != 0 && text != nullptr) memcpy(item.text, text, length);
            item.text[length] = '\0';
        }
        g_queueHead = next;
        queued = true;
    }
    portEXIT_CRITICAL(&g_queueMux);
    return queued;
}

static bool dequeueAction(PendingAction& item) {
    bool available = false;
    portENTER_CRITICAL(&g_queueMux);
    if (g_queueTail != g_queueHead) {
        item = g_actionQueue[g_queueTail];
        g_queueTail = (uint8_t)((g_queueTail + 1) % ACTION_QUEUE_SIZE);
        available = true;
    }
    portEXIT_CRITICAL(&g_queueMux);
    return available;
}

class TextWriteCallbacks final : public NimBLECharacteristicCallbacks {
public:
    explicit TextWriteCallbacks(Action action) : action_(action) {}
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        const std::string value = characteristic->getValue();
        enqueueAction(action_, value.data(), value.size());
    }
private:
    Action action_;
};

class ServerCallbacks final : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer*, NimBLEConnInfo&) override { g_notifyOnConnect = true; }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override { NimBLEDevice::startAdvertising(); }
};

static TextWriteCallbacks g_commandWrite(ACTION_COMMAND_WRITE);
static TextWriteCallbacks g_modeWrite(ACTION_MODE_WRITE);
static TextWriteCallbacks g_effectWrite(ACTION_EFFECT_WRITE);
static TextWriteCallbacks g_brightnessWrite(ACTION_BRIGHTNESS_WRITE);
static ServerCallbacks g_serverCallbacks;

static void addUserDescription(NimBLECharacteristic* characteristic, const char* description) {
    auto* descriptor = new NimBLEDescriptor(NimBLEUUID((uint16_t)0x2901), NIMBLE_PROPERTY::READ, 64);
    descriptor->setValue(description);
    characteristic->addDescriptor(descriptor);
}

static NimBLECharacteristic* addCharacteristic(NimBLEService* service, const char* uuid, uint32_t properties, uint16_t maxLength, const char* description) {
    NimBLECharacteristic* characteristic = service->createCharacteristic(uuid, properties, maxLength);
    addUserDescription(characteristic, description);
    return characteristic;
}

static void startBle() {
    NimBLEDevice::init("TrafficLightCostume");
    NimBLEDevice::setMTU(BLE_NEGOTIATED_MTU);
    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(&g_serverCallbacks);
    NimBLEService* service = server->createService(BLE_SERVICE_UUID);
    g_commandCharacteristic = addCharacteristic(service, BLE_COMMAND_UUID, NIMBLE_PROPERTY::WRITE, BLE_TEXT_MAX_LENGTH, "Command: stop, resume, next, status");
    g_modeCharacteristic = addCharacteristic(service, BLE_MODE_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE, BLE_TEXT_MAX_LENGTH, "Persistent mode");
    g_effectCharacteristic = addCharacteristic(service, BLE_EFFECT_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE, BLE_TEXT_MAX_LENGTH, "One-shot effect");
    g_brightnessCharacteristic = addCharacteristic(service, BLE_BRIGHTNESS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE, BLE_TEXT_MAX_LENGTH, "Brightness percent 0-100");
    g_stateCharacteristic = addCharacteristic(service, BLE_STATE_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, BLE_STATE_MAX_LENGTH, "Current device state");
    g_infoCharacteristic = addCharacteristic(service, BLE_DEVICE_INFO_UUID, NIMBLE_PROPERTY::READ, BLE_DEVICE_INFO_MAX_LENGTH, "Firmware and protocol versions");
    g_commandCharacteristic->setCallbacks(&g_commandWrite);
    g_modeCharacteristic->setCallbacks(&g_modeWrite);
    g_effectCharacteristic->setCallbacks(&g_effectWrite);
    g_brightnessCharacteristic->setCallbacks(&g_brightnessWrite);
    g_infoCharacteristic->setValue(FIRMWARE_VERSION ";protocol=" BLE_PROTOCOL_VERSION ";ota=wifi-only");
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->enableScanResponse(true);
    advertising->addServiceUUID(BLE_SERVICE_UUID);
    advertising->setName("TrafficLightCostume");
    advertising->start();
}

static void syncGattValues() {
    if (g_modeCharacteristic != nullptr) {
        g_modeCharacteristic->setValue(modeName(g_mode));
        g_effectCharacteristic->setValue(effectName(g_effect));
        char brightness[4];
        snprintf(brightness, sizeof(brightness), "%u", g_brightnessPercent);
        g_brightnessCharacteristic->setValue(brightness);
        g_commandCharacteristic->setValue(g_result);
    }
}

static void updateStateCharacteristic(bool force = false) {
    char state[160];
    snprintf(state, sizeof(state), "mode=%s;effect=%s;brightness=%u;power=%s;optical=%s;fw=%s;protocol=%s;result=%s",
             modeName(g_mode), effectName(g_effect), g_brightnessPercent,
             POWER_PROFILE_NAME, OPTICAL_PROFILE_NAME, FIRMWARE_VERSION, BLE_PROTOCOL_VERSION, g_result);
    if (force || strcmp(state, g_previousState) != 0) {
        strncpy(g_previousState, state, sizeof(g_previousState) - 1);
        g_previousState[sizeof(g_previousState) - 1] = '\0';
        if (g_stateCharacteristic != nullptr) {
            g_stateCharacteristic->setValue(state);
            if (NimBLEDevice::getServer()->getConnectedCount() > 0) g_stateCharacteristic->notify();
        }
    }
}

static void setResult(const char* result) {
    strncpy(g_result, result, sizeof(g_result) - 1);
    g_result[sizeof(g_result) - 1] = '\0';
}

static void setMode(Mode mode) {
    g_effect = EFFECT_NONE;
    g_mode = mode;
    if (mode != MODE_OFF) g_resumeMode = mode;
    g_paused = false;
    g_phaseStartedMs = millis();
    g_trafficPhase = 0;
    setResult("ok");
}

static void triggerEffect(Effect effect) {
    if (effect == EFFECT_NONE) {
        setResult("error:use_mode");
        return;
    }
    if (g_effect == EFFECT_NONE) g_resumeMode = g_mode;
    g_effect = effect;
    g_effectStartedMs = millis();
    g_showcaseIndex = 0;
    g_paused = false;
    setResult("ok");
}

static bool parseMode(const char* value, Mode& mode) {
    for (uint8_t i = 0; i < sizeof(MODE_NAMES) / sizeof(MODE_NAMES[0]); ++i) {
        if (strcmp(value, MODE_NAMES[i]) == 0) { mode = (Mode)i; return true; }
    }
    return false;
}

static bool parseEffect(const char* value, Effect& effect) {
    for (uint8_t i = 1; i < sizeof(EFFECT_NAMES) / sizeof(EFFECT_NAMES[0]); ++i) {
        if (strcmp(value, EFFECT_NAMES[i]) == 0) { effect = (Effect)i; return true; }
    }
    return false;
}

static bool parseBrightness(const char* value, uint8_t& brightness) {
    if (value[0] == '\0') return false;
    uint16_t parsed = 0;
    for (const char* cursor = value; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9') return false;
        parsed = (uint16_t)(parsed * 10 + (*cursor - '0'));
        if (parsed > 100) return false;
    }
    brightness = (uint8_t)parsed;
    return true;
}

static Mode nextMode(Mode mode) { return (Mode)(((uint8_t)mode + 1) % (uint8_t)(sizeof(MODE_NAMES) / sizeof(MODE_NAMES[0]))); }

static Effect signatureEffect() {
    switch (g_mode) {
        case MODE_TRAFFIC: return EFFECT_ROULETTE;
        case MODE_HALLOWEEN: return EFFECT_HAUNTED;
        case MODE_CYBERPUNK: return EFFECT_TRIPLE_ORBIT;
        case MODE_PARTY: return EFFECT_CELEBRATION;
        case MODE_DEMO: return EFFECT_COLOR_STORM;
        case MODE_OFF: return EFFECT_REACTOR;
    }
    return EFFECT_REACTOR;
}

static void dispatch(Action action, const char* text = "") {
    Mode mode;
    Effect effect;
    uint8_t brightness;
    switch (action) {
        case ACTION_MODE_WRITE:
            if (parseMode(text, mode)) setMode(mode); else setResult("error:bad_mode");
            break;
        case ACTION_EFFECT_WRITE:
            if (parseEffect(text, effect)) triggerEffect(effect); else setResult("error:bad_effect");
            break;
        case ACTION_BRIGHTNESS_WRITE:
            if (parseBrightness(text, brightness)) { g_brightnessPercent = brightness; setResult("ok"); }
            else setResult("error:bad_brightness");
            break;
        case ACTION_COMMAND_WRITE:
            if (strcmp(text, "stop") == 0) {
                if (g_mode != MODE_OFF) g_resumeMode = g_mode;
                g_effect = EFFECT_NONE;
                g_mode = MODE_OFF;
                g_paused = false;
                setResult("ok");
            } else if (strcmp(text, "resume") == 0) {
                setMode(g_mode == MODE_OFF ? g_resumeMode : g_mode);
            } else if (strcmp(text, "next") == 0) {
                setMode(nextMode(g_mode));
            } else if (strcmp(text, "status") == 0) {
                setResult("ok");
            } else setResult("error:bad_command");
            break;
        case ACTION_NEXT_MODE: setMode(nextMode(g_mode)); break;
        case ACTION_RETURN_TRAFFIC: setMode(MODE_TRAFFIC); break;
        case ACTION_SIGNATURE: triggerEffect(signatureEffect()); break;
        case ACTION_SHOWCASE: triggerEffect(EFFECT_SHOWCASE); break;
        case ACTION_INVALID:
        default: setResult("error:invalid_write"); break;
    }
    syncGattValues();
    updateStateCharacteristic(action == ACTION_COMMAND_WRITE && strcmp(text, "status") == 0);
}

static void setSection(uint16_t offset, const CRGB& color) {
    fill_solid(g_leds, LED_COUNT_TOTAL, CRGB::Black);
    fill_solid(g_leds + offset, LED_COUNT_PER_SECTION, color);
}

static void renderTraffic(uint32_t now) {
    if (now - g_phaseStartedMs >= PHASE_DURATION_MS[g_trafficPhase]) {
        g_phaseStartedMs = now;
        g_trafficPhase = (uint8_t)((g_trafficPhase + 1) % 3);
    }
    if (g_trafficPhase == 0) setSection(LED_OFFSET_BOTTOM, CRGB::Green);
    else if (g_trafficPhase == 1) setSection(LED_OFFSET_MIDDLE, CRGB::Yellow);
    else setSection(LED_OFFSET_TOP, CRGB::Red);
}

static void renderMode(uint32_t now) {
    switch (g_mode) {
        case MODE_TRAFFIC: renderTraffic(now); break;
        case MODE_HALLOWEEN:
            fill_solid(g_leds, LED_COUNT_TOTAL, ((now / 650) % 2) ? CRGB(150, 25, 0) : CRGB(45, 0, 70));
            break;
        case MODE_CYBERPUNK:
            fill_solid(g_leds, LED_COUNT_TOTAL, CRGB::Black);
            for (uint8_t section = 0; section < 3; ++section) {
                const uint16_t start = section * LED_COUNT_PER_SECTION;
                const uint16_t position = (uint16_t)((now / 70 + section * 9) % LED_COUNT_PER_SECTION);
                g_leds[start + position] = section == 1 ? CRGB::DeepPink : CRGB::Cyan;
                g_leds[start + ((position + LED_COUNT_PER_SECTION - 1) % LED_COUNT_PER_SECTION)] = CRGB(0, 20, 60);
            }
            break;
        case MODE_PARTY: fill_rainbow(g_leds, LED_COUNT_TOTAL, (uint8_t)(now / 18), 8); break;
        case MODE_DEMO: {
            const uint8_t selection = (uint8_t)((now / 1200) % 4);
            fill_solid(g_leds, LED_COUNT_TOTAL, CRGB::Black);
            if (selection == 0) fill_solid(g_leds + LED_OFFSET_BOTTOM, LED_COUNT_PER_SECTION, CRGB::Green);
            else if (selection == 1) fill_solid(g_leds + LED_OFFSET_MIDDLE, LED_COUNT_PER_SECTION, CRGB::Yellow);
            else if (selection == 2) fill_solid(g_leds + LED_OFFSET_TOP, LED_COUNT_PER_SECTION, CRGB::Red);
            else fill_rainbow(g_leds, LED_COUNT_TOTAL, (uint8_t)(now / 18), 8);
            break;
        }
        case MODE_OFF: fill_solid(g_leds, LED_COUNT_TOTAL, CRGB::Black); break;
    }
}

static void renderEffect(Effect effect, uint32_t now) {
    const uint8_t pulse = sin8((uint8_t)(now / 8));
    switch (effect) {
        case EFFECT_REACTOR: fill_solid(g_leds, LED_COUNT_TOTAL, CRGB(0, pulse / 3, pulse)); break;
        case EFFECT_HAUNTED: fill_solid(g_leds, LED_COUNT_TOTAL, ((now / 380) % 2) ? CRGB(60, 0, 100) : CRGB(120, 25, 0)); break;
        case EFFECT_ROULETTE: {
            static const CRGB colors[] = {CRGB::Green, CRGB::Yellow, CRGB::Red};
            const uint8_t section = (uint8_t)((now / 280) % 3);
            setSection(section * LED_COUNT_PER_SECTION, colors[section]);
            break;
        }
        case EFFECT_CELEBRATION:
            fill_rainbow(g_leds, LED_COUNT_TOTAL, (uint8_t)(now / 8), 10);
            for (uint8_t i = 0; i < 6; ++i) g_leds[(now / 50 + i * 13) % LED_COUNT_TOTAL] += CRGB::White;
            break;
        case EFFECT_COLOR_STORM: fill_rainbow(g_leds, LED_COUNT_TOTAL, (uint8_t)(now / 4), 19); break;
        case EFFECT_INCANDESCENT: fill_solid(g_leds, LED_COUNT_TOTAL, CRGB(180, 55 + pulse / 5, pulse / 12)); break;
        case EFFECT_TRIPLE_ORBIT:
            fill_solid(g_leds, LED_COUNT_TOTAL, CRGB::Black);
            for (uint8_t section = 0; section < 3; ++section) {
                const uint16_t position = (uint16_t)((now / 45 + section * 10) % LED_COUNT_PER_SECTION);
                g_leds[section * LED_COUNT_PER_SECTION + position] = section == 0 ? CRGB::Red : section == 1 ? CRGB::Green : CRGB::Blue;
            }
            break;
        case EFFECT_NONE:
        case EFFECT_SHOWCASE: renderMode(now); break;
    }
}

static void render(uint32_t now) {
    if (g_paused) return;
    if (g_effect == EFFECT_NONE) renderMode(now);
    else if (g_effect == EFFECT_SHOWCASE) {
        const uint32_t elapsed = now - g_effectStartedMs;
        if (elapsed >= sizeof(SHOWCASE_EFFECTS) / sizeof(SHOWCASE_EFFECTS[0]) * SHOWCASE_STEP_MS) {
            g_effect = EFFECT_NONE;
            g_mode = g_resumeMode;
            setResult("ok");
            syncGattValues();
            updateStateCharacteristic();
            renderMode(now);
        } else {
            g_showcaseIndex = (uint8_t)(elapsed / SHOWCASE_STEP_MS);
            renderEffect((Effect)SHOWCASE_EFFECTS[g_showcaseIndex], now);
        }
    } else if (now - g_effectStartedMs >= EFFECT_DURATION_MS) {
        g_effect = EFFECT_NONE;
        g_mode = g_resumeMode;
        setResult("ok");
        syncGattValues();
        updateStateCharacteristic();
        renderMode(now);
    } else renderEffect(g_effect, now);
}

static void renderSelfTest(uint32_t now) {
    static uint8_t step = 0;
    static uint32_t stepStartedMs = 0;
    if (now - stepStartedMs >= SELFTEST_STEP_MS) {
        stepStartedMs = now;
        if (++step >= 3) { g_selfTestDone = true; g_phaseStartedMs = now; return; }
    }
    if (step == 0) setSection(LED_OFFSET_BOTTOM, CRGB::Green);
    else if (step == 1) setSection(LED_OFFSET_MIDDLE, CRGB::Yellow);
    else setSection(LED_OFFSET_TOP, CRGB::Red);
}

static void pollButton(ButtonState& button, uint32_t now, Action shortAction, Action longAction) {
    const bool rawHigh = digitalRead(button.pin) == HIGH;
    if (rawHigh != button.rawHigh) { button.rawHigh = rawHigh; button.lastChangeMs = now; }
    if (rawHigh != button.stableHigh && now - button.lastChangeMs >= BUTTON_DEBOUNCE_MS) {
        button.stableHigh = rawHigh;
        if (!rawHigh) { button.pressedAtMs = now; button.longPressSent = false; }
        else if (!button.longPressSent) enqueueAction(shortAction, nullptr, 0);
    }
    if (!button.stableHigh && !button.longPressSent && now - button.pressedAtMs >= BUTTON_LONG_PRESS_MS) {
        button.longPressSent = true;
        enqueueAction(longAction, nullptr, 0);
    }
}

static void beginWifiOta() {
    if (OTA_WIFI_SSID[0] == '\0' || OTA_WIFI_PASSWORD[0] == '\0' || OTA_PASSWORD[0] == '\0') return;
    WiFi.mode(WIFI_STA);
    WiFi.begin(OTA_WIFI_SSID, OTA_WIFI_PASSWORD);
}

static void serviceWifiOta() {
    if (!g_otaStarted && WiFi.status() == WL_CONNECTED) {
        ArduinoOTA.setHostname("TrafficLightCostume");
        ArduinoOTA.setPassword(OTA_PASSWORD);
        ArduinoOTA.begin();
        g_otaStarted = true;
    }
    if (g_otaStarted) ArduinoOTA.handle();
}

void setup() {
#if SERIAL_DEBUG
    Serial.begin(SERIAL_BAUD);
    Serial.println("Traffic Light Costume - booting");
#endif
    pinMode(PIN_MODE_BUTTON, INPUT_PULLUP);
    pinMode(PIN_ACTION_BUTTON, INPUT_PULLUP);
    FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(g_leds, LED_COUNT_TOTAL);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_CURRENT_BUDGET_MA);
    FastLED.setBrightness((uint8_t)((uint16_t)g_brightnessPercent * 255 / 100));
    fill_solid(g_leds, LED_COUNT_TOTAL, CRGB::Black);
    FastLED.show();
    startBle();
    syncGattValues();
    updateStateCharacteristic(true);
    beginWifiOta();
}

void loop() {
    const uint32_t now = millis();
    serviceWifiOta();
    pollButton(g_modeButton, now, ACTION_NEXT_MODE, ACTION_RETURN_TRAFFIC);
    pollButton(g_actionButton, now, ACTION_SIGNATURE, ACTION_SHOWCASE);
    PendingAction action;
    while (dequeueAction(action)) {
        g_selfTestDone = true;
        dispatch(action.action, action.text);
    }
    if (g_notifyOnConnect) {
        g_notifyOnConnect = false;
        updateStateCharacteristic(true);
    }
    if (now - g_lastFrameMs >= LED_FRAME_INTERVAL_MS) {
        g_lastFrameMs = now;
        if (g_selfTestDone) render(now);
        else renderSelfTest(now);
        FastLED.setBrightness((uint8_t)((uint16_t)g_brightnessPercent * 255 / 100));
        FastLED.show();
    }
}

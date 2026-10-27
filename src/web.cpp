#include "web.h"

#include <WebServer.h>

#include "buttons.h"
#include "config.h"
#include "control.h"
#include "display.h"
#include "effects.h"
#include "network.h"
#include "web_page.h"

static WebServer g_server(80);
static void sendJson(int code, const char* json) {
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send(code, "application/json", json);
}

static void sendError(int code, const char* message) {
    char json[128];
    snprintf(json, sizeof(json), "{\"error\":\"%s\"}", message);
    sendJson(code, json);
}

static void queueCommand(const Command& command) {
    if (controlSend(command)) sendJson(202, "{\"queued\":true}");
    else sendError(503, "command queue full; retry shortly");
}

static void handleStatus() {
    const Status status = controlSnapshot();
    char colors[40] = "";
    for (uint8_t slot = 0; slot < status.colorCount; ++slot) {
        const size_t used = strlen(colors);
        snprintf(colors + used, sizeof(colors) - used, "%s\"%06x\"", slot > 0 ? "," : "", (unsigned)status.colors[slot]);
    }
    char json[384];
    snprintf(json, sizeof(json),
             "{\"mode\":\"%s\",\"lit\":\"%s\",\"color\":\"%s\",\"effect\":\"%s\",\"cycle\":%s,\"colors\":[%s],"
             "\"brightness\":%u,\"trafficSpeed\":%u,\"effectSpeed\":%u,\"network\":\"%s\",\"homeWifi\":%s,"
             "\"flip\":%s,\"fw\":\"%s\"}",
             modeName(status.state.mode), colorName(status.lit), colorName(status.state.manualColor),
             status.effectName != nullptr ? status.effectName : "", status.state.cycleEffects ? "true" : "false", colors,
             status.state.brightnessPercent, speedPercent(status.state.trafficSpeed),
             speedPercent(status.state.effectSpeed), networkModeName(networkInfo().mode),
             networkHomeConfigured() ? "true" : "false", displayFlipped() ? "true" : "false", FIRMWARE_VERSION);
    sendJson(200, json);
}

// The playlist is a const table, so reading names from this core is safe.
static void handleEffects() {
    String json = "[";
    for (uint8_t i = 0; i < EFFECT_COUNT; ++i) {
        if (i > 0) json += ',';
        json += '"';
        json += EFFECTS[i].name;
        json += '"';
    }
    json += ']';
    sendJson(200, json.c_str());
}

static bool findEffect(const String& name, uint8_t& index) {
    for (uint8_t i = 0; i < EFFECT_COUNT; ++i) {
        if (name == EFFECTS[i].name) { index = i; return true; }
    }
    return false;
}

static void handleEffect() {
    const String name = g_server.arg("effect");
    if (name == "cycle") return queueCommand({CommandType::SetEffect, EFFECT_CYCLE, 0, 0});
    uint8_t index;
    if (findEffect(name, index)) return queueCommand({CommandType::SetEffect, index, 0, 0});
    sendError(400, "effect must be cycle or a name from /api/effects");
}

static void handleEffectColor() {
    uint8_t index;
    if (!findEffect(g_server.arg("effect"), index)) return sendError(400, "effect must be a name from /api/effects");
    const String slot = g_server.arg("slot");
    if (slot.length() != 1 || slot[0] < '0' || slot[0] - '0' >= EFFECTS[index].colorCount) {
        return sendError(400, "slot must be 0 up to the effect's color count minus 1");
    }
    uint32_t rgb;
    if (!parseRgb(g_server.arg("color").c_str(), rgb)) return sendError(400, "color must be rrggbb hex or default");
    queueCommand({CommandType::SetEffectColor, index, (uint8_t)(slot[0] - '0'), rgb});
}

static void handleMode() {
    Mode mode;
    if (!parseMode(g_server.arg("mode").c_str(), mode)) return sendError(400, "mode must be auto, manual, or effects");
    queueCommand({CommandType::SetMode, (uint8_t)mode, 0, 0});
}

static void handleColor() {
    Color color;
    if (!parseColor(g_server.arg("color").c_str(), color)) return sendError(400, "color must be red, yellow, or green");
    queueCommand({CommandType::SelectColor, (uint8_t)color, 0, 0});
}

static void handleBrightness() {
    uint8_t percent;
    if (!parseBrightness(g_server.arg("value").c_str(), percent)) return sendError(400, "value must be an integer 0-100");
    queueCommand({CommandType::SetBrightness, percent, 0, 0});
}

// Applied by networkLoop() after this reply has gone out.
static void handleNetwork() {
    NetworkMode mode;
    if (!parseNetworkMode(g_server.arg("mode").c_str(), mode)) return sendError(400, "mode must be hotspot, station, or off");
    if (mode == NetworkMode::Station && !networkHomeConfigured()) return sendError(400, "home Wi-Fi is not set up in src/secrets.h");
    networkRequest(mode);
    sendJson(202, "{\"queued\":true}");
}

static void handleSettings() {
    const String flip = g_server.arg("flip");
    if (flip != "0" && flip != "1") return sendError(400, "flip must be 0 or 1");
    displaySetFlipped(flip == "1");
    sendJson(200, "{\"saved\":true}");
}

static void appendCounters(String& json, const ButtonCounters& c) {
    char text[200];
    snprintf(text, sizeof(text),
             "{\"edges\":%u,\"shortestEdgeUs\":%u,\"sampledChanges\":%u,\"presses\":%u,\"taps\":%u,\"holds\":%u,\"chords\":%u}",
             c.edges, c.shortestEdgeUs, c.sampledChanges, c.presses, c.taps, c.holds, c.chords);
    json += text;
}

static void appendMask(String& json, const char* key, uint8_t mask) {
    json += ",\"";
    json += key;
    json += "\":[";
    bool first = true;
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        if (!((mask >> i) & 1)) continue;
        if (!first) json += ',';
        json += '"';
        json += BUTTON_NAMES[i];
        json += '"';
        first = false;
    }
    json += ']';
}

// Button diagnostics; see buttons.h. Logs are oldest first; times are micros() since boot.
static void handleDiagnostics() {
    static const char* const KINDS[] = {"press", "tap", "hold", "chord"};
    static ButtonDiagnostics diag;  // Too large for comfort on the web task's stack.
    buttonsDiagnostics(diag);
    String json;
    json.reserve(6000);
    char text[160];
    snprintf(text, sizeof(text),
             "{\"fw\":\"%s\",\"nowUs\":%u,\"pollCore\":%d,\"interruptCore\":%d,\"webCore\":%d,\"network\":\"%s\","
             "\"polls\":%u,\"maxPollGapMs\":%u",
             FIRMWARE_VERSION, (unsigned)micros(), diag.pollCore, diag.interruptCore, xPortGetCoreID(),
             networkModeName(networkInfo().mode), diag.polls, diag.maxPollGapMs);
    json += text;
    appendMask(json, "pressed", diag.pressedMask);
    appendMask(json, "ignoredAtBoot", diag.ignoredAtBoot);
    appendMask(json, "edgeCountingOff", diag.stormMask);
    json += ",\"buttons\":{";
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        if (i > 0) json += ',';
        json += '"';
        json += BUTTON_NAMES[i];
        json += "\":";
        appendCounters(json, diag.buttons[i]);
    }
    json += "},\"edges\":[";
    const uint32_t edgeStart = diag.edgeTotal > BUTTON_EDGE_LOG ? diag.edgeTotal - BUTTON_EDGE_LOG : 0;
    for (uint32_t n = edgeStart; n < diag.edgeTotal; ++n) {
        const EdgeRecord& edge = diag.edges[n % BUTTON_EDGE_LOG];
        snprintf(text, sizeof(text), "%s[%u,\"%s\",%d]", n > edgeStart ? "," : "", edge.us, BUTTON_NAMES[edge.button], edge.pressed);
        json += text;
    }
    json += "],\"events\":[";
    const uint32_t eventStart = diag.eventTotal > BUTTON_EVENT_LOG ? diag.eventTotal - BUTTON_EVENT_LOG : 0;
    for (uint32_t n = eventStart; n < diag.eventTotal; ++n) {
        const EventRecord& event = diag.events[n % BUTTON_EVENT_LOG];
        snprintf(text, sizeof(text), "%s[%u,\"%s\",\"%s\"]", n > eventStart ? "," : "", event.us, BUTTON_NAMES[event.button],
                 KINDS[event.kind & 3]);
        json += text;
    }
    json += "]}";
    sendJson(200, json.c_str());
}

static void handleDiagnosticsReset() {
    buttonsResetDiagnostics();
    sendJson(200, "{\"reset\":true}");
}

static void handlePage() { g_server.send_P(200, "text/html", WEB_PAGE); }

static void webTask(void*) {
    networkStart();
    g_server.on("/", HTTP_GET, handlePage);
    g_server.on("/api/status", HTTP_GET, handleStatus);
    g_server.on("/api/mode", HTTP_POST, handleMode);
    g_server.on("/api/color", HTTP_POST, handleColor);
    g_server.on("/api/brightness", HTTP_POST, handleBrightness);
    g_server.on("/api/effects", HTTP_GET, handleEffects);
    g_server.on("/api/effect", HTTP_POST, handleEffect);
    g_server.on("/api/effect/color", HTTP_POST, handleEffectColor);
    g_server.on("/api/network", HTTP_POST, handleNetwork);
    g_server.on("/api/settings", HTTP_POST, handleSettings);
    g_server.on("/api/diag", HTTP_GET, handleDiagnostics);
    g_server.on("/api/diag/reset", HTTP_POST, handleDiagnosticsReset);
    g_server.onNotFound([] { sendError(404, "not found"); });
    g_server.begin();
    for (;;) {
        g_server.handleClient();
        // Reopen the listening socket after the radio was torn down rather than rely on it surviving.
        if (networkLoop()) g_server.begin();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void webBegin() {
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH);
    xTaskCreatePinnedToCore(webTask, "web", WEB_TASK_STACK_BYTES, nullptr, 1, nullptr, WEB_TASK_CORE);
}

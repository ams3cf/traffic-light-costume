#include "web.h"

#include <WebServer.h>

#include "config.h"
#include "control.h"
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
    char json[224];
    snprintf(json, sizeof(json),
             "{\"mode\":\"%s\",\"lit\":\"%s\",\"color\":\"%s\",\"effect\":\"%s\",\"cycle\":%s,\"brightness\":%u,"
             "\"trafficSpeed\":%u,\"effectSpeed\":%u,\"network\":\"%s\",\"fw\":\"%s\"}",
             modeName(status.state.mode), colorName(status.lit), colorName(status.state.manualColor),
             status.effectName != nullptr ? status.effectName : "", status.state.cycleEffects ? "true" : "false",
             status.state.brightnessPercent, speedPercent(status.state.trafficSpeed),
             speedPercent(status.state.effectSpeed), networkModeName(networkInfo().mode), FIRMWARE_VERSION);
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

static void handleEffect() {
    const String name = g_server.arg("effect");
    if (name == "cycle") return queueCommand({CommandType::SetEffect, EFFECT_CYCLE});
    for (uint8_t i = 0; i < EFFECT_COUNT; ++i) {
        if (name == EFFECTS[i].name) return queueCommand({CommandType::SetEffect, i});
    }
    sendError(400, "effect must be cycle or a name from /api/effects");
}

static void handleMode() {
    Mode mode;
    if (!parseMode(g_server.arg("mode").c_str(), mode)) return sendError(400, "mode must be auto, manual, or effects");
    queueCommand({CommandType::SetMode, (uint8_t)mode});
}

static void handleColor() {
    Color color;
    if (!parseColor(g_server.arg("color").c_str(), color)) return sendError(400, "color must be red, yellow, or green");
    queueCommand({CommandType::SelectColor, (uint8_t)color});
}

static void handleBrightness() {
    uint8_t percent;
    if (!parseBrightness(g_server.arg("value").c_str(), percent)) return sendError(400, "value must be an integer 0-100");
    queueCommand({CommandType::SetBrightness, percent});
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
    g_server.onNotFound([] { sendError(404, "not found"); });
    g_server.begin();
    for (;;) {
        g_server.handleClient();
        networkLoop();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void webBegin() {
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH);
    xTaskCreatePinnedToCore(webTask, "web", WEB_TASK_STACK_BYTES, nullptr, 1, nullptr, WEB_TASK_CORE);
}

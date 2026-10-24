#include "web.h"

#include <WebServer.h>
#include <WiFi.h>
#include <esp_netif.h>

#include "config.h"
#include "control.h"
#include "effects.h"
#include "web_page.h"

static WebServer g_server(80);

// Without a router option in DHCP, phones treat the costume network as
// local-only and keep using cellular data for the internet.
static void disableDhcpRouterOffer() {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif == nullptr) {
        log_e("AP netif not found; phones may route internet traffic to the costume");
        return;
    }
    dhcps_offer_t offerRouter = 0;
    esp_netif_dhcps_stop(netif);
    esp_err_t err = esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_ROUTER_SOLICITATION_ADDRESS, &offerRouter, sizeof(offerRouter));
    if (err != ESP_OK) log_e("Disabling DHCP router offer failed: 0x%x", err);
    err = esp_netif_dhcps_start(netif);
    if (err != ESP_OK) log_e("Restarting DHCP server failed: 0x%x", err);
}

static void startAccessPoint() {
    WiFi.mode(WIFI_AP);
    const IPAddress address(192, 168, 4, 1);
    WiFi.softAPConfig(address, address, IPAddress(255, 255, 255, 0));
    const bool started = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CLIENTS);
    disableDhcpRouterOffer();
#if SERIAL_DEBUG
    Serial.printf("Wi-Fi AP %s: \"%s\" at http://%s\n", started ? "started" : "FAILED", WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
#endif
}

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
    char json[160];
    snprintf(json, sizeof(json), "{\"mode\":\"%s\",\"lit\":\"%s\",\"color\":\"%s\",\"effect\":\"%s\",\"cycle\":%s,\"brightness\":%u,\"fw\":\"%s\"}",
             modeName(status.state.mode), colorName(status.lit), colorName(status.state.manualColor),
             status.effectName != nullptr ? status.effectName : "", status.state.cycleEffects ? "true" : "false",
             status.state.brightnessPercent, FIRMWARE_VERSION);
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

// Slow blink = waiting for a phone, solid = at least one phone joined.
static void updateStatusLed() {
    const bool on = WiFi.softAPgetStationNum() > 0 || (millis() / 500) % 2 == 0;
    digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW);
}

static void webTask(void*) {
    startAccessPoint();
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
        updateStatusLed();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void webBegin() {
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH);
    xTaskCreatePinnedToCore(webTask, "web", WEB_TASK_STACK_BYTES, nullptr, 1, nullptr, WEB_TASK_CORE);
}

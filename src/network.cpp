#include "network.h"

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_netif.h>

#include "config.h"
#include "settings.h"

static NetworkInfo g_info = {NetworkMode::Hotspot, false, false, false, -1, 0, "", "", ""};
static portMUX_TYPE g_infoMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool g_toggleRequested = false;
static uint32_t g_noticeUntilMs = 0;

const char* networkModeName(NetworkMode mode) { return mode == NetworkMode::Station ? "station" : "hotspot"; }

NetworkInfo networkInfo() {
    portENTER_CRITICAL(&g_infoMux);
    const NetworkInfo info = g_info;
    portEXIT_CRITICAL(&g_infoMux);
    return info;
}

void networkRequestToggle() { g_toggleRequested = true; }

static void copyText(char* out, size_t size, const char* text) {
    strncpy(out, text, size - 1);
    out[size - 1] = '\0';
}

// Notices persist until replaced when durationMs is 0.
static void setNotice(const char* text, uint32_t durationMs) {
    portENTER_CRITICAL(&g_infoMux);
    copyText(g_info.notice, sizeof(g_info.notice), text);
    portEXIT_CRITICAL(&g_infoMux);
    g_noticeUntilMs = durationMs == 0 ? 0 : millis() + durationMs;
#if SERIAL_DEBUG
    if (text[0] != '\0') Serial.printf("network: %s\n", text);
#endif
}

static void setOtaPercent(int8_t percent) {
    portENTER_CRITICAL(&g_infoMux);
    g_info.otaPercent = percent;
    portEXIT_CRITICAL(&g_infoMux);
}

static bool homeWifiConfigured() { return HOME_WIFI_SSID[0] != '\0'; }

static NetworkMode loadPreference() {
    const uint8_t value = settingsGet(SETTING_NETWORK, (uint8_t)NetworkMode::Hotspot);
    return value == (uint8_t)NetworkMode::Station ? NetworkMode::Station : NetworkMode::Hotspot;
}

static bool savePreference(NetworkMode mode) { return settingsSet(SETTING_NETWORK, (uint8_t)mode); }

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

static void startHotspot() {
    WiFi.mode(WIFI_AP);
    const IPAddress address(192, 168, 4, 1);
    WiFi.softAPConfig(address, address, IPAddress(255, 255, 255, 0));
    const bool started = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CLIENTS);
    disableDhcpRouterOffer();
    portENTER_CRITICAL(&g_infoMux);
    g_info.mode = NetworkMode::Hotspot;
    g_info.failed = !started;
    copyText(g_info.ssid, sizeof(g_info.ssid), WIFI_AP_SSID);
    copyText(g_info.address, sizeof(g_info.address), "192.168.4.1");
    portEXIT_CRITICAL(&g_infoMux);
#if SERIAL_DEBUG
    Serial.printf("Wi-Fi AP %s: \"%s\" at http://192.168.4.1\n", started ? "started" : "FAILED", WIFI_AP_SSID);
#endif
}

// No password: OTA is only reachable on the home network, and only in home Wi-Fi mode.
static void startOta() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.onStart([] { setOtaPercent(0); });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) { setOtaPercent((int8_t)(total == 0 ? 0 : done * 100 / total)); });
    ArduinoOTA.onEnd([] { setOtaPercent(100); });
    // The core reports a failed connect, then carries on and also reports END; keep only
    // the first error of an update, since it names the real cause.
    ArduinoOTA.onError([](ota_error_t error) {
        static const char* const NAMES[] = {"auth", "begin", "can't reach PC", "receive", "end"};
        if (networkInfo().otaPercent < 0) return;
        setOtaPercent(-1);
        char text[26];
        snprintf(text, sizeof(text), "OTA failed: %s", (unsigned)error < 5 ? NAMES[error] : "unknown");
        setNotice(text, 10000);
    });
    ArduinoOTA.begin();  // Also starts mDNS as OTA_HOSTNAME.local.
    portENTER_CRITICAL(&g_infoMux);
    g_info.otaEnabled = true;
    portEXIT_CRITICAL(&g_infoMux);
}

static bool startStation() {
    portENTER_CRITICAL(&g_infoMux);
    g_info.mode = NetworkMode::Station;
    g_info.connecting = true;
    copyText(g_info.ssid, sizeof(g_info.ssid), HOME_WIFI_SSID);
    portEXIT_CRITICAL(&g_infoMux);
    setNotice("joining home Wi-Fi...", 0);
    WiFi.mode(WIFI_STA);
    // Modem sleep drops the ESP32 to ~30-60 mA average, low enough that USB battery
    // banks cut the port after a minute or two; it also adds 1-2 s of request latency.
    WiFi.setSleep(false);
    WiFi.setHostname(OTA_HOSTNAME);
    WiFi.begin(HOME_WIFI_SSID, HOME_WIFI_PASSWORD);
    const uint32_t startedMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedMs < STATION_CONNECT_TIMEOUT_MS) vTaskDelay(pdMS_TO_TICKS(100));
    const bool connected = WiFi.status() == WL_CONNECTED;
    portENTER_CRITICAL(&g_infoMux);
    g_info.connecting = false;
    portEXIT_CRITICAL(&g_infoMux);
    if (!connected) {
        WiFi.disconnect(true);
        return false;
    }
    portENTER_CRITICAL(&g_infoMux);
    copyText(g_info.address, sizeof(g_info.address), WiFi.localIP().toString().c_str());
    portEXIT_CRITICAL(&g_infoMux);
    setNotice("", 0);
#if SERIAL_DEBUG
    Serial.printf("Wi-Fi joined \"%s\" as %s (http://%s.local)\n", HOME_WIFI_SSID, WiFi.localIP().toString().c_str(), OTA_HOSTNAME);
#endif
    startOta();
    MDNS.addService("http", "tcp", 80);
    return true;
}

void networkStart() {
    if (loadPreference() == NetworkMode::Station) {
        if (!homeWifiConfigured()) {
            startHotspot();
            setNotice("no HOME_WIFI in secrets.h", 10000);
        } else if (!startStation()) {
            startHotspot();
            setNotice("home Wi-Fi not found", 10000);
        }
        return;
    }
    startHotspot();
}

// Saves the other mode and restarts; the pause lets the OLED show where it is going.
static void toggleMode() {
    const NetworkMode next = networkInfo().mode == NetworkMode::Station ? NetworkMode::Hotspot : NetworkMode::Station;
    if (next == NetworkMode::Station && !homeWifiConfigured()) {
        setNotice("no HOME_WIFI in secrets.h", NETWORK_NOTICE_MS);
        return;
    }
    if (!savePreference(next)) {
        setNotice("could not save mode", NETWORK_NOTICE_MS);
        return;
    }
    setNotice(next == NetworkMode::Station ? "restart: home Wi-Fi" : "restart: hotspot", 0);
    vTaskDelay(pdMS_TO_TICKS(1500));
    ESP.restart();
}

// Hotspot: slow blink while waiting for a phone, solid once one joins.
// Station: solid while connected, fast blink while not.
static void updateStatusLed(const NetworkInfo& info) {
    const uint32_t now = millis();
    bool on;
    if (info.mode == NetworkMode::Station) on = WiFi.status() == WL_CONNECTED || (now / 100) % 2 == 0;
    else on = info.clients > 0 || (now / 500) % 2 == 0;
    digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW);
}

void networkLoop() {
    const NetworkInfo info = networkInfo();
    if (info.otaEnabled) ArduinoOTA.handle();
    if (info.mode == NetworkMode::Hotspot) {
        const uint8_t clients = WiFi.softAPgetStationNum();
        portENTER_CRITICAL(&g_infoMux);
        g_info.clients = clients;
        portEXIT_CRITICAL(&g_infoMux);
    }
    if (g_noticeUntilMs != 0 && (int32_t)(millis() - g_noticeUntilMs) >= 0) setNotice("", 0);
    if (g_toggleRequested) {
        g_toggleRequested = false;
        toggleMode();
    }
    updateStatusLed(info);
}

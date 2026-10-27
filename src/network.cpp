#include "network.h"

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_netif.h>

#include "config.h"
#include "settings.h"

static NetworkInfo g_info = {NetworkMode::Off, false, false, false, -1, 0, "", "", ""};
static portMUX_TYPE g_infoMux = portMUX_INITIALIZER_UNLOCKED;
static constexpr int8_t NO_REQUEST = -1;
static volatile int8_t g_requested = NO_REQUEST;
// Saved choice for when Wi-Fi is on; what an off -> on switch returns to.
static volatile NetworkMode g_preferred = NetworkMode::Hotspot;
static uint32_t g_noticeUntilMs = 0;
static bool g_otaCallbacksSet = false;

static const char* const MODE_NAMES[] = {"hotspot", "station", "off"};

const char* networkModeName(NetworkMode mode) { return MODE_NAMES[(uint8_t)mode]; }

bool parseNetworkMode(const char* text, NetworkMode& mode) {
    for (uint8_t i = 0; i < sizeof(MODE_NAMES) / sizeof(MODE_NAMES[0]); ++i) {
        if (strcmp(text, MODE_NAMES[i]) == 0) { mode = (NetworkMode)i; return true; }
    }
    return false;
}

NetworkInfo networkInfo() {
    portENTER_CRITICAL(&g_infoMux);
    const NetworkInfo info = g_info;
    portEXIT_CRITICAL(&g_infoMux);
    return info;
}

void networkRequest(NetworkMode mode) { g_requested = (int8_t)mode; }

NetworkMode networkToggleTarget() {
    const NetworkMode mode = networkInfo().mode;
    if (mode == NetworkMode::Off) return g_preferred;
    return mode == NetworkMode::Station ? NetworkMode::Hotspot : NetworkMode::Station;
}

void networkRequestToggle() { networkRequest(networkToggleTarget()); }

void networkRequestOnOff() { networkRequest(networkInfo().mode == NetworkMode::Off ? g_preferred : NetworkMode::Off); }

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

bool networkHomeConfigured() { return HOME_WIFI_SSID[0] != '\0'; }

static NetworkMode loadPreference() {
    const uint8_t value = settingsGet(SETTING_NETWORK, (uint8_t)NetworkMode::Hotspot);
    return value == (uint8_t)NetworkMode::Station ? NetworkMode::Station : NetworkMode::Hotspot;
}

static bool savePreference(NetworkMode mode) {
    if (mode == NetworkMode::Off) return settingsSet(SETTING_WIFI_ON, 0);
    g_preferred = mode;
    return settingsSet(SETTING_WIFI_ON, 1) && settingsSet(SETTING_NETWORK, (uint8_t)mode);
}

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
    if (g_otaCallbacksSet) {
        ArduinoOTA.begin();
        portENTER_CRITICAL(&g_infoMux);
        g_info.otaEnabled = true;
        portEXIT_CRITICAL(&g_infoMux);
        return;
    }
    g_otaCallbacksSet = true;
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
    WiFi.setHostname(OTA_HOSTNAME);
    WiFi.mode(WIFI_STA);
    // Modem sleep drops the ESP32 to ~30-60 mA average, low enough that USB battery
    // banks cut the port after a minute or two; it also adds 1-2 s of request latency.
    WiFi.setSleep(false);
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

// Tears down whatever is running: OTA (and with it mDNS), then the radio.
static void stopNetwork() {
    if (networkInfo().otaEnabled) ArduinoOTA.end();
    WiFi.mode(WIFI_OFF);
    portENTER_CRITICAL(&g_infoMux);
    g_info.mode = NetworkMode::Off;
    g_info.otaEnabled = false;
    g_info.failed = false;
    g_info.clients = 0;
    g_info.ssid[0] = '\0';
    g_info.address[0] = '\0';
    portEXIT_CRITICAL(&g_infoMux);
}

static void applyMode(NetworkMode mode) {
    stopNetwork();
    if (mode == NetworkMode::Off) {
        setNotice("", 0);
#if SERIAL_DEBUG
        Serial.println("Wi-Fi off");
#endif
    } else if (mode == NetworkMode::Hotspot) {
        startHotspot();
        setNotice("", 0);
    } else if (!networkHomeConfigured()) {
        startHotspot();
        setNotice("no HOME_WIFI in secrets.h", 10000);
    } else if (!startStation()) {
        startHotspot();
        setNotice("home Wi-Fi not found", 10000);
    }
}

void networkStart() {
    g_preferred = loadPreference();
    applyMode(settingsGet(SETTING_WIFI_ON, 1) != 0 ? g_preferred : NetworkMode::Off);
}

// Saves the choice, then switches in place; the pause lets the web reply go out
// and the OLED show where it is going. Returns true if the network changed.
static bool switchTo(NetworkMode target) {
    if (target == NetworkMode::Station && !networkHomeConfigured()) {
        setNotice("no HOME_WIFI in secrets.h", NETWORK_NOTICE_MS);
        return false;
    }
    if (!savePreference(target)) setNotice("could not save Wi-Fi mode", NETWORK_NOTICE_MS);
    if (target == networkInfo().mode) return false;
    static const char* const NOTICES[] = {"starting hotspot...", "joining home Wi-Fi...", "turning Wi-Fi off..."};
    setNotice(NOTICES[(uint8_t)target], 0);
    vTaskDelay(pdMS_TO_TICKS(NETWORK_SWITCH_DELAY_MS));
    applyMode(target);
    return true;
}

// Hotspot: slow blink while waiting for a phone, solid once one joins.
// Station: solid while connected, fast blink while not. Off: dark.
static void updateStatusLed(const NetworkInfo& info) {
    const uint32_t now = millis();
    bool on;
    if (info.mode == NetworkMode::Off) on = false;
    else if (info.mode == NetworkMode::Station) on = WiFi.status() == WL_CONNECTED || (now / 100) % 2 == 0;
    else on = info.clients > 0 || (now / 500) % 2 == 0;
    digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW);
}

bool networkLoop() {
    const NetworkInfo info = networkInfo();
    if (info.otaEnabled) ArduinoOTA.handle();
    if (info.mode == NetworkMode::Hotspot) {
        const uint8_t clients = WiFi.softAPgetStationNum();
        portENTER_CRITICAL(&g_infoMux);
        g_info.clients = clients;
        portEXIT_CRITICAL(&g_infoMux);
    }
    if (g_noticeUntilMs != 0 && (int32_t)(millis() - g_noticeUntilMs) >= 0) setNotice("", 0);
    updateStatusLed(info);
    const int8_t requested = g_requested;
    if (requested == NO_REQUEST) return false;
    g_requested = NO_REQUEST;
    return switchTo((NetworkMode)requested);
}

// ============================================================================
// Network mode: the costume's own hotspot (default), the home Wi-Fi from
// src/secrets.h (which also enables OTA firmware updates), or Wi-Fi off.
//
// Switching takes effect in place, without a restart: from the MODE + GREEN
// chord (hotspot <-> home), the MODE + RED chord (Wi-Fi off <-> on), or the
// web page. The choice is saved in NVS. If home Wi-Fi cannot be joined within
// STATION_CONNECT_TIMEOUT_MS, the hotspot starts instead; the saved choice is
// kept, so the next boot tries home Wi-Fi again.
//
// Runs in the web task (core 0). networkInfo() and the networkRequest*()
// functions are safe from any task.
// ============================================================================
#pragma once

#include <stdint.h>

enum class NetworkMode : uint8_t { Hotspot, Station, Off };

struct NetworkInfo {
    NetworkMode mode;    // What is actually running (Hotspot after a fallback).
    bool connecting;     // Still trying to join home Wi-Fi.
    bool failed;         // The hotspot failed to start.
    bool otaEnabled;
    int8_t otaPercent;   // -1 when no update is in progress.
    uint8_t clients;     // Phones joined to the hotspot.
    char ssid[33];
    char address[16];
    char notice[26];     // Short message for the OLED; empty when none.
};

const char* networkModeName(NetworkMode mode);
bool parseNetworkMode(const char* text, NetworkMode& mode);
bool networkHomeConfigured();

void networkStart();  // Blocks up to STATION_CONNECT_TIMEOUT_MS in station mode.
// Returns true when the network was switched, so the caller can restart its server.
bool networkLoop();
void networkRequest(NetworkMode mode);
void networkRequestToggle();  // Hotspot <-> home Wi-Fi; from off, back on.
void networkRequestOnOff();   // Off <-> the saved hotspot / home choice.
// Where networkRequestToggle() would go, for the OLED hint.
NetworkMode networkToggleTarget();
NetworkInfo networkInfo();

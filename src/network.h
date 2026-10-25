// ============================================================================
// Network mode: the costume's own hotspot (default) or the home Wi-Fi from
// src/secrets.h, which also enables OTA firmware updates.
//
// The chosen mode is saved in NVS. Toggling (MODE + GREEN chord) saves the
// other mode and restarts. If home Wi-Fi cannot be joined within
// STATION_CONNECT_TIMEOUT_MS, the hotspot starts for that boot instead; the
// saved preference is kept, so the next boot tries home Wi-Fi again.
//
// Runs in the web task (core 0). networkInfo() and networkRequestToggle()
// are safe from any task.
// ============================================================================
#pragma once

#include <stdint.h>

enum class NetworkMode : uint8_t { Hotspot, Station };

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

void networkStart();  // Blocks up to STATION_CONNECT_TIMEOUT_MS in station mode.
void networkLoop();
void networkRequestToggle();
NetworkInfo networkInfo();

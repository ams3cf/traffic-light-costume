// ============================================================================
// Physical buttons: normally-closed to a common ground, internal pull-ups
// (see BUTTON_PRESSED_LEVEL). Gestures (see gestures.h) become ButtonEvent
// commands for the lights core; what they do depends on the mode and is
// decided in state.cpp. MODE + GREEN held together switches hotspot / home
// Wi-Fi; MODE + RED turns Wi-Fi off or back on.
//
// Gestures come from sampling the pins every frame (core 1). For diagnosis,
// a pin-change interrupt (also core 1) additionally counts and logs every raw
// edge, so bounce and electrical noise show up even when they are too short
// for the frame-rate sampling to see. Read it at GET /api/diag.
// ============================================================================
#pragma once

#include <stdint.h>

#include "gestures.h"

static constexpr uint8_t BUTTON_EDGE_LOG = 96;
static constexpr uint8_t BUTTON_EVENT_LOG = 32;
static constexpr uint8_t BUTTON_EVENT_CHORD = 3;  // EventRecord::kind for a chord; otherwise a Gesture.

extern const char* const BUTTON_NAMES[BUTTON_COUNT];

struct ButtonCounters {
    uint32_t edges;           // Raw level changes seen by the interrupt.
    uint32_t shortestEdgeUs;  // Shortest gap between two edges; 0 until there are two.
    uint32_t sampledChanges;  // Level changes seen by the per-frame sampling, before debouncing.
    uint32_t presses;
    uint32_t taps;
    uint32_t holds;
    uint32_t chords;
};

struct EdgeRecord {
    uint32_t us;
    uint8_t button;
    bool pressed;
};

struct EventRecord {
    uint32_t us;
    uint8_t button;
    uint8_t kind;
};

// The logs are rings: entry n lives at [n % size]; edgeTotal / eventTotal count all ever written.
struct ButtonDiagnostics {
    ButtonCounters buttons[BUTTON_COUNT];
    uint8_t pressedMask;     // As last sampled.
    uint8_t ignoredAtBoot;   // Read pressed at boot (ignored until released).
    uint8_t stormMask;       // Interrupt switched off: too many edges per frame (noise).
    int8_t pollCore;
    int8_t interruptCore;
    uint32_t polls;
    uint32_t maxPollGapMs;   // Longest time between two samples; > ~30 ms means the lights core stalled.
    uint32_t edgeTotal;
    uint32_t eventTotal;
    EdgeRecord edges[BUTTON_EDGE_LOG];
    EventRecord events[BUTTON_EVENT_LOG];
};

void buttonsBegin();
void buttonsPoll(uint32_t now);
// Safe from any task.
void buttonsDiagnostics(ButtonDiagnostics& out);
void buttonsResetDiagnostics();

#include "buttons.h"

#include <Arduino.h>

#include "config.h"
#include "control.h"
#include "network.h"

// Indexed by ButtonId.
static const uint8_t BUTTON_PINS[BUTTON_COUNT] = {PIN_MODE_BUTTON, PIN_RED_BUTTON, PIN_YELLOW_BUTTON, PIN_GREEN_BUTTON};
const char* const BUTTON_NAMES[BUTTON_COUNT] = {"mode", "red", "yellow", "green"};
static const char* const GESTURE_NAMES[] = {"press", "tap", "hold"};

static ButtonGestures g_gestures({BUTTON_DEBOUNCE_MS, BUTTON_LONG_PRESS_MS, BUTTON_REPEAT_MS, BUTTON_CHORD_MS});

// Written by the interrupt and the poll (both core 1), read and reset from the web core.
static portMUX_TYPE g_diagMux = portMUX_INITIALIZER_UNLOCKED;
static ButtonDiagnostics g_diag = {};
static uint32_t g_lastEdgeUs[BUTTON_COUNT] = {};
static uint32_t g_edgesAtLastPoll[BUTTON_COUNT] = {};
static uint32_t g_lastPollMs = 0;
static uint8_t g_lastSampledMask = 0;
static volatile bool g_reattachRequested = false;

static uint8_t readPressedMask() {
    uint8_t mask = 0;
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        if (digitalRead(BUTTON_PINS[i]) == BUTTON_PRESSED_LEVEL) mask |= (uint8_t)(1 << i);
    }
    return mask;
}

static void ARDUINO_ISR_ATTR onEdge(void* arg) {
    const uint8_t index = (uint8_t)(uintptr_t)arg;
    const uint32_t us = micros();
    const bool pressed = digitalRead(BUTTON_PINS[index]) == BUTTON_PRESSED_LEVEL;
    portENTER_CRITICAL_ISR(&g_diagMux);
    ButtonCounters& counters = g_diag.buttons[index];
    const uint32_t gap = us - g_lastEdgeUs[index];
    if (counters.edges > 0 && (counters.shortestEdgeUs == 0 || gap < counters.shortestEdgeUs)) counters.shortestEdgeUs = gap;
    g_lastEdgeUs[index] = us;
    ++counters.edges;
    g_diag.edges[g_diag.edgeTotal % BUTTON_EDGE_LOG] = {us, index, pressed};
    ++g_diag.edgeTotal;
    g_diag.interruptCore = (int8_t)xPortGetCoreID();
    portEXIT_CRITICAL_ISR(&g_diagMux);
}

// Called on the lights core, so the interrupts are serviced there too.
static void attachEdgeInterrupts() {
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) attachInterruptArg(BUTTON_PINS[i], onEdge, (void*)(uintptr_t)i, CHANGE);
}

void buttonsBegin() {
    for (const uint8_t pin : BUTTON_PINS) pinMode(pin, INPUT_PULLUP);
    delay(5);  // Let the pull-ups charge open (pressed or unplugged) lines before sampling.
    const uint8_t ignored = g_gestures.begin(readPressedMask());
    g_diag.ignoredAtBoot = ignored;
    g_diag.interruptCore = -1;
    attachEdgeInterrupts();
#if SERIAL_DEBUG
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        if ((ignored >> i) & 1) Serial.printf("button %s reads pressed at boot; ignored until released (unplugged?)\n", BUTTON_NAMES[i]);
    }
#endif
}

void buttonsDiagnostics(ButtonDiagnostics& out) {
    portENTER_CRITICAL(&g_diagMux);
    out = g_diag;
    portEXIT_CRITICAL(&g_diagMux);
}

void buttonsResetDiagnostics() {
    portENTER_CRITICAL(&g_diagMux);
    const uint8_t ignored = g_diag.ignoredAtBoot;
    const int8_t interruptCore = g_diag.interruptCore;
    memset(&g_diag, 0, sizeof(g_diag));
    memset(g_edgesAtLastPoll, 0, sizeof(g_edgesAtLastPoll));
    g_diag.ignoredAtBoot = ignored;
    g_diag.interruptCore = interruptCore;
    portEXIT_CRITICAL(&g_diagMux);
    g_reattachRequested = true;
}

// Returns the buttons whose interrupt must be switched off for flooding.
static uint8_t recordPoll(uint8_t pressedMask, uint32_t now, const ButtonEvent* events, uint8_t count) {
    const uint32_t us = micros();
    uint8_t storms = 0;
    portENTER_CRITICAL(&g_diagMux);
    if (g_diag.polls > 0 && now - g_lastPollMs > g_diag.maxPollGapMs) g_diag.maxPollGapMs = now - g_lastPollMs;
    g_lastPollMs = now;
    ++g_diag.polls;
    g_diag.pollCore = (int8_t)xPortGetCoreID();
    g_diag.pressedMask = pressedMask;
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        ButtonCounters& counters = g_diag.buttons[i];
        if (((pressedMask ^ g_lastSampledMask) >> i) & 1) ++counters.sampledChanges;
        if (counters.edges - g_edgesAtLastPoll[i] > BUTTON_EDGE_STORM_PER_FRAME && !((g_diag.stormMask >> i) & 1)) {
            g_diag.stormMask |= (uint8_t)(1 << i);
            storms |= (uint8_t)(1 << i);
        }
        g_edgesAtLastPoll[i] = counters.edges;
    }
    g_lastSampledMask = pressedMask;
    for (uint8_t e = 0; e < count; ++e) {
        const ButtonEvent& event = events[e];
        ButtonCounters& counters = g_diag.buttons[(uint8_t)event.button];
        uint8_t kind = BUTTON_EVENT_CHORD;
        if (event.kind == ButtonEventKind::Chord) ++counters.chords;
        else {
            kind = (uint8_t)event.gesture;
            if (event.gesture == Gesture::Press) ++counters.presses;
            else if (event.gesture == Gesture::Tap) ++counters.taps;
            else ++counters.holds;
        }
        g_diag.events[g_diag.eventTotal % BUTTON_EVENT_LOG] = {us, (uint8_t)event.button, kind};
        ++g_diag.eventTotal;
    }
    portEXIT_CRITICAL(&g_diagMux);
    return storms;
}

static void handleChord(ButtonId button) {
    if (button == ButtonId::Green) networkRequestToggle();
    else if (button == ButtonId::Red) networkRequestOnOff();
}

void buttonsPoll(uint32_t now) {
    if (g_reattachRequested) {
        g_reattachRequested = false;
        attachEdgeInterrupts();
    }
    ButtonEvent events[BUTTON_COUNT * 2 + 1];
    const uint8_t pressedMask = readPressedMask();
    const uint8_t count = g_gestures.update(pressedMask, now, events, sizeof(events) / sizeof(events[0]));
    const uint8_t storms = recordPoll(pressedMask, now, events, count);
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        if (!((storms >> i) & 1)) continue;
        detachInterrupt(BUTTON_PINS[i]);
        log_e("button %s: over %u edges in one frame; edge counting off until diagnostics reset", BUTTON_NAMES[i],
              BUTTON_EDGE_STORM_PER_FRAME);
    }
    for (uint8_t i = 0; i < count; ++i) {
        const ButtonEvent& event = events[i];
#if SERIAL_DEBUG
        Serial.printf("button %s %s\n", BUTTON_NAMES[(uint8_t)event.button],
                      event.kind == ButtonEventKind::Chord ? "chord with mode" : GESTURE_NAMES[(uint8_t)event.gesture]);
#endif
        if (event.kind == ButtonEventKind::Chord) handleChord(event.button);
        else controlSend(buttonCommand(event.button, event.gesture));
    }
}

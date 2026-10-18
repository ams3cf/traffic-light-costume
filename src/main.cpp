// ============================================================================
// Traffic Light Costume — Firmware
//
// Drives three independent WS2812B LED rings (red, yellow, green) on an ESP32
// using FastLED. Implements a non-blocking traffic-signal sequence plus a
// startup self-test. All wiring/behavior knobs live in config.h.
//
// NOTE: nothing here has been validated against physical hardware. It
// compiles cleanly; treat behavior as UNTESTED until flashed and confirmed.
// ============================================================================

#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

// ---------------------------------------------------------------------------
// LED buffers — one CRGB (3 bytes) per LED per ring.
// ---------------------------------------------------------------------------
CRGB g_redRing[LED_COUNT_RED];
CRGB g_yellowRing[LED_COUNT_YELLOW];
CRGB g_greenRing[LED_COUNT_GREEN];

// ---------------------------------------------------------------------------
// Traffic sequence state
// ---------------------------------------------------------------------------
enum SignalPhase {
    PHASE_GREEN,
    PHASE_YELLOW,
    PHASE_RED,
};

// Millisecond durations for each phase (indexed by phase value).
static const uint32_t PHASE_DURATION_MS[] = {
    TIMING_GREEN_MS,   // PHASE_GREEN
    TIMING_YELLOW_MS,  // PHASE_YELLOW
    TIMING_RED_MS,     // PHASE_RED
};

SignalPhase g_phase        = PHASE_GREEN;
uint32_t    g_phaseStartMs = 0;
bool        g_selfTestDone = false;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void allOff();
static void setRing(CRGB* ring, uint16_t count, const CRGB& color);
static void beginTrafficSequence();

// ---------------------------------------------------------------------------
// Self-test: light each ring in its own color, one at a time, then go dark.
// Non-blocking (millis() state machine). Lights red -> yellow -> green so a
// mis-wired ring is immediately obvious, then hands off to the sequence.
// ---------------------------------------------------------------------------
static void runSelfTest() {
    static uint8_t  step       = 0;  // 0=red, 1=yellow, 2=green, 3=done
    static uint32_t stepStartMs = 0;

    switch (step) {
        case 0:
            setRing(g_redRing, LED_COUNT_RED, CRGB::Red);
            if (millis() - stepStartMs >= SELFTEST_STEP_MS) { step = 1; stepStartMs = millis(); }
            break;
        case 1:
            setRing(g_yellowRing, LED_COUNT_YELLOW, CRGB::Yellow);
            if (millis() - stepStartMs >= SELFTEST_STEP_MS) { step = 2; stepStartMs = millis(); }
            break;
        case 2:
            setRing(g_greenRing, LED_COUNT_GREEN, CRGB::Green);
            if (millis() - stepStartMs >= SELFTEST_STEP_MS) { step = 3; stepStartMs = millis(); }
            break;
        case 3:
        default:
            g_selfTestDone = true;
            beginTrafficSequence();
            break;
    }
}

// ---------------------------------------------------------------------------
// Traffic sequence: a simplified US-style cycle.
//   GREEN (go) -> YELLOW (caution) -> RED (stop) -> GREEN ...
// Non-blocking via millis(); only one ring is lit at a time.
// ---------------------------------------------------------------------------
static void runTrafficSequence() {
    uint32_t elapsed = millis() - g_phaseStartMs;

    if (elapsed >= PHASE_DURATION_MS[g_phase]) {
        g_phase = (SignalPhase)((g_phase + 1) % 3);  // wrap GREEN->YELLOW->RED
        g_phaseStartMs = millis();
    }

    switch (g_phase) {
        case PHASE_GREEN:  setRing(g_greenRing,  LED_COUNT_GREEN,  CRGB::Green);  break;
        case PHASE_YELLOW: setRing(g_yellowRing, LED_COUNT_YELLOW, CRGB::Yellow); break;
        case PHASE_RED:    setRing(g_redRing,    LED_COUNT_RED,    CRGB::Red);    break;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Turn every ring off (single FastLED.show()).
static void allOff() {
    fill_solid(g_redRing,    LED_COUNT_RED,    CRGB::Black);
    fill_solid(g_yellowRing, LED_COUNT_YELLOW, CRGB::Black);
    fill_solid(g_greenRing,  LED_COUNT_GREEN,  CRGB::Black);
    FastLED.show();
}

// Light one ring a solid color and clear the other two, then push to hardware.
static void setRing(CRGB* ring, uint16_t count, const CRGB& color) {
    fill_solid(g_redRing,    LED_COUNT_RED,    CRGB::Black);
    fill_solid(g_yellowRing, LED_COUNT_YELLOW, CRGB::Black);
    fill_solid(g_greenRing,  LED_COUNT_GREEN,  CRGB::Black);
    fill_solid(ring, count, color);
    FastLED.show();
}

// Reset the sequence state machine and show the starting (green) phase.
static void beginTrafficSequence() {
    g_phase        = PHASE_GREEN;
    g_phaseStartMs = millis();
    setRing(g_greenRing, LED_COUNT_GREEN, CRGB::Green);
#if SERIAL_DEBUG
    Serial.println("Self-test complete. Running traffic sequence.");
#endif
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
#if SERIAL_DEBUG
    Serial.begin(SERIAL_BAUD);
    Serial.println();
    Serial.println("Traffic Light Costume - booting");
    Serial.printf("Rings: R=%d Y=%d G=%d  pins: R=%d Y=%d G=%d  brightness=%d\n",
                  LED_COUNT_RED, LED_COUNT_YELLOW, LED_COUNT_GREEN,
                  PIN_RED, PIN_YELLOW, PIN_GREEN, BRIGHTNESS_MASTER);
#endif

    // Attach each ring to its own FastLED controller. WS2812B uses GRB order.
    FastLED.addLeds<WS2812B, PIN_RED,    GRB>(g_redRing,    LED_COUNT_RED);
    FastLED.addLeds<WS2812B, PIN_YELLOW, GRB>(g_yellowRing, LED_COUNT_YELLOW);
    FastLED.addLeds<WS2812B, PIN_GREEN,  GRB>(g_greenRing,  LED_COUNT_GREEN);

    // Master brightness cap — applied to every pixel on every show().
    FastLED.setBrightness(BRIGHTNESS_MASTER);

    // Begin dark, then run the self-test.
    allOff();
}

void loop() {
    if (!g_selfTestDone) {
        runSelfTest();
    } else {
        runTrafficSequence();
    }
}

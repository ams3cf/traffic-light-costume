#include "lights.h"

#include <FastLED.h>

#include "buttons.h"
#include "config.h"
#include "control.h"
#include "effects.h"
#include "settings.h"

// Traffic phases run bottom to top: green, yellow, red.
static const Color PHASE_COLORS[] = {Color::Green, Color::Yellow, Color::Red};
static const uint32_t PHASE_DURATION_MS[] = {TIMING_GREEN_MS, TIMING_YELLOW_MS, TIMING_RED_MS};

static CRGB g_leds[LED_COUNT_TOTAL];
static LightState g_state = {Mode::Auto, Color::Red, 0, true, DEFAULT_BRIGHTNESS_PERCENT, SPEED_NORMAL, SPEED_NORMAL};
static uint8_t g_trafficPhase = 0;
static uint32_t g_phaseStartedMs = 0;
static uint32_t g_effectStartedMs = 0;
// Effects run on their own clock, advanced in fixed frames at effectSpeed, so both
// time-based and frame-based (fire, fades) effects speed up and slow down.
static constexpr uint32_t EFFECT_FRAME_BUDGET = LED_FRAME_INTERVAL_MS * 100;
static constexpr uint8_t EFFECT_MAX_FRAMES_PER_SHOW = 4;
static uint32_t g_effectClockMs = 0;
static uint32_t g_effectBudget = 0;  // Real ms x speed percent not yet rendered.
static uint32_t g_lastEffectMs = 0;
static uint32_t g_lastFrameMs = 0;
static bool g_selfTestDone = false;

static uint16_t sectionOffset(Color color) {
    switch (color) {
        case Color::Green: return LED_OFFSET_BOTTOM;
        case Color::Yellow: return LED_OFFSET_MIDDLE;
        default: return LED_OFFSET_TOP;
    }
}

static CRGB sectionColor(Color color) {
    switch (color) {
        case Color::Green: return CRGB::Green;
        case Color::Yellow: return CRGB::Yellow;
        default: return CRGB::Red;
    }
}

static void lightChamber(Color color) {
    fill_solid(g_leds, LED_COUNT_TOTAL, CRGB::Black);
    if (color != Color::None) fill_solid(g_leds + sectionOffset(color), LED_COUNT_PER_SECTION, sectionColor(color));
}

static void restartTimers(uint32_t now) {
    g_trafficPhase = 0;
    g_phaseStartedMs = now;
    g_effectStartedMs = now;
    g_lastEffectMs = now;
    g_effectBudget = EFFECT_FRAME_BUDGET;  // Render the first effect frame immediately.
    fill_solid(g_leds, LED_COUNT_TOTAL, CRGB::Black);
}

static void handleCommand(const Command& command, uint32_t now) {
    const LightState before = g_state;
    if (!g_selfTestDone) {
        g_selfTestDone = true;
        restartTimers(now);
    }
    if (!applyCommand(g_state, command, EFFECT_COUNT)) return;
    // Saved immediately so a low "safe current" brightness also applies to the next boot's
    // self-test. Only the web slider changes brightness, once per release, so writes are rare.
    if (g_state.brightnessPercent != before.brightnessPercent) settingsSet(SETTING_BRIGHTNESS, g_state.brightnessPercent);
    if (g_state.mode != before.mode || g_state.effectIndex != before.effectIndex ||
        g_state.cycleEffects != before.cycleEffects) {
        restartTimers(now);
    }
#if SERIAL_DEBUG
    Serial.printf("state mode=%s color=%s effect=%s cycle=%d brightness=%u trafficSpeed=%u%% effectSpeed=%u%%\n",
                  modeName(g_state.mode), colorName(g_state.manualColor), EFFECTS[g_state.effectIndex].name,
                  g_state.cycleEffects, g_state.brightnessPercent, speedPercent(g_state.trafficSpeed),
                  speedPercent(g_state.effectSpeed));
#endif
}

static Color renderTraffic(uint32_t now) {
    const uint32_t duration = PHASE_DURATION_MS[g_trafficPhase] * 100 / speedPercent(g_state.trafficSpeed);
    if (now - g_phaseStartedMs >= duration) {
        g_phaseStartedMs = now;
        g_trafficPhase = (uint8_t)((g_trafficPhase + 1) % 3);
    }
    lightChamber(PHASE_COLORS[g_trafficPhase]);
    return PHASE_COLORS[g_trafficPhase];
}

static void renderEffects(uint32_t now) {
    if (g_state.cycleEffects && now - g_effectStartedMs >= EFFECT_STEP_MS) {
        g_state.effectIndex = (uint8_t)((g_state.effectIndex + 1) % EFFECT_COUNT);
        restartTimers(now);
    }
    g_effectBudget += (now - g_lastEffectMs) * speedPercent(g_state.effectSpeed);
    g_lastEffectMs = now;
    for (uint8_t frames = 0; g_effectBudget >= EFFECT_FRAME_BUDGET && frames < EFFECT_MAX_FRAMES_PER_SHOW; ++frames) {
        g_effectClockMs += LED_FRAME_INTERVAL_MS;
        g_effectBudget -= EFFECT_FRAME_BUDGET;
        EFFECTS[g_state.effectIndex].render(g_leds, g_effectClockMs);
    }
    // Drop any backlog (e.g. after a stall) rather than fast-forwarding through it.
    if (g_effectBudget > EFFECT_FRAME_BUDGET) g_effectBudget = EFFECT_FRAME_BUDGET;
}

// Returns the chamber that is lit, for the status API.
static Color render(uint32_t now) {
    switch (g_state.mode) {
        case Mode::Auto: return renderTraffic(now);
        case Mode::Manual: lightChamber(g_state.manualColor); return g_state.manualColor;
        case Mode::Effects: renderEffects(now); return Color::None;
    }
    return Color::None;
}

static Color renderSelfTest(uint32_t now) {
    static const Color steps[] = {Color::Green, Color::Yellow, Color::Red};
    static uint8_t step = 0;
    static uint32_t stepStartedMs = now;
    if (now - stepStartedMs >= SELFTEST_STEP_MS) {
        stepStartedMs = now;
        if (++step >= 3) {
            g_selfTestDone = true;
            restartTimers(now);
            return render(now);
        }
    }
    lightChamber(steps[step]);
    return steps[step];
}

void lightsBegin() {
    const uint8_t brightness = settingsGet(SETTING_BRIGHTNESS, DEFAULT_BRIGHTNESS_PERCENT);
    g_state.brightnessPercent = brightness <= 100 ? brightness : DEFAULT_BRIGHTNESS_PERCENT;
#if SERIAL_DEBUG
    Serial.printf("brightness %u%% (saved)\n", g_state.brightnessPercent);
#endif
    buttonsBegin();
    FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(g_leds, LED_COUNT_TOTAL);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_CURRENT_BUDGET_MA);
    fill_solid(g_leds, LED_COUNT_TOTAL, CRGB::Black);
    FastLED.show();
    controlPublish({g_state, Color::None, EFFECTS[g_state.effectIndex].name});
}

// Sleeps on the command queue until the next frame is due. A command renders a
// frame immediately, so the LEDs and the published status change together.
void lightsRun() {
    const uint32_t sinceFrame = millis() - g_lastFrameMs;
    const TickType_t wait = sinceFrame >= LED_FRAME_INTERVAL_MS ? 0 : pdMS_TO_TICKS(LED_FRAME_INTERVAL_MS - sinceFrame);
    Command command;
    const bool received = controlReceive(command, wait);
    if (received) {
        handleCommand(command, millis());
        while (controlReceive(command, 0)) handleCommand(command, millis());
    }
    const uint32_t now = millis();
    if (!received && now - g_lastFrameMs < LED_FRAME_INTERVAL_MS) return;
    g_lastFrameMs = now;
    buttonsPoll(now);
    const Color lit = g_selfTestDone ? render(now) : renderSelfTest(now);
    FastLED.setBrightness((uint8_t)((uint16_t)g_state.brightnessPercent * 255 / 100));
    FastLED.show();
    controlPublish({g_state, lit, EFFECTS[g_state.effectIndex].name});
}

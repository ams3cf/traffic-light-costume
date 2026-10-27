#include "lights.h"

#include <FastLED.h>

#include "buttons.h"
#include "config.h"
#include "control.h"
#include "effects.h"
#include "rings.h"
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
// Each effect's current colors, starting from its defaults; indexed like EFFECTS.
static CRGB g_effectColors[MAX_EFFECTS][MAX_EFFECT_COLORS];
static uint64_t g_unsavedColors = 0;  // Bit per effect.
static bool g_savePending = false;
static uint32_t g_saveDueMs = 0;
static_assert(MAX_EFFECTS <= 64, "g_unsavedColors has one bit per effect");

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

// Restores what was showing at power-off. Brightness is restored on its own so a
// low "safe current" setting applies even if the rest is missing.
static void loadState() {
    const LightState fallback = g_state;
    LightState saved;
    saved.mode = (Mode)settingsGet(SETTING_MODE, (uint8_t)fallback.mode);
    saved.manualColor = (Color)settingsGet(SETTING_MANUAL_COLOR, (uint8_t)fallback.manualColor);
    saved.effectIndex = settingsGet(SETTING_EFFECT, fallback.effectIndex);
    saved.cycleEffects = settingsGet(SETTING_CYCLE, fallback.cycleEffects) != 0;
    saved.brightnessPercent = settingsGet(SETTING_BRIGHTNESS, fallback.brightnessPercent);
    saved.trafficSpeed = settingsGet(SETTING_TRAFFIC_SPEED, fallback.trafficSpeed);
    saved.effectSpeed = settingsGet(SETTING_EFFECT_SPEED, fallback.effectSpeed);
    sanitizeState(saved, fallback, EFFECT_COUNT);
    g_state = saved;
}

static void loadEffectColors() {
    for (uint8_t i = 0; i < EFFECT_COUNT; ++i) {
        const Effect& effect = EFFECTS[i];
        memcpy(g_effectColors[i], effect.defaults, sizeof(g_effectColors[i]));
        uint8_t rgb[MAX_EFFECT_COLORS * 3];
        if (effect.colorCount == 0 || !settingsGetEffectColors(effect.name, rgb, effect.colorCount * 3)) continue;
        for (uint8_t slot = 0; slot < effect.colorCount; ++slot) {
            g_effectColors[i][slot] = CRGB(rgb[slot * 3], rgb[slot * 3 + 1], rgb[slot * 3 + 2]);
        }
    }
}

// Colors equal to the defaults are removed rather than stored, so a later change
// to an effect's defaults still reaches it.
static void saveEffectColors(uint8_t index) {
    const Effect& effect = EFFECTS[index];
    if (memcmp(g_effectColors[index], effect.defaults, effect.colorCount * sizeof(CRGB)) == 0) {
        settingsSetEffectColors(effect.name, nullptr, 0);
        return;
    }
    uint8_t rgb[MAX_EFFECT_COLORS * 3];
    for (uint8_t slot = 0; slot < effect.colorCount; ++slot) memcpy(rgb + slot * 3, g_effectColors[index][slot].raw, 3);
    settingsSetEffectColors(effect.name, rgb, effect.colorCount * 3);
}

// Writing waits for the state to settle, so stepping through modes or holding a
// speed button costs one flash write. Settings skip unchanged values.
static void scheduleSave(uint32_t now) {
    g_savePending = true;
    g_saveDueMs = now + STATE_SAVE_DELAY_MS;
}

static void saveIfDue(uint32_t now) {
    if (!g_savePending || (int32_t)(now - g_saveDueMs) < 0) return;
    g_savePending = false;
    settingsSet(SETTING_MODE, (uint8_t)g_state.mode);
    settingsSet(SETTING_MANUAL_COLOR, (uint8_t)g_state.manualColor);
    settingsSet(SETTING_EFFECT, g_state.effectIndex);
    settingsSet(SETTING_CYCLE, g_state.cycleEffects ? 1 : 0);
    settingsSet(SETTING_TRAFFIC_SPEED, g_state.trafficSpeed);
    settingsSet(SETTING_EFFECT_SPEED, g_state.effectSpeed);
    for (uint8_t i = 0; i < EFFECT_COUNT; ++i) {
        if ((g_unsavedColors >> i) & 1) saveEffectColors(i);
    }
    g_unsavedColors = 0;
}

static void setEffectColor(const Command& command, uint32_t now) {
    if (command.value >= EFFECT_COUNT || command.slot >= EFFECTS[command.value].colorCount) return;
    CRGB& color = g_effectColors[command.value][command.slot];
    color = command.rgb == EFFECT_COLOR_DEFAULT ? EFFECTS[command.value].defaults[command.slot] : CRGB(command.rgb);
    g_unsavedColors |= 1ULL << command.value;
    scheduleSave(now);
}

static void handleCommand(const Command& command, uint32_t now) {
    const LightState before = g_state;
    if (!g_selfTestDone) {
        g_selfTestDone = true;
        restartTimers(now);
    }
    if (command.type == CommandType::SetEffectColor) return setEffectColor(command, now);
    if (!applyCommand(g_state, command, EFFECT_COUNT)) return;
    // Saved immediately so a low "safe current" brightness also applies to the next boot's
    // self-test. Only the web slider changes brightness, once per release, so writes are rare.
    if (g_state.brightnessPercent != before.brightnessPercent) settingsSet(SETTING_BRIGHTNESS, g_state.brightnessPercent);
    else scheduleSave(now);
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
        EFFECTS[g_state.effectIndex].render(g_leds, g_effectClockMs, g_effectColors[g_state.effectIndex]);
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

static void publish(Color lit) {
    const uint8_t index = g_state.effectIndex;
    Status status = {g_state, lit, EFFECTS[index].name, EFFECTS[index].colorCount, {}};
    for (uint8_t slot = 0; slot < status.colorCount; ++slot) {
        const CRGB& color = g_effectColors[index][slot];
        status.colors[slot] = (uint32_t)color.r << 16 | (uint32_t)color.g << 8 | color.b;
    }
    controlPublish(status);
}

void lightsBegin() {
    loadState();
    loadEffectColors();
    ringsBegin();
#if SERIAL_DEBUG
    Serial.printf("restored mode=%s effect=%s brightness=%u%%\n", modeName(g_state.mode), EFFECTS[g_state.effectIndex].name,
                  g_state.brightnessPercent);
#endif
    buttonsBegin();
    FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(g_leds, LED_COUNT_TOTAL);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_CURRENT_BUDGET_MA);
    fill_solid(g_leds, LED_COUNT_TOTAL, CRGB::Black);
    FastLED.show();
    publish(Color::None);
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
    publish(lit);
    saveIfDue(now);
}

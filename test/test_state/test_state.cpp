#include <unity.h>

#include "state.h"

static constexpr uint8_t EFFECTS = 5;

static LightState initial() { return {Mode::Auto, Color::Red, 0, true, 31, SPEED_NORMAL, SPEED_NORMAL}; }

static bool button(LightState& state, ButtonId id, Gesture gesture) {
    return applyCommand(state, buttonCommand(id, gesture), EFFECTS);
}

void setUp() {}
void tearDown() {}

static void test_parse_mode_accepts_names() {
    Mode mode;
    TEST_ASSERT_TRUE(parseMode("effects", mode));
    TEST_ASSERT_EQUAL(Mode::Effects, mode);
    TEST_ASSERT_FALSE(parseMode("Auto", mode));
    TEST_ASSERT_FALSE(parseMode("", mode));
}

static void test_parse_color_rejects_none() {
    Color color;
    TEST_ASSERT_TRUE(parseColor("yellow", color));
    TEST_ASSERT_EQUAL(Color::Yellow, color);
    TEST_ASSERT_FALSE(parseColor("none", color));
    TEST_ASSERT_FALSE(parseColor("blue", color));
}

static void test_parse_brightness_bounds() {
    uint8_t percent = 0;
    TEST_ASSERT_TRUE(parseBrightness("0", percent));
    TEST_ASSERT_EQUAL_UINT8(0, percent);
    TEST_ASSERT_TRUE(parseBrightness("100", percent));
    TEST_ASSERT_EQUAL_UINT8(100, percent);
    TEST_ASSERT_FALSE(parseBrightness("101", percent));
    TEST_ASSERT_FALSE(parseBrightness("", percent));
    TEST_ASSERT_FALSE(parseBrightness("5a", percent));
    TEST_ASSERT_FALSE(parseBrightness("-1", percent));
}

static void test_select_color_enters_manual() {
    LightState state = initial();
    TEST_ASSERT_TRUE(applyCommand(state, {CommandType::SelectColor, (uint8_t)Color::Green}, EFFECTS));
    TEST_ASSERT_EQUAL(Mode::Manual, state.mode);
    TEST_ASSERT_EQUAL(Color::Green, state.manualColor);
}

static void test_repeat_color_press_is_no_change() {
    LightState state = initial();
    applyCommand(state, {CommandType::SelectColor, (uint8_t)Color::Yellow}, EFFECTS);
    TEST_ASSERT_FALSE(applyCommand(state, {CommandType::SelectColor, (uint8_t)Color::Yellow}, EFFECTS));
}

static void test_select_color_ignores_none_and_out_of_range() {
    LightState state = initial();
    TEST_ASSERT_FALSE(applyCommand(state, {CommandType::SelectColor, (uint8_t)Color::None}, EFFECTS));
    TEST_ASSERT_FALSE(applyCommand(state, {CommandType::SelectColor, 9}, EFFECTS));
    TEST_ASSERT_EQUAL(Mode::Auto, state.mode);
}

static void test_mode_tap_cycles_through_all_three() {
    LightState state = initial();
    TEST_ASSERT_FALSE(button(state, ButtonId::Mode, Gesture::Press));
    button(state, ButtonId::Mode, Gesture::Tap);
    TEST_ASSERT_EQUAL(Mode::Manual, state.mode);
    button(state, ButtonId::Mode, Gesture::Tap);
    TEST_ASSERT_EQUAL(Mode::Effects, state.mode);
    button(state, ButtonId::Mode, Gesture::Tap);
    TEST_ASSERT_EQUAL(Mode::Auto, state.mode);
}

static void test_mode_hold_returns_to_auto() {
    LightState state = initial();
    state.mode = Mode::Effects;
    button(state, ButtonId::Mode, Gesture::Hold);
    TEST_ASSERT_EQUAL(Mode::Auto, state.mode);
}

static void test_manual_color_press_selects_color() {
    LightState state = initial();
    state.mode = Mode::Manual;
    TEST_ASSERT_TRUE(button(state, ButtonId::Green, Gesture::Press));
    TEST_ASSERT_EQUAL(Color::Green, state.manualColor);
    TEST_ASSERT_FALSE(button(state, ButtonId::Red, Gesture::Tap));
    TEST_ASSERT_FALSE(button(state, ButtonId::Red, Gesture::Hold));
}

static void test_auto_green_red_change_speed_and_stay_in_auto() {
    LightState state = initial();
    button(state, ButtonId::Green, Gesture::Press);
    button(state, ButtonId::Green, Gesture::Hold);
    TEST_ASSERT_EQUAL(Mode::Auto, state.mode);
    TEST_ASSERT_EQUAL_UINT8(SPEED_NORMAL + 2, state.trafficSpeed);
    TEST_ASSERT_FALSE(button(state, ButtonId::Green, Gesture::Tap));
    button(state, ButtonId::Red, Gesture::Press);
    TEST_ASSERT_EQUAL_UINT8(SPEED_NORMAL + 1, state.trafficSpeed);
    TEST_ASSERT_EQUAL_UINT8(SPEED_NORMAL, state.effectSpeed);
}

static void test_auto_yellow_resets_speed() {
    LightState state = initial();
    state.trafficSpeed = 0;
    button(state, ButtonId::Yellow, Gesture::Press);
    TEST_ASSERT_EQUAL_UINT8(SPEED_NORMAL, state.trafficSpeed);
    TEST_ASSERT_EQUAL(Mode::Auto, state.mode);
}

static void test_speed_clamps_at_limits() {
    LightState state = initial();
    for (int i = 0; i < 20; ++i) button(state, ButtonId::Green, Gesture::Hold);
    TEST_ASSERT_EQUAL_UINT8(SPEED_LEVELS - 1, state.trafficSpeed);
    for (int i = 0; i < 20; ++i) button(state, ButtonId::Red, Gesture::Hold);
    TEST_ASSERT_EQUAL_UINT8(0, state.trafficSpeed);
}

static void test_effects_taps_step_and_wrap() {
    LightState state = initial();
    state.mode = Mode::Effects;
    TEST_ASSERT_FALSE(button(state, ButtonId::Green, Gesture::Press));
    button(state, ButtonId::Green, Gesture::Tap);
    TEST_ASSERT_EQUAL_UINT8(1, state.effectIndex);
    button(state, ButtonId::Red, Gesture::Tap);
    button(state, ButtonId::Red, Gesture::Tap);
    TEST_ASSERT_EQUAL_UINT8(EFFECTS - 1, state.effectIndex);
    TEST_ASSERT_EQUAL(Mode::Effects, state.mode);
    TEST_ASSERT_TRUE(state.cycleEffects);
}

static void test_effects_holds_change_effect_speed() {
    LightState state = initial();
    state.mode = Mode::Effects;
    button(state, ButtonId::Green, Gesture::Hold);
    TEST_ASSERT_EQUAL_UINT8(SPEED_NORMAL + 1, state.effectSpeed);
    TEST_ASSERT_EQUAL_UINT8(0, state.effectIndex);
    button(state, ButtonId::Red, Gesture::Hold);
    button(state, ButtonId::Red, Gesture::Hold);
    TEST_ASSERT_EQUAL_UINT8(SPEED_NORMAL - 1, state.effectSpeed);
    button(state, ButtonId::Yellow, Gesture::Press);
    TEST_ASSERT_EQUAL_UINT8(SPEED_NORMAL, state.effectSpeed);
    TEST_ASSERT_EQUAL_UINT8(SPEED_NORMAL, state.trafficSpeed);
}

static void test_speed_percent_table() {
    TEST_ASSERT_EQUAL_UINT16(100, speedPercent(SPEED_NORMAL));
    TEST_ASSERT_TRUE(speedPercent(0) < 100);
    TEST_ASSERT_TRUE(speedPercent(SPEED_LEVELS - 1) > 100);
    TEST_ASSERT_EQUAL_UINT16(100, speedPercent(200));
}

static void test_manual_mode_keeps_last_color() {
    LightState state = initial();
    applyCommand(state, {CommandType::SelectColor, (uint8_t)Color::Green}, EFFECTS);
    applyCommand(state, {CommandType::SetMode, (uint8_t)Mode::Auto}, EFFECTS);
    applyCommand(state, {CommandType::SetMode, (uint8_t)Mode::Manual}, EFFECTS);
    TEST_ASSERT_EQUAL(Color::Green, state.manualColor);
}

static void test_set_mode_rejects_out_of_range() {
    LightState state = initial();
    TEST_ASSERT_FALSE(applyCommand(state, {CommandType::SetMode, 3}, EFFECTS));
}

static void test_set_effect_holds_chosen_effect() {
    LightState state = initial();
    TEST_ASSERT_TRUE(applyCommand(state, {CommandType::SetEffect, 3}, EFFECTS));
    TEST_ASSERT_EQUAL(Mode::Effects, state.mode);
    TEST_ASSERT_EQUAL_UINT8(3, state.effectIndex);
    TEST_ASSERT_FALSE(state.cycleEffects);
}

static void test_set_effect_cycle_keeps_current_effect() {
    LightState state = initial();
    applyCommand(state, {CommandType::SetEffect, 2}, EFFECTS);
    TEST_ASSERT_TRUE(applyCommand(state, {CommandType::SetEffect, EFFECT_CYCLE}, EFFECTS));
    TEST_ASSERT_TRUE(state.cycleEffects);
    TEST_ASSERT_EQUAL_UINT8(2, state.effectIndex);
}

static void test_set_effect_rejects_out_of_range() {
    LightState state = initial();
    TEST_ASSERT_FALSE(applyCommand(state, {CommandType::SetEffect, EFFECTS}, EFFECTS));
    TEST_ASSERT_EQUAL(Mode::Auto, state.mode);
}

static void test_brightness_rejects_over_100() {
    LightState state = initial();
    TEST_ASSERT_TRUE(applyCommand(state, {CommandType::SetBrightness, 80}, EFFECTS));
    TEST_ASSERT_EQUAL_UINT8(80, state.brightnessPercent);
    TEST_ASSERT_FALSE(applyCommand(state, {CommandType::SetBrightness, 101}, EFFECTS));
    TEST_ASSERT_EQUAL_UINT8(80, state.brightnessPercent);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parse_mode_accepts_names);
    RUN_TEST(test_parse_color_rejects_none);
    RUN_TEST(test_parse_brightness_bounds);
    RUN_TEST(test_select_color_enters_manual);
    RUN_TEST(test_repeat_color_press_is_no_change);
    RUN_TEST(test_select_color_ignores_none_and_out_of_range);
    RUN_TEST(test_mode_tap_cycles_through_all_three);
    RUN_TEST(test_mode_hold_returns_to_auto);
    RUN_TEST(test_manual_color_press_selects_color);
    RUN_TEST(test_auto_green_red_change_speed_and_stay_in_auto);
    RUN_TEST(test_auto_yellow_resets_speed);
    RUN_TEST(test_speed_clamps_at_limits);
    RUN_TEST(test_effects_taps_step_and_wrap);
    RUN_TEST(test_effects_holds_change_effect_speed);
    RUN_TEST(test_speed_percent_table);
    RUN_TEST(test_manual_mode_keeps_last_color);
    RUN_TEST(test_set_mode_rejects_out_of_range);
    RUN_TEST(test_set_effect_holds_chosen_effect);
    RUN_TEST(test_set_effect_cycle_keeps_current_effect);
    RUN_TEST(test_set_effect_rejects_out_of_range);
    RUN_TEST(test_brightness_rejects_over_100);
    return UNITY_END();
}

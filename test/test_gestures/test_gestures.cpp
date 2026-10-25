#include <unity.h>

#include "gestures.h"

static constexpr GestureTiming TIMING = {40, 1000, 400, 3000};
static constexpr uint8_t MODE = 1 << (uint8_t)ButtonId::Mode;
static constexpr uint8_t RED = 1 << (uint8_t)ButtonId::Red;
static constexpr uint8_t GREEN = 1 << (uint8_t)ButtonId::Green;

static ButtonEvent g_events[64];
static uint8_t g_count;
static uint32_t g_now;

// Holds `mask` for `durationMs`, sampling every 10 ms like the 25 ms frame loop would.
static void hold(ButtonGestures& gestures, uint8_t mask, uint32_t durationMs) {
    for (uint32_t end = g_now + durationMs; g_now < end; g_now += 10) {
        g_count += gestures.update(mask, g_now, g_events + g_count, (uint8_t)(sizeof(g_events) / sizeof(g_events[0]) - g_count));
    }
}

static uint8_t countOf(ButtonEventKind kind, ButtonId button, Gesture gesture) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < g_count; ++i) {
        const ButtonEvent& e = g_events[i];
        if (e.kind == kind && e.button == button && (kind == ButtonEventKind::Chord || e.gesture == gesture)) ++n;
    }
    return n;
}

static uint8_t gestures(ButtonId button, Gesture gesture) { return countOf(ButtonEventKind::Gesture, button, gesture); }
static uint8_t chords(ButtonId button) { return countOf(ButtonEventKind::Chord, button, Gesture::Press); }

void setUp() {
    g_count = 0;
    g_now = 1000;
}
void tearDown() {}

static void test_short_press_is_press_then_tap() {
    ButtonGestures g(TIMING);
    g.begin(0);
    hold(g, RED, 200);
    hold(g, 0, 100);
    TEST_ASSERT_EQUAL_UINT8(1, gestures(ButtonId::Red, Gesture::Press));
    TEST_ASSERT_EQUAL_UINT8(1, gestures(ButtonId::Red, Gesture::Tap));
    TEST_ASSERT_EQUAL_UINT8(0, gestures(ButtonId::Red, Gesture::Hold));
}

static void test_bounce_shorter_than_debounce_is_ignored() {
    ButtonGestures g(TIMING);
    g.begin(0);
    hold(g, GREEN, 20);
    hold(g, 0, 100);
    TEST_ASSERT_EQUAL_UINT8(0, g_count);
}

static void test_long_hold_repeats_and_suppresses_tap() {
    ButtonGestures g(TIMING);
    g.begin(0);
    hold(g, GREEN, 2000);  // Hold at ~1.04 s, then every 400 ms: ~1.44 s, ~1.84 s.
    hold(g, 0, 100);
    TEST_ASSERT_EQUAL_UINT8(3, gestures(ButtonId::Green, Gesture::Hold));
    TEST_ASSERT_EQUAL_UINT8(0, gestures(ButtonId::Green, Gesture::Tap));
}

static void test_pressed_at_boot_is_ignored_until_released() {
    ButtonGestures g(TIMING);
    TEST_ASSERT_EQUAL_UINT8(MODE, g.begin(MODE));
    hold(g, MODE, 3000);
    hold(g, 0, 100);
    TEST_ASSERT_EQUAL_UINT8(0, g_count);
    hold(g, MODE, 100);
    hold(g, 0, 100);
    TEST_ASSERT_EQUAL_UINT8(1, gestures(ButtonId::Mode, Gesture::Tap));
}

static void test_mode_then_green_chord_fires_once_without_side_effects() {
    ButtonGestures g(TIMING);
    g.begin(0);
    hold(g, MODE, 200);
    hold(g, MODE | GREEN, 4000);
    hold(g, 0, 100);
    TEST_ASSERT_EQUAL_UINT8(1, chords(ButtonId::Green));
    TEST_ASSERT_EQUAL_UINT8(0, gestures(ButtonId::Green, Gesture::Press));
    TEST_ASSERT_EQUAL_UINT8(0, gestures(ButtonId::Green, Gesture::Hold));
    TEST_ASSERT_EQUAL_UINT8(0, gestures(ButtonId::Mode, Gesture::Hold));
    TEST_ASSERT_EQUAL_UINT8(0, gestures(ButtonId::Mode, Gesture::Tap));
}

static void test_green_then_mode_chord_suppresses_later_gestures() {
    ButtonGestures g(TIMING);
    g.begin(0);
    hold(g, GREEN, 200);
    hold(g, MODE | GREEN, 4000);
    hold(g, 0, 100);
    TEST_ASSERT_EQUAL_UINT8(1, chords(ButtonId::Green));
    TEST_ASSERT_EQUAL_UINT8(1, gestures(ButtonId::Green, Gesture::Press));  // Already sent before MODE.
    TEST_ASSERT_EQUAL_UINT8(0, gestures(ButtonId::Green, Gesture::Hold));
    TEST_ASSERT_EQUAL_UINT8(0, gestures(ButtonId::Green, Gesture::Tap));
    TEST_ASSERT_EQUAL_UINT8(0, gestures(ButtonId::Mode, Gesture::Press));
}

static void test_chord_released_early_does_not_fire() {
    ButtonGestures g(TIMING);
    g.begin(0);
    hold(g, MODE | GREEN, 2000);
    hold(g, 0, 100);
    TEST_ASSERT_EQUAL_UINT8(0, chords(ButtonId::Green));
    TEST_ASSERT_EQUAL_UINT8(0, gestures(ButtonId::Mode, Gesture::Tap));
}

static void test_two_color_buttons_are_not_a_chord() {
    ButtonGestures g(TIMING);
    g.begin(0);
    hold(g, RED | GREEN, 200);
    hold(g, 0, 100);
    TEST_ASSERT_EQUAL_UINT8(0, chords(ButtonId::Green));
    TEST_ASSERT_EQUAL_UINT8(1, gestures(ButtonId::Red, Gesture::Tap));
    TEST_ASSERT_EQUAL_UINT8(1, gestures(ButtonId::Green, Gesture::Tap));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_short_press_is_press_then_tap);
    RUN_TEST(test_bounce_shorter_than_debounce_is_ignored);
    RUN_TEST(test_long_hold_repeats_and_suppresses_tap);
    RUN_TEST(test_pressed_at_boot_is_ignored_until_released);
    RUN_TEST(test_mode_then_green_chord_fires_once_without_side_effects);
    RUN_TEST(test_green_then_mode_chord_suppresses_later_gestures);
    RUN_TEST(test_chord_released_early_does_not_fire);
    RUN_TEST(test_two_color_buttons_are_not_a_chord);
    return UNITY_END();
}

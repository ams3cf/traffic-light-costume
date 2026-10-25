#include "buttons.h"

#include "config.h"
#include "control.h"
#include "gestures.h"
#include "network.h"

// Indexed by ButtonId.
static const uint8_t BUTTON_PINS[BUTTON_COUNT] = {PIN_MODE_BUTTON, PIN_RED_BUTTON, PIN_YELLOW_BUTTON, PIN_GREEN_BUTTON};
static const char* const BUTTON_NAMES[BUTTON_COUNT] = {"mode", "red", "yellow", "green"};
static const char* const GESTURE_NAMES[] = {"press", "tap", "hold"};

static ButtonGestures g_gestures({BUTTON_DEBOUNCE_MS, BUTTON_LONG_PRESS_MS, BUTTON_REPEAT_MS, BUTTON_CHORD_MS});

static uint8_t readPressedMask() {
    uint8_t mask = 0;
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        if (digitalRead(BUTTON_PINS[i]) == BUTTON_PRESSED_LEVEL) mask |= (uint8_t)(1 << i);
    }
    return mask;
}

void buttonsBegin() {
    for (const uint8_t pin : BUTTON_PINS) pinMode(pin, INPUT_PULLUP);
    delay(5);  // Let the pull-ups charge open (pressed or unplugged) lines before sampling.
    const uint8_t ignored = g_gestures.begin(readPressedMask());
#if SERIAL_DEBUG
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        if ((ignored >> i) & 1) Serial.printf("button %s reads pressed at boot; ignored until released (unplugged?)\n", BUTTON_NAMES[i]);
    }
#else
    (void)ignored;
#endif
}

static void handleChord(ButtonId button) {
    if (button == ButtonId::Green) networkRequestToggle();
}

void buttonsPoll(uint32_t now) {
    ButtonEvent events[BUTTON_COUNT * 2 + 1];
    const uint8_t count = g_gestures.update(readPressedMask(), now, events, sizeof(events) / sizeof(events[0]));
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

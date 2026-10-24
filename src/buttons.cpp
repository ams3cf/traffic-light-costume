#include "buttons.h"

#include "config.h"
#include "control.h"

struct Button {
    uint8_t pin;
    Command press;
    bool hasLongPress;
    Command longPress;
    bool stableHigh;
    bool rawHigh;
    bool longPressSent;
    uint32_t lastChangeMs;
    uint32_t pressedAtMs;
};

static Button g_buttons[] = {
    {PIN_MODE_BUTTON, {CommandType::NextMode, 0}, true, {CommandType::SetMode, (uint8_t)Mode::Auto}, true, true, false, 0, 0},
    {PIN_RED_BUTTON, {CommandType::SelectColor, (uint8_t)Color::Red}, false, {}, true, true, false, 0, 0},
    {PIN_YELLOW_BUTTON, {CommandType::SelectColor, (uint8_t)Color::Yellow}, false, {}, true, true, false, 0, 0},
    {PIN_GREEN_BUTTON, {CommandType::SelectColor, (uint8_t)Color::Green}, false, {}, true, true, false, 0, 0},
};

void buttonsBegin() {
    for (const Button& button : g_buttons) pinMode(button.pin, INPUT_PULLUP);
}

// Buttons with a long press act on release so a hold does not also fire the short action;
// the others act on the press edge for immediate feedback.
static void pollButton(Button& button, uint32_t now) {
    const bool rawHigh = digitalRead(button.pin) == HIGH;
    if (rawHigh != button.rawHigh) { button.rawHigh = rawHigh; button.lastChangeMs = now; }
    if (rawHigh != button.stableHigh && now - button.lastChangeMs >= BUTTON_DEBOUNCE_MS) {
        button.stableHigh = rawHigh;
        if (!rawHigh) {
            button.pressedAtMs = now;
            button.longPressSent = false;
            if (!button.hasLongPress) controlSend(button.press);
        } else if (button.hasLongPress && !button.longPressSent) {
            controlSend(button.press);
        }
    }
    if (button.hasLongPress && !button.stableHigh && !button.longPressSent && now - button.pressedAtMs >= BUTTON_LONG_PRESS_MS) {
        button.longPressSent = true;
        controlSend(button.longPress);
    }
}

void buttonsPoll(uint32_t now) {
    for (Button& button : g_buttons) pollButton(button, now);
}

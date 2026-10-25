#include "gestures.h"

static constexpr uint8_t MODE_INDEX = (uint8_t)ButtonId::Mode;

static void emit(ButtonEvent* events, uint8_t capacity, uint8_t& count, const ButtonEvent& event) {
    if (count < capacity) events[count++] = event;
}

uint8_t ButtonGestures::begin(uint8_t pressedMask) {
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        const bool pressed = (pressedMask >> i) & 1;
        buttons_[i] = {pressed, pressed, pressed, false, false, 0, 0};
    }
    chordButton_ = -1;
    return pressedMask & ((1 << BUTTON_COUNT) - 1);
}

void ButtonGestures::onPress(uint8_t index, uint32_t now, ButtonEvent* events, uint8_t capacity, uint8_t& count) {
    Button& button = buttons_[index];
    button.held = false;
    button.suppressed = false;
    button.nextHoldMs = now + timing_.longPressMs;
    // A chord forms when MODE and one other active button are down together, in either order.
    for (uint8_t other = 0; other < BUTTON_COUNT; ++other) {
        if (other == index || (index != MODE_INDEX && other != MODE_INDEX)) continue;
        Button& partner = buttons_[other];
        if (!partner.stable || partner.ignore) continue;
        button.suppressed = true;
        partner.suppressed = true;
        chordButton_ = (int8_t)(index == MODE_INDEX ? other : index);
        chordFired_ = false;
        chordStartMs_ = now;
        return;
    }
    emit(events, capacity, count, {ButtonEventKind::Gesture, (ButtonId)index, Gesture::Press});
}

void ButtonGestures::onRelease(uint8_t index, ButtonEvent* events, uint8_t capacity, uint8_t& count) {
    Button& button = buttons_[index];
    if (!button.suppressed && !button.held) emit(events, capacity, count, {ButtonEventKind::Gesture, (ButtonId)index, Gesture::Tap});
    button.suppressed = false;
    if (index == MODE_INDEX || index == chordButton_) chordButton_ = -1;
}

uint8_t ButtonGestures::update(uint8_t pressedMask, uint32_t now, ButtonEvent* events, uint8_t capacity) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        Button& button = buttons_[i];
        const bool raw = (pressedMask >> i) & 1;
        if (raw != button.raw) { button.raw = raw; button.lastChangeMs = now; }
        if (raw == button.stable || now - button.lastChangeMs < timing_.debounceMs) continue;
        button.stable = raw;
        if (button.ignore) {
            if (!raw) button.ignore = false;
        } else if (raw) {
            onPress(i, now, events, capacity, count);
        } else {
            onRelease(i, events, capacity, count);
        }
    }
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        Button& button = buttons_[i];
        if (!button.stable || button.ignore || button.suppressed || (int32_t)(now - button.nextHoldMs) < 0) continue;
        button.held = true;
        button.nextHoldMs = now + timing_.repeatMs;
        emit(events, capacity, count, {ButtonEventKind::Gesture, (ButtonId)i, Gesture::Hold});
    }
    if (chordButton_ >= 0 && !chordFired_ && now - chordStartMs_ >= timing_.chordMs) {
        chordFired_ = true;
        emit(events, capacity, count, {ButtonEventKind::Chord, (ButtonId)chordButton_, Gesture::Press});
    }
    return count;
}

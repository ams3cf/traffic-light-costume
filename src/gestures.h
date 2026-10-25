// ============================================================================
// Button gesture recognition: debounce, press / tap / hold, and MODE chords.
//
// Pure C++ (no Arduino), so it is unit-tested on the host. buttons.cpp feeds
// it a bitmask of which buttons read as pressed; it returns events.
//
//   Press  debounced press edge
//   Tap    release with no Hold in between
//   Hold   long-press time reached, then every repeat interval while held
//   Chord  MODE and another button held together for chordMs
//
// MODE acts as a shift key: once MODE and another button are down together,
// neither produces Press/Tap/Hold until released, so a chord never also
// changes speed, color, or mode. Buttons pressed at begin() (held at boot, or
// an unplugged normally-closed button) are ignored until released.
// ============================================================================
#pragma once

#include <stdint.h>

#include "state.h"

static constexpr uint8_t BUTTON_COUNT = 4;  // Indexed by ButtonId; bit i of a mask = ButtonId i.

enum class ButtonEventKind : uint8_t { Gesture, Chord };

struct ButtonEvent {
    ButtonEventKind kind;
    ButtonId button;  // For Chord: the button held together with MODE.
    Gesture gesture;  // Only for kind == Gesture.
};

struct GestureTiming {
    uint32_t debounceMs;
    uint32_t longPressMs;
    uint32_t repeatMs;
    uint32_t chordMs;
};

class ButtonGestures {
public:
    explicit ButtonGestures(const GestureTiming& timing) : timing_(timing) {}

    // Returns the mask of buttons that read pressed and are therefore ignored until released.
    uint8_t begin(uint8_t pressedMask);
    // Feeds one sample; writes up to `capacity` events and returns how many.
    uint8_t update(uint8_t pressedMask, uint32_t now, ButtonEvent* events, uint8_t capacity);

private:
    struct Button {
        bool stable;
        bool raw;
        bool ignore;
        bool held;
        bool suppressed;
        uint32_t lastChangeMs;
        uint32_t nextHoldMs;
    };

    void onPress(uint8_t index, uint32_t now, ButtonEvent* events, uint8_t capacity, uint8_t& count);
    void onRelease(uint8_t index, ButtonEvent* events, uint8_t capacity, uint8_t& count);

    GestureTiming timing_;
    Button buttons_[BUTTON_COUNT] = {};
    int8_t chordButton_ = -1;
    bool chordFired_ = false;
    uint32_t chordStartMs_ = 0;
};

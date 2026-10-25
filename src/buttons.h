// ============================================================================
// Physical buttons: normally-closed to a common ground, internal pull-ups
// (see BUTTON_PRESSED_LEVEL). Gestures (see gestures.h) become ButtonEvent
// commands for the lights core; what they do depends on the mode and is
// decided in state.cpp. MODE + GREEN held together toggles the network mode.
// ============================================================================
#pragma once

#include <stdint.h>

void buttonsBegin();
void buttonsPoll(uint32_t now);

// ============================================================================
// The only channel between the web core and the lights core.
//
// Commands flow in through a FreeRTOS queue (any task may send; only the
// lights core receives). Status flows out as a snapshot the lights core
// publishes every frame and the web core copies under a spinlock.
// ============================================================================
#pragma once

#include <Arduino.h>

#include "state.h"

struct Status {
    LightState state;
    Color lit;               // Chamber currently lit; None while an effect runs.
    const char* effectName;  // Points at a static string; safe to read on any core.
};

void controlBegin();
bool controlSend(const Command& command);  // False when the queue is full.
bool controlReceive(Command& command, TickType_t wait);
void controlPublish(const Status& status);
Status controlSnapshot();

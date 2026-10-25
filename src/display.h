// ============================================================================
// OLED status display: a read-only consumer of the Status snapshot, running
// in its own low-priority task on the web core so slow I2C redraws can never
// stall the lights. The display may be absent or plugged in later; the task
// probes for it and (re)initializes whenever it appears.
//
//   EFFECTS        1.35x     <- mode, then speed (or color in manual)
//   fire  cycling     31%    <- what is showing, brightness
//   tap G next  R previous   <- rotating: button hints, Wi-Fi name/password
// ============================================================================
#pragma once

void displayBegin();

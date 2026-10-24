// ============================================================================
// Lights core: owns the LED buffer, the costume state, and all animation
// timing. Runs only from loop() (core 1); it is the sole consumer of the
// command queue and the sole publisher of Status.
// ============================================================================
#pragma once

void lightsBegin();
void lightsRun();

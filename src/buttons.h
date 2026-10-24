// ============================================================================
// Physical buttons, all active-low with internal pull-ups. Presses become
// Commands on the shared queue, the same path the web UI uses.
//
//   MODE   short: next mode (auto -> manual -> effects)   long: auto
//   RED / YELLOW / GREEN: switch to manual with that chamber lit
// ============================================================================
#pragma once

#include <stdint.h>

void buttonsBegin();
void buttonsPoll(uint32_t now);

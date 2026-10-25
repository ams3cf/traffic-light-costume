// ============================================================================
// Traffic Light Costume — ESP32 firmware
//
// Two cores, one queue between them:
//   core 1  loop() -> lights: buttons, modes, effects, FastLED output
//   core 0  web task: Wi-Fi access point, HTTP UI and status API
//   core 0  display task: OLED status (read-only, low rate)
// ============================================================================

#include <Arduino.h>

#include "config.h"
#include "control.h"
#include "display.h"
#include "lights.h"
#include "web.h"

void setup() {
#if SERIAL_DEBUG
    Serial.begin(SERIAL_BAUD);
    Serial.println("Traffic Light Costume - booting");
#endif
    controlBegin();
    lightsBegin();
    webBegin();
    displayBegin();
}

// Arduino runs loop() on core 1 (ARDUINO_RUNNING_CORE), away from the Wi-Fi stack.
void loop() { lightsRun(); }

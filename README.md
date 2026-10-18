# Traffic Light Costume

A wearable traffic-light costume for a child, driven by an **ESP32** and one
daisy-chained **WS2812B LED strip** divided into bottom, middle, and top sections.

The firmware exposes a text-based BLE GATT interface for **LightBlue by Punch
Through**, physical buttons, and Wi-Fi OTA. It runs independently of the phone
after a BLE disconnect.

> **Status: firmware baseline, NOT hardware-validated.** This code compiles
> cleanly with PlatformIO but has not been flashed or tested against physical
> hardware. Board variant, pinout, LED counts, and power design are documented
> *assumptions* below — verify and adjust `src/config.h` to match your build.

---

## Features

- Three 29-LED WS2812B sections driven as one 87-LED daisy chain.
- Open BLE GATT controls with plain-text mode/effect/brightness values and state notifications.
- Optional mode/action buttons and an effect showcase sequence.
- Non-blocking animation loop with a global 1,500 mA FastLED power limiter.
- Optional password-protected Wi-Fi OTA; disabled until local credentials are configured.

## Repository layout

```
traffic-light-costume/
├── platformio.ini      # build config (board, framework, FastLED dependency)
├── README.md           # this file
├── .gitignore
└── src/
    ├── config.h        # ALL hardware/timing/brightness knobs
    └── main.cpp        # self-test + traffic sequence state machine
```

## Configuration

Everything tunable is in [`src/config.h`](src/config.h):

| Setting | Default | Meaning |
|---|---:|---|
| `PIN_LED_DATA` | `16` | Data pin for the full LED chain |
| `PIN_MODE_BUTTON` | `18` | Optional mode button to ground |
| `PIN_ACTION_BUTTON` | `19` | Optional action button to ground |
| `LED_COUNT_PER_SECTION` | `29` | LEDs per section |
| `LED_CURRENT_BUDGET_MA` | `1500` | Global FastLED current limit |
| `DEFAULT_BRIGHTNESS_PERCENT` | `31` | Startup brightness, 0-100 |
| Section order | bottom, middle, top | Physical order from the ESP32 data input |

See [docs/BLE_PROTOCOL.md](docs/BLE_PROTOCOL.md) for the exact GATT UUIDs,
LightBlue workflow, values, feedback format, and access-control limitations.

---

## Wiring assumptions (UNVERIFIED — confirm before soldering)

The full WS2812B chain needs **5V**, **GND**, and one **DATA IN** connection.
Connect the ESP32 data pin to the bottom section's input, then connect each
section's data output to the next section's input in this order: bottom, middle,
top. The data output of the top section is left unconnected.

| Section | LED indexes | Signal color | Notes |
|---------|-------------|--------------|-------|
| Bottom  | 0-28        | Green        | First section connected to ESP32 data |
| Middle  | 29-57       | Yellow       | Chained after bottom |
| Top     | 58-86       | Red          | Chained after middle |

The default LED data pin avoids ESP32 strapping pins (GPIO 0, 2, 5, 12, 15),
the flash pins (GPIO 6–11), input-only pins (GPIO 34–39), and the UART0 console
(GPIO 1, 3). GPIO18 and GPIO19 are configured as optional buttons with internal
pull-ups; each switch connects the pin to ground. If your board routes a
different set, change `src/config.h`.

**Recommended LED wiring:**
1. Common **GND** between the ESP32, the LED ring, and the power source.
2. **5V** to the ring's power pad.
3. **DATA IN** to the ESP32 GPIO through a ~330 Ω series resistor (protects the
   first LED from overvoltage/ringing on the data line).
4. A 100–1000 µF electrolytic capacitor across the power rail at the ring helps
   absorb current spikes when the LEDs switch.

## Power budget (assumptions, not measurements)

The firmware sets FastLED's global estimated output ceiling to **1,500 mA**.
This is a software limiter, not hardware overcurrent protection, and actual
WS2812B current varies by pixel batch, color, voltage, and temperature. Use a
regulated 5 V supply sized with suitable headroom and wiring; validate current
with a meter before wearing the costume.

Changing the LED count, supply voltage, or power limit requires a new current
check. The limiter cannot protect against wiring faults or a supply short.

**Do not power the rings from the ESP32's 3.3 V rail.** WS2812B data is 3.3 V
tolerant (ESP32 drives it directly), but the LEDs' *power* should come from 5 V,
shared with the ESP32's 5 V input (or a separate 5 V rail with common ground).

## Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI):
  ```bash
  pipx install platformio
  # or, if you prefer a venv:
  # python3 -m venv ~/pio && ~/pio/bin/pip install platformio
  ```
- A USB-to-serial connection to the ESP32 (most DevKits expose one via their
  onboard USB-UART bridge).

## Build & upload

```bash
pio run -e bench
pio run -e production
pio run -e production -t upload    # flash over USB
pio device monitor                 # serial monitor (115200 baud)
```

The `bench` and `production` profiles target the same ESP32 board. Production
silences serial debug output. NimBLE-Arduino provides BLE; no iOS application
is included or needed.

### Optional Wi-Fi OTA

Create `src/secrets.h` locally (it is ignored by Git) with your Wi-Fi network and
a unique OTA password:

```cpp
#define OTA_WIFI_SSID "your-network"
#define OTA_WIFI_PASSWORD "your-wifi-password"
#define OTA_PASSWORD "a-long-unique-ota-password"
```

Without all three settings, Wi-Fi is not started and the costume works offline.
With settings present, the ESP32 joins that network and enables password-
protected ArduinoOTA. This is **Wi-Fi OTA only**, not Bluetooth firmware update.

If auto-upload picks the wrong port, set it explicitly in `platformio.ini`:

```ini
upload_port = /dev/ttyUSB0      # or COM3 on Windows, /dev/cu.usbserial-* on macOS
```

## What remains unverified

- [ ] Exact ESP32 board variant (defaults assume generic `esp32dev`).
- [ ] Physical pinout / wiring (defaults are safe-but-assumed).
- [ ] Actual LED counts per section (configured as 29 each).
- [ ] Measured current and physical operation of the 1,500 mA limiter.
- [ ] BLE operation and the LightBlue test on a physical iPhone.
- [ ] Optional buttons and Wi-Fi OTA on the target hardware.

Do not treat the above as validated until you flash and observe the board.

## License

Personal project — no license selected yet.

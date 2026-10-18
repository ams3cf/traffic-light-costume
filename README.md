# Traffic Light Costume

A wearable traffic-light costume for a child, driven by an **ESP32** and three
independently-controlled **WS2812B LED rings** (red / yellow / green).

The firmware boots into a **startup self-test** (lights each ring in turn so a
mis-wired ring is obvious), then runs a **non-blocking** US-style traffic
sequence: `GREEN -> YELLOW -> RED -> GREEN ...`.

> **Status: firmware baseline, NOT hardware-validated.** This code compiles
> cleanly with PlatformIO but has not been flashed or tested against physical
> hardware. Board variant, pinout, LED counts, and power design are documented
> *assumptions* below — verify and adjust `src/config.h` to match your build.

---

## Features

- Three WS2812B rings driven independently via FastLED (one data pin each).
- Centralized configuration — every pin, LED count, brightness, and timing value
  lives in `src/config.h`, not scattered through the code.
- Non-blocking sequence (millis()-based state machine; no `delay()`).
- Startup self-test to catch wiring mistakes before the show.
- Conservative brightness cap (eye-safe for a child, current-safe for batteries).

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

| Setting            | Default            | Meaning                                   |
|--------------------|--------------------|-------------------------------------------|
| `PIN_RED`          | `16`               | Data pin for the red ring                 |
| `PIN_YELLOW`       | `17`               | Data pin for the yellow ring              |
| `PIN_GREEN`        | `18`               | Data pin for the green ring               |
| `LED_COUNT_RED`    | `8`                | LEDs in the red ring                      |
| `LED_COUNT_YELLOW` | `8`                | LEDs in the yellow ring                   |
| `LED_COUNT_GREEN`  | `8`                | LEDs in the green ring                    |
| `BRIGHTNESS_MASTER`| `80`               | Master brightness, 0–255 (~31%)           |
| `TIMING_GREEN_MS`  | `5000`             | Green phase duration                      |
| `TIMING_YELLOW_MS` | `2000`             | Yellow phase duration                     |
| `TIMING_RED_MS`    | `5000`             | Red phase duration                        |
| `SELFTEST_STEP_MS` | `600`              | Per-ring self-test dwell                  |

---

## Wiring assumptions (UNVERIFIED — confirm before soldering)

Each WS2812B ring needs three connections: **5V**, **GND**, and **DATA IN**
(the data-out pad is for daisy-chaining and is left unconnected here).

| Ring   | ESP32 pin (default) | Notes                                          |
|--------|---------------------|------------------------------------------------|
| Red    | GPIO16              | Output-capable, no boot-strapping              |
| Yellow | GPIO17              | Output-capable, no boot-strapping              |
| Green  | GPIO18              | Output-capable, no boot-strapping              |

The default pins deliberately avoid ESP32 strapping pins (GPIO 0, 2, 5, 12, 15),
the flash pins (GPIO 6–11), input-only pins (GPIO 34–39), and the UART0 console
(GPIO 1, 3). Most ESP32 DevKit / NodeMCU-32S boards expose all three chosen pins
on the header. If your board routes a different set, change `src/config.h`.

**Recommended wiring order per ring:**
1. Common **GND** between the ESP32, the LED ring, and the power source.
2. **5V** to the ring's power pad.
3. **DATA IN** to the ESP32 GPIO through a ~330 Ω series resistor (protects the
   first LED from overvoltage/ringing on the data line).
4. A 100–1000 µF electrolytic capacitor across the power rail at the ring helps
   absorb current spikes when the LEDs switch.

## Power budget (assumptions, not measurements)

WS2812B can draw up to **~60 mA per LED** at full white. With the defaults
(3 × 8 = 24 LEDs, brightness capped at 80/255 ≈ 31%) the worst case is roughly
`24 × 60 mA × 0.31 ≈ 450 mA`, well within a USB power bank (≥1 A) or a single
18650 cell via a buck/boost module.

If you raise `LED_COUNT_*` or `BRIGHTNESS_MASTER`, re-check the supply. At full
brightness 24 LEDs would want ~1.4 A — do not run that from a bare LiPo without
adequate cell protection and wiring gauge.

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
git clone https://github.com/ams3cf/traffic-light-costume.git
cd traffic-light-costume

pio run              # compile (first run downloads the ESP32 toolchain + FastLED)
pio run -t upload    # flash over USB (edit upload_port if auto-detect fails)
pio device monitor   # open serial monitor (115200 baud) to see boot logs
```

If auto-upload picks the wrong port, set it explicitly in `platformio.ini`:

```ini
upload_port = /dev/ttyUSB0      # or COM3 on Windows, /dev/cu.usbserial-* on macOS
```

## What remains unverified

- [ ] Exact ESP32 board variant (defaults assume generic `esp32dev`).
- [ ] Physical pinout / wiring (defaults are safe-but-assumed).
- [ ] Actual LED counts per ring (defaults are 8 each).
- [ ] Real current draw and power-supply headroom.
- [ ] On-hardware behavior of the self-test and sequence.

Do not treat the above as validated until you flash and observe the board.

## License

Personal project — no license selected yet.

# Traffic Light Costume

A wearable traffic-light costume driven by an **ESP-WROOM-32** and an 87-pixel
daisy-chained WS2812B LED chain divided into bottom, middle, and top sections.
The costume runs its own Wi-Fi network and serves a small control page; four
physical buttons work alongside it, and lighting never depends on a phone.

> **Status:** LED output, the Wi-Fi access point, and the full web API are
> verified on the bench (3-pixel chain). Physical buttons, the 87-pixel chain,
> current draw, and phone behavior on the no-gateway network still need
> validation. Do not describe those as field-tested.

---

## Features

- Three modes: **Auto** (traffic cycle), **Manual** (one chamber, chosen by
  buttons or phone), **Effects** (pick one of 13 effects from a
  dropdown, or cycle through them all, 15 s each).
- WPA2 access point `TrafficLight` with a one-page control UI and a JSON status
  API, so button presses show up on every connected phone.
- Lights and web server on separate cores; they share only a command queue and a
  status snapshot, so web traffic cannot stall animation.
- Four active-low buttons: mode, red, yellow, and green.
- Global 1,500 mA FastLED power limiter.

## Repository layout

```
traffic-light-costume/
├── platformio.ini       # bench / production / native-test environments
├── docs/WEB_API.md      # Wi-Fi settings, endpoints, status format, modes
├── src/
│   ├── config.h         # ALL hardware/timing/brightness/Wi-Fi knobs
│   ├── main.cpp         # starts both cores
│   ├── state.{h,cpp}    # modes and commands (pure C++, unit-tested)
│   ├── control.{h,cpp}  # command queue + status snapshot between cores
│   ├── lights.{h,cpp}   # core 1: modes, timing, FastLED output
│   ├── effects.{h,cpp}  # effects-mode playlist
│   ├── buttons.{h,cpp}  # debounced physical buttons
│   ├── web.{h,cpp}      # core 0: access point + HTTP API
│   └── web_page.h       # the control page (served from flash)
└── test/test_state/     # host unit tests for state.cpp
```

## Configuration

Everything tunable is in [`src/config.h`](src/config.h):

| Setting | Default | Meaning |
|---|---:|---|
| `PIN_LED_DATA` | `16` (`RX2` on this board) | Data pin for the full LED chain |
| `PIN_STATUS_LED` | `2` | Onboard status LED (board-dependent) |
| `PIN_MODE_BUTTON` | `18` | MODE button to ground |
| `PIN_RED_BUTTON` | `19` | RED button to ground |
| `PIN_YELLOW_BUTTON` | `21` | YELLOW button to ground |
| `PIN_GREEN_BUTTON` | `22` | GREEN button to ground |
| `LED_COUNT_PER_SECTION` | `29` (bench: `1`) | LEDs per section |
| `LED_CURRENT_BUDGET_MA` | `1500` | Global FastLED current limit |
| `DEFAULT_BRIGHTNESS_PERCENT` | `31` | Startup brightness, 0-100 |
| `EFFECT_STEP_MS` | `15000` | Time per effect in Effects mode |
| `WIFI_AP_SSID` / `WIFI_AP_PASSWORD` | `TrafficLight` / `greenlight` | Costume network; override in `src/secrets.h` |
| Section order | bottom, middle, top | Physical order from the ESP32 data input |

See [docs/WEB_API.md](docs/WEB_API.md) for the endpoints and status format.

---

## Wiring

The full WS2812B chain needs **5V**, **GND**, and one **DATA IN** connection.
Connect the ESP32 data pin to the bottom section's input, then connect each
section's data output to the next section's input in this order: bottom, middle,
top. The data output of the top section is left unconnected.

| Section | LED indexes | Signal color | Notes |
|---------|-------------|--------------|-------|
| Bottom  | 0-28        | Green        | First section connected to ESP32 data |
| Middle  | 29-57       | Yellow       | Chained after bottom |
| Top     | 58-86       | Red          | Chained after middle |

The current pin map targets the ESP-WROOM-32 module. On the user's carrier
board, GPIO16 is labelled `RX2`; it is used as LED data, not as the USB serial
console. GPIO18/19/21/22 are the four buttons, each wired active-low to ground
with internal pull-ups. Avoid tying the LED supply's +5V to the ESP32 USB +5V.

**Recommended LED wiring:**
1. Connect common **GND** between ESP32, all LED sections, and the LED supply.
2. Power each 29-LED chamber in parallel from the regulated 5V LED supply; chain
  only the data line bottom -> middle -> top.
3. Connect GPIO16 through the 330-ohm series resistor to the first DIN.
4. The wiring plan calls for a 1000uF / 10V-or-higher electrolytic across each
  chamber supply input; observe capacitor polarity.
5. Do not connect the separate USB +5V rails together. Share ground only.

The main LED-feed fuse is still a proposal. Select its rating against wire
ampacity and the power-bank characteristics before use. The separate ESP32
USB supply and LED supply must share ground for data signaling.

## Power budget (assumptions, not measurements)

The firmware sets FastLED's global estimated output ceiling to **1,500 mA**.
This is a software limiter, not hardware overcurrent protection, and actual
WS2812B current varies by pixel batch, color, voltage, and temperature. Use a
regulated 5 V supply sized with suitable headroom and wiring; validate current
with a meter before wearing the costume.

Changing the LED count, supply voltage, or power limit requires a new current
check. The limiter cannot protect against wiring faults or a supply short.

**Do not power the LEDs from the ESP32's 3.3 V rail.** ESP32 data output is
3.3V; verify signal integrity with the actual strip and wiring. A level shifter
may be needed if the first pixel does not reliably read the signal.

## Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI):
  ```bash
  pipx install platformio
  # or, if you prefer a venv:
  # python3 -m venv ~/pio && ~/pio/bin/pip install platformio
  ```
- A USB-to-serial connection to the ESP32 (most DevKits expose one via their
  onboard USB-UART bridge).

## Build, test & upload

```bash
pio test -e native                 # host unit tests for the state logic
pio run -e bench -e production     # build both firmware profiles
pio run -e bench -t upload         # 3-pixel bench chain, one pixel per chamber
pio run -e production -t upload    # 87-pixel costume chain, serial debug off
pio device monitor                 # serial monitor (115200 baud)
```

If auto-upload picks the wrong port, add `upload_port = /dev/ttyUSB0` to the
environment in `platformio.ini`.

FastLED is pinned to **3.7.8**. With `^3.7.0`, PlatformIO resolved 3.10.8,
which on this Arduino-ESP32 2.0.x core drove a WS2812B test chain solid white
regardless of the commanded color; 3.7.8 and Adafruit NeoPixel 1.12.3 both
drove the same wiring correctly.

## Using it

1. Power the costume. The onboard LED blinks slowly while waiting for a phone
   and stays on once one has joined.
2. On the phone, join Wi-Fi `TrafficLight` (password `greenlight`) and open
   `http://192.168.4.1`. Bookmark it or add it to the home screen.
3. Tap a lamp to light that chamber (switches to Manual), use the
   Auto / Manual / Effects toggle, and set brightness with the slider. In
   Effects mode, the dropdown picks one effect to hold or **Cycle all**.

The network offers no internet gateway, so phones should keep using cellular
data while joined. That behavior is confirmed on Linux but not yet on a phone.

### Buttons

- MODE short press cycles Auto → Manual → Effects. Hold for one second to
  return to Auto.
- RED, YELLOW, and GREEN switch to Manual with that chamber lit, acting on the
  press edge. Entering Manual from MODE shows the last chosen color.
- Inputs are active-low with internal pull-ups and 40 ms debounce.

### Why not Bluetooth

BLE was removed in favor of Wi-Fi. On this board, connections failed or took
15 s+ from both a Linux host and a phone, with NimBLE and with the core's
Bluedroid stack, and with no LED code running; advertising and discovery
worked. The same radio serves the Wi-Fi access point reliably (492/492 status
polls in a 2-minute soak test).

## Handoff: verified and pending

Verified on the bench (ESP-WROOM-32 DevKit, 3-pixel chain on GPIO16):

- FastLED 3.7.8 drives red/green/blue correctly (3.10.8 did not).
- Access point starts; a Linux client joins, receives `192.168.4.x` with no
  default route, and its wired internet route is unaffected.
- Every endpoint in [docs/WEB_API.md](docs/WEB_API.md), including error
  responses; status reflects each command on the next request. Page is about
  3 KB; status polls take about 30 ms.
- Two-minute soak in Effects mode: 492/492 polls succeeded, no reboot, all 13
  effects cycled.
- `pio test -e native`: 13/13 state tests pass.
- Effects dropdown: a picked effect holds past the 15 s rotation; Cycle all
  resumes from it.

Still pending; do not claim validated until performed:

- Join from the actual phone; confirm the page works and cellular data keeps
  working while joined.
- Physical buttons, including MODE hold and the UI updating after a press.
- Effects as they actually look on the 87-pixel chain inside the chambers.
- Measure supply current and verify the 1,500 mA software limit under load;
  choose the proposed fuse from the actual wiring and supply.
- Confirm GPIO2 status LED polarity on this carrier board.

## License

Personal project — no license selected yet.

# Traffic Light Costume

A wearable traffic-light costume driven by an **ESP-WROOM-32** and an 87-pixel
daisy-chained WS2812B LED chain divided into bottom, middle, and top sections.
The costume runs its own Wi-Fi network and serves a small control page; four
physical buttons and a small OLED work alongside it, and lighting never
depends on a phone. A home Wi-Fi mode allows firmware updates over the air.

Parts, pin map, wiring, and dimensions: [docs/HARDWARE.md](docs/HARDWARE.md).

> **Status:** LED output, the Wi-Fi hotspot, the web API, the button wiring,
> the OLED, home Wi-Fi mode, and OTA updates are verified on the bench (3-pixel
> chain). Per-mode button behavior, hotspot fallback, the 87-pixel chain, and
> current draw still need validation. Do not describe those as field-tested.

---

## Features

- Three modes: **Auto** (traffic cycle), **Manual** (one chamber, chosen by
  buttons or phone), **Effects** (pick one of 13 effects from a
  dropdown, or cycle through them all, 15 s each).
- WPA2 access point `TrafficLight` with a one-page control UI and a JSON status
  API, so button presses show up on every connected phone.
- Lights and web server on separate cores; they share only a command queue and a
  status snapshot, so web traffic cannot stall animation.
- Four normally-closed buttons on a common ground: mode, red, yellow, and green.
- 0.91" OLED showing mode, speed, brightness, button hints, and Wi-Fi details.
- Home Wi-Fi mode (hold MODE + GREEN for 3 s) with OTA firmware updates; falls back to the hotspot if home Wi-Fi is out of range.
- Global 1,500 mA FastLED power limiter.

## Repository layout

```
traffic-light-costume/
├── platformio.ini       # bench / production / native-test environments
├── docs/HARDWARE.md     # parts, pin map, wiring, power, dimensions
├── docs/WEB_API.md      # Wi-Fi settings, endpoints, status format, modes
├── src/
│   ├── config.h         # ALL hardware/timing/brightness/Wi-Fi knobs
│   ├── main.cpp         # starts both cores
│   ├── state.{h,cpp}    # modes and commands (pure C++, unit-tested)
│   ├── control.{h,cpp}  # command queue + status snapshot between cores
│   ├── lights.{h,cpp}   # core 1: modes, timing, FastLED output
│   ├── effects.{h,cpp}  # effects-mode playlist
│   ├── buttons.{h,cpp}  # reads the buttons, dispatches gestures
│   ├── gestures.{h,cpp} # debounce, tap/hold, MODE chords (pure C++, unit-tested)
│   ├── network.{h,cpp}  # hotspot / home Wi-Fi mode, OTA
│   ├── display.{h,cpp}  # OLED status display (core 0, read-only)
│   ├── web.{h,cpp}      # core 0: HTTP UI and API
│   ├── web_page.h       # the control page (served from flash)
│   └── secrets.example.h # template for the git-ignored src/secrets.h
└── test/                # host unit tests (state, gestures)
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
| `PIN_OLED_SDA` / `PIN_OLED_SCL` | `32` / `33` | OLED status display (I2C) |
| `BUTTON_PRESSED_LEVEL` | `HIGH` | Pin level while pressed (`HIGH` = normally-closed, `LOW` = normally-open) |
| `LED_COUNT_PER_SECTION` | `29` (bench: `1`) | LEDs per section |
| `LED_CURRENT_BUDGET_MA` | `1500` | Global FastLED current limit |
| `DEFAULT_BRIGHTNESS_PERCENT` | `31` | Brightness until one is set; after that the saved value is used |
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
console. GPIO18/19/21/22 are the four buttons. They are normally-closed, with
one side of each on a shared ground wire (any ESP32 GND pin) and internal
pull-ups, so no resistors are needed. Avoid tying the LED supply's +5V to the ESP32 USB +5V.

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
pio test -e native                 # host unit tests (state, gestures)
pio run -e bench -e production     # build both firmware profiles
pio run -e bench -t upload         # 3-pixel bench chain, one pixel per chamber
pio run -e production -t upload    # 87-pixel costume chain, serial debug off
pio device monitor                 # serial monitor (115200 baud)
```

If auto-upload picks the wrong port, add `upload_port = /dev/ttyUSB0` to the
environment in `platformio.ini`. For uploads over Wi-Fi, see
[Home Wi-Fi mode and OTA updates](#home-wi-fi-mode-and-ota-updates).

FastLED is pinned to **3.7.8**. With `^3.7.0`, PlatformIO resolved 3.10.8,
which on this Arduino-ESP32 2.0.x core drove a WS2812B test chain solid white
regardless of the commanded color; 3.7.8 and Adafruit NeoPixel 1.12.3 both
drove the same wiring correctly.

## Using it

1. Power the costume. The onboard LED blinks slowly while waiting for a phone
   and stays on once one has joined.
2. On the phone, join Wi-Fi `TrafficLight` (password `greenlight`) and open
   `http://192.168.4.1`. Bookmark it or add it to the home screen.
3. Use the Auto / Manual / Effects toggle and the brightness slider. In
   Manual, tap a lamp to light that chamber (lamps are inactive in the other
   modes, where the hardware color buttons adjust speed). In Effects, the
   dropdown picks one effect to hold or **Cycle all**. The current speed is
   shown under the toggle. Brightness is saved on the board, so it also
   applies to the next power-up, including the boot self-test.

The network offers no internet gateway, so phones should keep using cellular
data while joined. That behavior is confirmed on Linux but not yet on a phone.

### Buttons

- MODE tap cycles Auto → Manual → Effects (on release). Hold for one second
  to return to Auto.
- RED, YELLOW, and GREEN depend on the mode. "Hold" means one second, then
  repeating every 0.4 s until released:

  | Mode | GREEN | RED | YELLOW |
  |---|---|---|---|
  | Auto | Faster cycle (press or hold) | Slower cycle (press or hold) | Normal speed |
  | Manual | Light green | Light red | Light yellow |
  | Effects | Tap: next effect. Hold: faster animation | Tap: previous effect. Hold: slower animation | Normal speed |

- Speeds run from 0.4× to 2.5× in seven steps; Auto and Effects keep separate
  speeds. Entering Manual from MODE shows the last chosen color.
- Buttons are normally-closed to ground with internal pull-ups: a pin rests
  LOW and reads HIGH while pressed. 40 ms debounce.
- MODE works as a shift key: hold MODE + GREEN for 3 s to switch network
  mode (see below). While MODE is held, other buttons do nothing else.
- Each pin's level at boot counts as "released". An unplugged or broken button
  reads as permanently pressed, so it is ignored (serial logs
  `button <name> reads pressed at boot`) rather than firing. A button held
  during power-up only acts after it is released and pressed again.

### Status display (OLED)

A 0.91" 128×32 SSD1306 I2C OLED shows the wearer what the costume is doing,
what the buttons do right now, and how a friend can join the Wi-Fi, so nobody
needs a lookup table.

```
EFFECTS            1.35x     <- mode, then speed (manual: the color)
fire  cycling        31%     <- what is showing, brightness
tap G next  R previous       <- rotates every 3 s: button hints for this
                                mode, MODE hint, network chord hint, Wi-Fi
                                name / password (or home network), address
                                and phones connected (or OTA on/off)
```

**Wiring.** The module header is GND, VCC, SCL, SDA.

| OLED pin | ESP32 |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL | GPIO33 (`D33`) |
| SDA | GPIO32 (`D32`) |

GPIO21/22, the ESP32's usual I2C pins, are taken by the YELLOW and GREEN
buttons. The module's schematic shows its I2C pull-ups (4.7 kΩ) go to its own
3.3 V regulator output, so SDA/SCL stay at 3.3 V even if VCC is 5 V. Typical
draw is 6-10 mA with text, about 20 mA worst case; negligible next to the
ESP32's Wi-Fi (~150 mA) and the LEDs (up to 1,500 mA).

**Behavior.**

- Runs in its own low-priority task on core 0 and only reads the status
  snapshot, so slow I2C redraws cannot stutter the lights. Redraws only when
  the text changes (checked every 200 ms).
- Can be plugged in or unplugged while running: the task probes address 0x3C
  every 2 s and re-initializes the display when it appears. Serial logs
  `OLED found at 0x3C`.
- Dims to minimum contrast after 30 s with no state change, to limit OLED
  burn-in. Any state change (mode, effect, speed, brightness) brightens it.
  Rotating hints alone do not keep it bright.
- If the access point fails to start, the bottom line reads
  `WI-FI FAILED: restart`.
- The onboard GPIO2 LED still blinks while waiting for a phone and stays on
  once one has joined (bench indicator).

### Home Wi-Fi mode and OTA updates

The costume has two network modes:

| Mode | When | Reach the page at | OTA |
|---|---|---|---|
| **Hotspot** (default) | Out and about | Join `TrafficLight`, open `http://192.168.4.1` | Off |
| **Home Wi-Fi** | At home, for updates | `http://trafficlight.local` or the IP on the OLED | On |

**Setup (once).** Copy the template and fill in your home network.
`src/secrets.h` is git-ignored; never commit it.

```bash
cp src/secrets.example.h src/secrets.h
$EDITOR src/secrets.h      # HOME_WIFI_SSID, HOME_WIFI_PASSWORD
pio run -e production -t upload   # one USB flash so the costume knows them
```

**Switching.** Hold **MODE + GREEN for 3 s** (press MODE first or both
together). The OLED shows `restart: home Wi-Fi` (or `restart: hotspot`) and
the board restarts into the other mode, about 2 s with the lights blank. The
choice is saved, so it survives restarts and OTA updates. While MODE is held,
the color buttons do nothing else, so the chord never changes speed or color.

If home Wi-Fi cannot be joined within 15 s (out of range, router down), the
hotspot starts for that boot and the OLED shows `home Wi-Fi not found`. The
saved choice stays home Wi-Fi, so the next boot tries again; use the chord to
switch back to hotspot permanently. The lights run normally the whole time.

**Uploading over the air.** In home Wi-Fi mode:

```bash
pio run -e production_ota -t upload           # or bench_ota on the bench
pio run -e production_ota -t upload --upload-port 192.168.1.42  # if .local fails
```

The OLED shows `UPDATING nn%` during the upload, then the board restarts into
home Wi-Fi mode with the new firmware. OTA has no password: anyone on the home
network can upload while the costume is in home Wi-Fi mode, and it is never
reachable in hotspot mode.
`trafficlight.local` needs mDNS on the laptop (Avahi with `nss-mdns` on Linux);
otherwise use the IP shown on the OLED.

**Firewall.** During an upload the costume opens a TCP connection *back* to
the computer on port 40100 (`--host_port` in `platformio.ini`). A firewall
that blocks it makes the upload stop at `Waiting for device...` / `No response
from device`, and the OLED shows `OTA failed: can't reach PC`. With `ufw`
(Omarchy's default), allow it once for the home subnet:

```bash
sudo ufw allow proto tcp from 10.1.0.0/22 to any port 40100 comment 'traffic light OTA'
```

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
- `pio test -e native`: 29/29 tests pass (state and gestures).
- OLED found at 0x3C and showing all three lines (user confirmed).
- Hotspot boot with the network-mode code: no saved preference, starts the
  hotspot, no NVS errors.
- Home Wi-Fi mode: the MODE + GREEN chord switched to home Wi-Fi; the saved
  choice survived a USB flash; joined in about 4 s and served the API at
  `trafficlight.local`.
- OTA: `pio run -e bench_ota -t upload` (884 KB) took about 30 s, reported
  `Result: OK`, and the board restarted (`SW_CPU_RESET`) and rejoined home
  Wi-Fi within 2 s. Requires the `ufw` rule for port 40100.
- Effects dropdown: a picked effect holds past the 15 s rotation; Cycle all
  resumes from it.

Still pending; do not claim validated until performed:

- Join from the actual phone; confirm the page works and cellular data keeps
  working while joined.
- Per-mode button behavior (speed, effect stepping, MODE hold) and the page
  updating after a press. Normally-closed wiring itself is confirmed: all four
  buttons read released at boot.
- OLED dimming after 30 s and hot-plug on the real module.
- Home Wi-Fi mode: chord back to hotspot, and fallback to the hotspot when
  home Wi-Fi is out of range.
- OTA of the `production` build into the costume.
- Effects as they actually look on the 87-pixel chain inside the chambers.
- Measure supply current and verify the 1,500 mA software limit under load;
  choose the proposed fuse from the actual wiring and supply.
- Confirm GPIO2 status LED polarity on this carrier board.

## License

Personal project — no license selected yet.

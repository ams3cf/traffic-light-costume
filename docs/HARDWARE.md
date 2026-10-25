# Hardware

What is in the costume, how it is wired, and what each part needs. Values
marked **measured** or **verified** were confirmed on the bench. Values marked
**listing** come from the seller's listing or schematic and have not been
measured. Fill in the blank dimensions with calipers before designing mounts.

## Bill of materials

| Part | Qty | Notes |
|---|---:|---|
| ESP32 DevKit, 30-pin, ESP-WROOM-32 | 1 | CP2102 USB-UART (**verified**), micro-USB |
| WS2812B addressable LEDs | 87 (bench: 3) | Three 29-LED chambers in one chain |
| Momentary pushbuttons, normally-closed | 4 | MODE, RED, YELLOW, GREEN; pigtails with a common ground |
| 0.91" 128×32 OLED, SSD1306, I2C | 1 | White; sold as a 2-pack |
| 330 Ω resistor | 1 | Series resistor on LED data |
| 1000 µF ≥10 V electrolytic | 3 | One per chamber supply input (planned) |
| 5 V LED supply | 1 | Separate from the ESP32's USB supply; shares ground only |

## ESP32 DevKit

| Item | Value |
|---|---|
| Module | ESP-WROOM-32 (no PSRAM, so GPIO16/17 are free) |
| USB-UART | Silicon Labs CP2102 (**verified**: Linux `cp210x` driver, `/dev/ttyUSB0`) |
| Flash | 4 MB, default partitions: two 1.25 MB app slots (OTA) |
| Onboard LED | GPIO2, blue, used as a network indicator |
| Buttons | EN (reset), BOOT (GPIO0) |
| Pinout reference | [listing image](https://m.media-amazon.com/images/I/71TbFu6PnoL._AC_SL1001_.jpg) |
| Board size (L × W × H) | _to measure_ |
| Header row spacing | _to measure_ |
| Mounting holes | _to measure_ |

### Pin map

| GPIO | Board label | Use | Notes |
|---:|---|---|---|
| 16 | `RX2` | LED data → 330 Ω → first pixel DIN | **verified** |
| 18 | `D18` | MODE button | NC to common ground, internal pull-up |
| 19 | `D19` | RED button | NC to common ground, internal pull-up |
| 21 | `D21` | YELLOW button | NC to common ground, internal pull-up |
| 22 | `D22` | GREEN button | NC to common ground, internal pull-up |
| 32 | `D32` | OLED SDA | I2C, 400 kHz |
| 33 | `D33` | OLED SCL | I2C |
| 2 | `D2` | Onboard LED | Network indicator |
| — | `3V3` | OLED VCC | |
| — | `GND` | Buttons common, OLED GND, LED supply GND | |

Free and output-capable for future use: GPIO4, 5\*, 13, 14, 17, 23, 25, 26,
27. (\*GPIO5 is a strapping pin; avoid driving it at boot.) Avoid GPIO0, 2,
12, 15 (strapping), 6-11 (flash), 34-39 (input only), and 1/3 (USB serial).

## LEDs

| Item | Value |
|---|---|
| Type | WS2812B, GRB order, 800 kHz |
| Chain | Bottom (green) 0-28 → middle (yellow) 29-57 → top (red) 58-86 |
| Bench chain | 3 pixels, one per chamber (`bench` build) |
| Data | 3.3 V from GPIO16 through 330 Ω; works reliably with 5 V pixel power (**verified** on the bench chain) |
| Driver | FastLED 3.7.8 (3.10.x drives this chain solid white) |
| Current | Software-limited to 1,500 mA total; measure before wearing |

## Buttons

| Item | Value |
|---|---|
| Type | Momentary, **normally-closed** |
| Wiring | One side of each to its GPIO, other sides joined on one common ground pigtail |
| Logic | Rests LOW, reads HIGH while pressed (`BUTTON_PRESSED_LEVEL HIGH`) |
| Verified | All four read released at boot with pigtails connected |
| Fault behavior | A broken wire reads as permanently pressed; firmware ignores any button pressed at boot |

## OLED status display

| Item | Value |
|---|---|
| Panel | 0.91", 128 × 32, white, SSD1306 (listing) |
| Interface | I2C at address 0x3C (**verified**: `OLED found at 0x3C`) |
| Supply | 3.3-5 V (listing); wired to `3V3` |
| Header | GND, VCC, SCL, SDA (listing schematic) |
| On-board parts | 662K 3.3 V LDO; 4.7 kΩ SDA/SCL pull-ups to the LDO output, so I2C stays at 3.3 V even on 5 V (listing schematic) |
| Current | About 6-10 mA with text, about 20 mA all pixels lit, about 10 µA asleep (typical for SSD1306 modules) |
| Module PCB size | About 38 × 12 mm (typical for this module, listing class); _measure_ |
| Active area | About 22.4 × 5.6 mm (typical 0.91" SSD1306 panel); _measure_ |
| Thickness / mounting holes | _to measure_ |
| References | [schematic image](https://m.media-amazon.com/images/I/61DnWOHZ78L._AC_SL1200_.jpg) |

## Power

| Load | Supply | Typical | Peak |
|---|---|---|---|
| ESP32 with Wi-Fi | ESP32 USB | ~120-180 mA | 250+ mA |
| OLED | ESP32 3V3 | ~10 mA | ~20 mA |
| LED chain | 5 V LED supply | depends on effect | 1,500 mA (software limit) |

Keep the ESP32's USB supply and the LED supply separate; join their grounds
only.

### Bench testing the chain from USB

For wiring checks, the 87 LEDs can run from the ESP32's USB supply (`VIN`)
at low brightness, with the LED supply's +5 V disconnected. Budget: 500 mA
USB − ~250 mA ESP32 peak − ~87 mA LED idle (about 1 mA per WS2812B even when
dark) leaves about 160 mA; plan for about 100 mA. Each color channel is about
20 mA at full brightness (estimates, not measured):

| Pattern | At 100% | Brightness for ~100 mA |
|---|---:|---:|
| One chamber green or red (29 LEDs) | ~580 mA | ~17% |
| One chamber yellow (red + green) | ~1,160 mA | ~8% |
| Rainbow effects across 87 | ~1,740 mA | ~5% |
| All 87 white (no effect does this) | ~5,220 mA | ~2% |

Set brightness to 5% or less **before** connecting the chain. Brightness is
saved, so the boot self-test and every mode stay at that level after a
power cycle. Do not power LEDs from the ESP32's 3.3 V rail. The main LED-feed fuse
is still to be chosen against the wire gauge and supply.

## Wiring overview

```
               ESP32 DevKit (30-pin)
             +----------------------+
  LED 5V --- |                      |
  supply     | GPIO16 (RX2) --330Ω--+--> DIN bottom -> middle -> top
  GND -------+ GND                  |
             | GPIO18 ---- MODE   --+
             | GPIO19 ---- RED    --+  NC buttons,
             | GPIO21 ---- YELLOW --+  common ground pigtail
             | GPIO22 ---- GREEN  --+
             | GND ----------------- common ground
             | GPIO32 ---- OLED SDA |
             | GPIO33 ---- OLED SCL |
             | 3V3 ------- OLED VCC |
             | GND ------- OLED GND |
             +----------+-----------+
                        |
                   micro-USB (ESP32 supply, flashing, serial log)
```

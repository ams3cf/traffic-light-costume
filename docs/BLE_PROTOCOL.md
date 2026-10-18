# BLE GATT Protocol

Protocol version: `1`  
Advertised name: `TrafficLightCostume`  
Transport: BLE GATT, UTF-8/ASCII text values, no binary packet framing  
Firmware version: `1.0.0`

## Service and UUIDs

All UUIDs are stable custom 128-bit UUIDs. Characteristic user descriptions (GATT descriptor `0x2901`) name each control in LightBlue.

| Attribute | UUID | Properties | Value format |
|---|---|---|---|
| Traffic Light Costume service | `9d4a0001-5f5b-4c1a-9f1a-7c2b6e8d0001` | Primary service | - |
| Command | `9d4a0002-5f5b-4c1a-9f1a-7c2b6e8d0002` | Write with response | One command listed below |
| Mode | `9d4a0003-5f5b-4c1a-9f1a-7c2b6e8d0003` | Read, write with response | Mode token |
| Effect | `9d4a0004-5f5b-4c1a-9f1a-7c2b6e8d0004` | Read, write with response | Effect token |
| Brightness | `9d4a0005-5f5b-4c1a-9f1a-7c2b6e8d0005` | Read, write with response | Decimal integer `0`-`100` |
| Device State | `9d4a0006-5f5b-4c1a-9f1a-7c2b6e8d0006` | Read, notify | ASCII key/value state, maximum 160 bytes |
| Device Info | `9d4a0007-5f5b-4c1a-9f1a-7c2b6e8d0007` | Read | `fw=1.0.0;protocol=1;ota=wifi-only` |

## Values

Write plain text directly in LightBlue. Input is case-sensitive, has no trailing newline, and is limited to 23 bytes. The GATT attributes are sized to bound incoming text; oversized writes are rejected by ATT. The firmware validates the complete value before dispatching it. Invalid tokens do not change the selected mode/effect/brightness; the State value's `result` field reports the error.

Persistent Mode values:

- `traffic`
- `halloween`
- `cyberpunk`
- `party`
- `demo`
- `off`

Selecting a mode cancels any active one-shot effect. An effect returns to the mode that was active when it started.

One-shot Effect values:

- `reactor`
- `haunted`
- `roulette`
- `celebration`
- `color_storm`
- `incandescent`
- `triple_orbit`
- `showcase`

Effects play for a bounded duration (18 seconds) and return to the previous mode. `showcase` runs the implemented effects in sequence. Writing an unsupported token, including `none`, reports `error:bad_effect`; use Mode to select a persistent mode.

Command values:

- `stop`: turn LEDs off, retaining the previous non-off mode for resume.
- `resume`: resume the retained mode (or restart the current mode if already active).
- `next`: advance to the next persistent mode in the order listed above, wrapping to `traffic`.
- `status`: refresh the state notification/readback.

Command write responses acknowledge the GATT write; inspect Device State for application-level validation/result because ATT write-with-response itself is not used to encode semantic command errors.

Brightness is a decimal ASCII integer from `0` through `100`, inclusive. The firmware additionally limits estimated LED output to the configured 1,500 mA global budget using FastLED's power limiter. This is a software estimate, not a substitute for a properly sized supply or physical current measurement.

## Device State

Device State is a semicolon-separated ASCII record. Current fields:

`mode=<mode>;effect=<effect>;brightness=<0-100>;power=limit_1500mA;optical=diffused;fw=<version>;protocol=<version>;result=<result>`

Example:

`mode=halloween;effect=haunted;brightness=40;power=limit_1500mA;optical=diffused;fw=1.0.0;protocol=1;result=ok`

The power profile names the 1,500 mA FastLED software limit. `optical=diffused` is the configured optical profile label; there is no optical hardware switching in this firmware. The value is kept below the 160-byte characteristic limit. The firmware notifies connected clients after a state change and when a client connects. Disconnecting does not change the selected mode or stop animation; the device continues autonomously and advertises again for reconnects.

`result` is `ok` on accepted operations, or one of `error:bad_mode`, `error:bad_effect`, `error:bad_brightness`, `error:bad_command`, `error:invalid_write`, or `error:use_mode`.

## Buttons

Both buttons are optional; firmware enables the internal pull-up and works when either switch is absent. Wire the switch between its input and ground.

| Input | Short press | Long press (1 second) |
|---|---|---|
| GPIO18, Mode | Cycle persistent modes | Return directly to Traffic mode |
| GPIO19, Action | Trigger current mode's signature effect | Run the showcase sequence |

Button actions and BLE writes use the same main-loop dispatcher. The GPIO18 long-press path remains available even when BLE is disconnected or inaccessible.

## Access Control and OTA

The BLE GATT service is intentionally open: no pairing, bonding, encryption, or application-level authentication is required, so LightBlue can operate it immediately. Any nearby BLE central can read the state and issue lighting writes while in range. Do not deploy this interface where unauthorized nearby control is a concern. Physical return-to-Traffic remains available through the GPIO18 long press.

BLE is not used to update firmware. OTA is Wi-Fi-only through ArduinoOTA and remains disabled until credentials are configured in an ignored `src/secrets.h` file. OTA uses its own password; do not reuse BLE controls as an update mechanism.

## Reproducible LightBlue Test (iOS)

This is a test procedure, not a claim that an iPhone test has passed. A physical iPhone running LightBlue and a flashed ESP32 are required to verify radio behavior.

1. Open LightBlue by Punch Through on iOS.
2. Scan for `TrafficLightCostume` and connect.
3. Discover the GATT services and open the Traffic Light Costume service above.
4. Write the UTF-8 text `traffic` to Mode.
5. Write `halloween` to Mode.
6. Write `reactor` to Effect.
7. Write `40` to Brightness.
8. Read Device State and verify the mode/effect/brightness fields and `result=ok`.
9. Subscribe to Device State notifications and write another valid mode or effect; verify the changed state arrives as a notification.
10. Disconnect. Verify the selected lighting mode/effect continues without the phone connected.
11. Reconnect, rediscover/read Device State, and verify it reports the current state.

If LightBlue displays raw hexadecimal for a returned text value, use its UTF-8/text view; writes remain ordinary text values, not manually composed bytes.

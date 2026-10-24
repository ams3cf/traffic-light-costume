# Web API

The costume runs its own WPA2 access point. Join it, then open
`http://192.168.4.1` for the control page, or call the API directly.

| Setting | Default | Where |
|---|---|---|
| SSID | `TrafficLight` | `WIFI_AP_SSID` in `src/config.h` (override in `src/secrets.h`) |
| Password | `greenlight` | `WIFI_AP_PASSWORD` (WPA2 needs 8+ characters) |
| Address | `192.168.4.1` | fixed in `src/web.cpp` |
| Max phones | 4 | `WIFI_AP_MAX_CLIENTS` |

The DHCP server does not offer a default gateway, so a joined phone or laptop
treats the network as local-only and keeps its cellular or wired connection
for the internet. Verified on Linux/NetworkManager (no default route added);
not yet verified on an iPhone or Android phone.

## Endpoints

Commands are `POST` with an `application/x-www-form-urlencoded` body. They are
queued to the lights core and return `202 {"queued":true}`; the change is
visible in the next `/api/status` response.

| Method | Path | Body | Effect |
|---|---|---|---|
| GET | `/` | | Control page |
| GET | `/api/status` | | Current state (below) |
| POST | `/api/mode` | `mode=auto\|manual\|effects` | Switch mode |
| POST | `/api/color` | `color=red\|yellow\|green` | Switch to manual with that chamber lit |
| POST | `/api/brightness` | `value=0..100` | Set brightness percent |
| GET | `/api/effects` | | Effect names, in playlist order (JSON array) |
| POST | `/api/effect` | `effect=cycle\|<name>` | Switch to effects mode and hold that effect, or `cycle` through all |

Errors return JSON `{"error":"<what to send instead>"}` with `400` for a bad
value, `404` for an unknown path or wrong method, and `503` if the command
queue is full (retry).

## Status

```json
{"mode":"auto","lit":"green","color":"red","effect":"fire","cycle":true,"brightness":31,"fw":"2.0.0"}
```

| Field | Meaning |
|---|---|
| `mode` | `auto`, `manual`, or `effects` |
| `lit` | Chamber currently lit: the traffic phase in auto, the selected color in manual, `none` in effects |
| `color` | Color manual mode shows (kept while in other modes) |
| `effect` | Current or next effects-mode effect |
| `cycle` | `true` while rotating through all effects, `false` while holding one |
| `brightness` | Requested percent, before the 1,500 mA power limiter |
| `fw` | Firmware version |

The page polls this endpoint every second, so presses on the hardware buttons
show up on every connected phone.

## Modes

- **Auto:** green 5 s → yellow 2 s → red 5 s, repeating.
- **Manual:** one chamber lit, chosen by the RED/YELLOW/GREEN buttons or the
  page. Entering manual from the mode toggle shows the last chosen color.
- **Effects:** either holds one effect chosen from the page's dropdown, or
  (**Cycle all**, the default) rotates through the playlist in
  `src/effects.cpp`, 15 s each (`EFFECT_STEP_MS`). Switching back to cycling
  gives the current effect a full 15 s before advancing.

## Example

```bash
curl http://192.168.4.1/api/status
curl -d color=red http://192.168.4.1/api/color
curl -d mode=effects http://192.168.4.1/api/mode
curl -d effect=fire http://192.168.4.1/api/effect
curl -d effect=cycle http://192.168.4.1/api/effect
```

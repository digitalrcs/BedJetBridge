# BedJet Matter Bridge

A lightweight **Matter-over-Wi-Fi bridge** that exposes a BedJet (via **BedJetWebSchedule**) as a **Matter Thermostat** + **Matter Fan**.

This project runs on an ESP32 and:

- Connects to your Wi-Fi (STA mode)
- Provides a setup portal (SoftAP) to configure Wi-Fi + BedJetWebSchedule base URL
- Exposes two Matter endpoints: **Thermostat** and **Fan**

## What this is (and is not)

- **This bridge does not talk to the BedJet over BLE directly.**
  Instead, it talks to an existing **BedJetWebSchedule** instance over HTTP on your LAN.
- **BedJetWebSchedule** is responsible for the BLE link to the BedJet and exposes an HTTP API.
- This bridge translates Matter commands into those HTTP calls and keeps Matter state in sync by polling BedJetWebSchedule.

## Features

- **Matter Thermostat endpoint**
  - Modes: OFF / HEAT / COOL / EMERGENCY_HEAT (mapped to BedJet EXT-HEAT)
  - Setpoint control (BedJet range is clamped to **62–110°F**)
  - Local temperature reflected from BedJet `air=` reading when available
- **Matter Fan endpoint**
  - Speed as **0–100%**, mapped to BedJet fan steps **0–19**
  - ON/OFF behavior (0% -> OFF)
- **Adaptive polling**
  - 30s steady-state polling
  - 5s polling for 60s after a command (faster convergence for voice assistants)
- **Automatic BLE connect attempt**
  - If BedJetWebSchedule reports `ble_connected=false`, the bridge POSTs `/api/ble/connect` before sending commands
- **Built-in status page** (HTTP :80)
  - Shows IP, BedJet base URL, commissioning status, pairing code, QR URL
- **Field-safe recovery controls**
  - Hold **BOOT at power-on** to force the setup portal
  - Hold **BOOT for ~5s** while running to decommission Matter and reboot

## Requirements

### Hardware

- An **ESP32** that supports the Arduino `Matter.h` library (via the **Arduino-ESP32** core), plus Wi-Fi.
- A working **BedJetWebSchedule** instance reachable on your LAN.
  - BedJetWebSchedule is commonly run on another ESP32 that connects to the BedJet over BLE.

### Software

- Arduino IDE or PlatformIO
- Espressif **Arduino-ESP32** core that includes Matter (`#include <Matter.h>`)

### Network

- The bridge and BedJetWebSchedule must be on the same routable network (no captive portals)
- HTTP access from the bridge to the BedJetWebSchedule base URL

## Quick start

1. **Flash** the sketch (`BedJetMatterBridge.ino`) to your ESP32.
2. Open **Serial Monitor** at **115200**.
3. On first boot (or if config is missing), the device starts a setup AP:
   - SSID: `BedJetBridge-<XXXX>`
   - Connect and browse to `http://192.168.4.1/`
4. Enter:
   - Wi-Fi SSID + password
   - BedJetWebSchedule base URL (example: `http://192.168.1.253`)
   - Default mode used when the BedJet is OFF and you set temp/fan: `HEAT` or `COOL`
5. The device reboots, joins Wi-Fi, and prints its **Matter pairing code** on Serial.
6. (Optional) Visit the bridge status page: `http://<bridge-ip>/` for commissioning + pairing details.
7. Use your Matter controller to **pair** using the code.

## How it works

### Data flow

```
Matter Controller  ->  ESP32 Bridge  ->  BedJetWebSchedule  ->  BedJet (BLE)
       ^                   |
       |                   v
       +------------ State poll (/api/state) --------------+
```

### Key behaviors (from the code)

- **Configuration**
  - Stored in ESP32 NVS using `Preferences` namespace `bjbridge` (`ssid`, `pass`, `base`, `defmode`).
  - If SSID/base are missing, or BOOT is held at power-on, the bridge starts a SoftAP and serves a minimal HTML config portal.

- **Matter endpoints**
  - A `MatterThermostat` and a `MatterFan` are created and started, then `Matter.begin()` is called.
  - If not commissioned, pairing code and QR URL are printed on Serial (and exposed via the status page).

- **Matter -> BedJetWebSchedule (commands)**
  - The bridge detects controller writes by polling Matter getters and comparing them to cached values.
  - Writes are turned into a single queued command (`PendingCmd`), executed via HTTP POST:
    - Mode: `POST /api/cmd/button?name=<MODE>`
    - Temp: `POST /api/cmd/button?name=<MODE>&temp=<F>`
    - Fan:  `POST /api/cmd/button?name=<MODE>&fan=<STEP>`
  - Fan percent is mapped to BedJet fan steps 0–19.
  - If the BedJet is OFF and you set temp or fan > 0, the bridge uses your configured default mode (`HEAT` or `COOL`) to turn it on.

- **BedJetWebSchedule -> Matter (state sync)**
  - The bridge polls `GET /api/state` and parses `status_summary` for:
    - `mode=...`
    - `fan=...%`
    - `target=...F`
    - `air=...F`
  - Parsed values update Matter mode, setpoints, fan speed, and local temperature.
  - Polling is **adaptive**: slower when idle, faster for ~60s after sending a command.

- **BLE auto-connect**
  - If `ble_connected` is false/unknown, the bridge attempts `POST /api/ble/connect` once before issuing a command.

- **Recovery controls**
  - Hold **BOOT ~5 seconds** while running to **decommission Matter** and reboot.

## BedJetWebSchedule API contract

This bridge expects the following endpoints on the configured base URL:

| Method | Path | Purpose | Required fields |
|---|---|---|---|
| GET | `/api/state` | Returns current state as JSON | `status_summary` (string), optional `ble_connected` (bool) |
| POST | `/api/ble/connect` | Requests BedJetWebSchedule to connect to the BedJet over BLE | JSON body containing `ok` is helpful but not strictly required |
| POST | `/api/cmd/button?name=<...>&fan=<...>&temp=<...>` | Issues a BedJet command | JSON body containing `ok` is helpful but not strictly required |

Notes:

- `/api/cmd/button` is **POST-only** in BedJetWebSchedule.
- The bridge does not use a full JSON parser. It relies on the `status_summary` string format (e.g. `mode=heat fan=55% target=89F air=85F ...`).

## Local status UI

The bridge runs a small HTTP server on port **80**:

- `GET /` - Plain-text status (Wi-Fi, IP, base URL, last mode, commissioning status, pairing code, QR URL)
- `POST /button` - Queue a mode button (form field: `name=HEAT|COOL|TURBO|EXT-HEAT|DRY|OFF...`)

This is primarily for bring-up and diagnostics.

## Security notes

This is intended for **trusted home networks**.

- The setup portal and status endpoint have **no authentication**.
- Wi-Fi credentials are stored in ESP32 NVS (`Preferences`).
- Commands to BedJetWebSchedule are sent over plain **HTTP**.

If you care about segmentation:

- Put BedJet devices on an IoT VLAN and only allow the minimum required east-west traffic.
- Avoid exposing the bridge or BedJetWebSchedule ports to the internet.

## Troubleshooting

- **Bridge never joins Wi-Fi**
  - Hold BOOT while powering on to force the setup portal.
  - Verify SSID/password; watch Serial output for `STA_DISCONNECTED reason=...`.

- **Bridge joins Wi-Fi but never controls BedJet**
  - Verify the BedJetWebSchedule base URL (no trailing slash required; the bridge will normalize it).
  - Confirm you can hit `http://<bedjetwebschedule-ip>/api/state` from another device.

- **BLE not connected**
  - The bridge will try `POST /api/ble/connect` automatically before commands.
  - If it still fails, BedJetWebSchedule may be out of BLE range, not running, or already paired with another controller.

- **Matter pairing issues**
  - Pairing code prints repeatedly when not commissioned.
  - If previously paired and you want to start over: hold BOOT for ~5s to decommission, then reboot and re-pair.

## Project layout

This repo is the original Arduino sketch split into modules for easier debugging and version control:

- `BedJetMatterBridge.ino` — thin wrapper calling `App::setup()` / `App::loop()`
- `App.{h,cpp}` — main orchestration (setup/loop)
- `Globals.{h,cpp}` — shared state and constants
- `Utils.{h,cpp}` — small helpers (HTML escape, temperature conversion, clamping, fan step conversion, URL normalization)
- `HttpUtil.{h,cpp}` — HTTP GET/POST helpers
- `ConfigPortal.{h,cpp}` — Preferences load/save + AP config portal + Wi-Fi STA connect
- `BedJetClient.{h,cpp}` — BedJetWebSchedule API client + parser + command queue
- `MatterIntegration.{h,cpp}` — Matter endpoints and write-back logic
- `StatusWeb.{h,cpp}` — minimal HTTP endpoint on port 80 (root + /button)

## License

See `LICENSE`.

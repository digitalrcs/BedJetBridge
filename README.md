# BedJetMatterBridge (split)

This is the same sketch as `BedJetMatterBridge.ino`, split into modules.

## Files

- `BedJetMatterBridge.ino` — thin wrapper calling `App::setup()` / `App::loop()`
- `App.{h,cpp}` — main orchestration (setup/loop)
- `Globals.{h,cpp}` — shared state (kept global to minimize behavior changes)
- `Utils.{h,cpp}` — small helpers (HTML escape, conversions, clamping, fan step conversion)
- `HttpUtil.{h,cpp}` — HTTP GET/POST helpers
- `ConfigPortal.{h,cpp}` — Preferences load/save + AP config portal + Wi-Fi STA connect
- `BedJetClient.{h,cpp}` — BedJetWebSchedule API client + parser + command queue
- `MatterIntegration.{h,cpp}` — Matter endpoints and write-back logic
- `StatusWeb.{h,cpp}` — minimal HTTP endpoint on port 80 (root + /button POST)

## Arduino IDE

Put all files in the same sketch folder (Arduino IDE will compile `.ino` + `.cpp` automatically).

## API expectations

This bridge targets the **BedJetWebSchedule** HTTP API:

- `GET /api/state` (status + `ble_connected`)
- `POST /api/ble/connect` (connect ESP32 to BedJet over BLE)
- `POST /api/cmd/button?...` (commands)

Note: `/api/cmd/button` is **POST-only** in BedJetWebSchedule. A browser address bar issues `GET` and will 404.


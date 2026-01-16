#pragma once
/*
  BedJet Matter Bridge - split into modules for easier debugging.

  Original sketch: BedJetMatterBridge.ino
  Split by: ChatGPT (GPT-5.2 Thinking)

  Notes:
    - This keeps the original global state and function names to minimize behavior changes.
    - Each module includes Globals.h to access shared state.
*/

#include <Arduino.h>
#include <Matter.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>

// If your board uses a different BOOT/PROG pin, define BOOT_PIN in build flags or before including this header.
#ifndef BOOT_PIN
#define BOOT_PIN 0
#endif

// --------- BedJet capabilities ----------
static constexpr int BJ_FAN_MIN = 0;
static constexpr int BJ_FAN_MAX = 19;

static constexpr int BJ_TEMP_MIN_F = 62;
static constexpr int BJ_TEMP_MAX_F = 110;

// Polling strategy:
//   - When idle, poll slowly to reduce chatter.
//   - After any command is sent to BedJetWebSchedule, poll faster for a short period
//     to converge state quickly for voice assistants.
static constexpr uint32_t POLL_IDLE_MS      = 30000; // 30s steady-state
static constexpr uint32_t POLL_ACTIVE_MS    = 5000;  // 5s after changes
static constexpr uint32_t POLL_ACTIVE_FOR_MS = 60000; // 60s window after commands
static constexpr uint32_t HTTP_TIMEOUT_MS = 2500;

// --------- Types (kept in header to avoid Arduino auto-proto edge cases) ----------
struct BjParsed {
  String mode;     // lowercase
  int fanPct = -1; // 0..100
  int targetF = -1;
  int airF = -1;
};

enum CmdKind : uint8_t {
  CMD_NONE = 0,
  CMD_SET_TEMP_F,
  CMD_SET_FAN_PCT,
  CMD_SET_MODE_BUTTON
};

struct PendingCmd {
  CmdKind kind = CMD_NONE;
  int tempF = -1;
  int fanPct = -1;
  String buttonName; // uppercase
  uint32_t enqueuedAt = 0;
};

// --------- Globals ----------
extern Preferences prefs;
extern WebServer web;

extern MatterThermostat gThermostat;
extern MatterFan gFan;

extern String gStaSsid;
extern String gStaPass;
extern String gBedJetBase;        // http://x.x.x.x (no trailing slash)
extern String gDefaultModeOnTemp; // HEAT or COOL

extern bool gConfigMode;
extern uint32_t gLastPoll;
extern uint32_t gLastCmdAt;   // millis() timestamp of last outbound command

extern String gLastBedJetMode;       // lowercase
extern String gLastBedJetModeButton; // uppercase

// Last known BLE connection state as reported by BedJetWebSchedule /api/state:
//   -1 = unknown/not yet polled
//    0 = not connected
//    1 = connected
extern int8_t gLastBleConnected;

extern bool gSuppressMatterUpdate;
extern uint8_t gLastSeenThermoMode;
extern double gLastSeenHeatSetpointC;
extern double gLastSeenCoolSetpointC;
extern uint8_t gLastSeenMatterFanPct;
extern uint8_t gLastSeenMatterFanMode;

extern PendingCmd gCmd;

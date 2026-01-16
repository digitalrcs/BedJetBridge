#include "App.h"
#include "Globals.h"
#include "ConfigPortal.h"
#include "MatterIntegration.h"
#include "StatusWeb.h"
#include "BedJetClient.h"

#include <esp_system.h>

// --------- App entrypoints ----------

void App::setup() {
  Serial.begin(115200);
  // Give the Serial monitor a moment to attach, especially after ESP.restart().
  delay(800);

  Serial.println();
  Serial.printf("[BOOT] Reset reason: %d\n", (int)esp_reset_reason());

  // Verbose Wi-Fi event logging (helps when it "doesn't connect" without obvious output)
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.println("[WIFI] Event: STA_CONNECTED");
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.printf("[WIFI] Event: GOT_IP %s\n", WiFi.localIP().toString().c_str());
        break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.printf("[WIFI] Event: DISCONNECTED reason=%u\n", info.wifi_sta_disconnected.reason);
        break;
      default:
        break;
    }
  });

  pinMode(BOOT_PIN, INPUT_PULLUP);

  prefsLoad();

  Serial.println("=== BedJet Matter Bridge (STA + Thermostat + Fan) ===");
  Serial.printf("Saved SSID: %s\n", gStaSsid.c_str());
  Serial.printf("Saved Base: %s\n", gBedJetBase.c_str());
  Serial.printf("Default Mode On Temp/Fan>0: %s\n", gDefaultModeOnTemp.c_str());

  bool forceCfg = (digitalRead(BOOT_PIN) == LOW);

  if (forceCfg || gStaSsid.isEmpty() || gBedJetBase.isEmpty()) {
    Serial.println("[CFG] Entering AP config portal mode.");
    startConfigAP();
    return;
  }

  connectSta();
  startMatterEndpoints();

  // Seed last-seen Matter values so the first loop doesn't immediately enqueue commands.
  gLastSeenThermoMode = (uint8_t)gThermostat.getMode();
  gLastSeenHeatSetpointC = gThermostat.getHeatingSetpoint();
  gLastSeenCoolSetpointC = gThermostat.getCoolingSetpoint();
  startStatusWeb();
  gLastSeenMatterFanPct = (uint8_t)gFan.getSpeedPercent();
  gLastSeenMatterFanMode = (uint8_t)gFan.getMode();

  // Do an initial poll quickly
  gLastPoll = 0;
}

void App::loop() {
  if (gConfigMode) {
    web.handleClient();
    delay(2);
    return;
  }

  web.handleClient();

  static uint32_t lastHint = 0;
  if (!Matter.isDeviceCommissioned() && (millis() - lastHint) > 10000) {
    lastHint = millis();
    Serial.printf("[MATTER] Pairing code: %s\n", Matter.getManualPairingCode().c_str());
  }

  checkForThermostatWritesFromController();
  checkForFanWritesFromController();

  // Execute queued command via HTTP POST
  processPendingCmdOnce();

  // Poll BedJet -> update Matter
  // Keep polling independent of Matter commissioning state (HTTP status feed is useful either way),
  // but poll less often when idle to reduce network chatter.
  const uint32_t now = millis();
  const uint32_t sinceCmd = now - gLastCmdAt;
  const uint32_t pollEvery = (sinceCmd < POLL_ACTIVE_FOR_MS) ? POLL_ACTIVE_MS : POLL_IDLE_MS;
  if ((now - gLastPoll) >= pollEvery) {
    gLastPoll = now;
    syncFromBedJetOnce();
  }

  // Hold BOOT ~5s to decommission
  static uint32_t pressedAt = 0;
  bool pressed = (digitalRead(BOOT_PIN) == LOW);
  if (pressed && pressedAt == 0) pressedAt = millis();
  if (!pressed) pressedAt = 0;
  if (pressedAt && (millis() - pressedAt) > 5000) {
    Serial.println("[MATTER] Decommissioning...");
    Matter.decommission();
    delay(500);
    ESP.restart();
  }

  delay(2);
}
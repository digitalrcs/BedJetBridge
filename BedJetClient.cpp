#include "BedJetClient.h"
#include "HttpUtil.h"
#include "Utils.h"
#include <math.h>

// --------- BedJet client + parsing ----------

static bool jsonGetString(const String& json, const char* key, String& out) {
  String k = String("\"") + key + "\":";
  int i = json.indexOf(k);
  if (i < 0) return false;
  i += k.length();
  while (i < (int)json.length() && (json[i] == ' ')) i++;
  if (i >= (int)json.length() || json[i] != '\"') return false;
  i++;
  String v;
  for (; i < (int)json.length(); i++) {
    char c = json[i];
    if (c == '\\') {
      if (i + 1 < (int)json.length()) { v += json[i + 1]; i++; }
      continue;
    }
    if (c == '\"') break;
    v += c;
  }
  out = v;
  return true;
}

static bool jsonGetBool(const String& json, const char* key, bool& out) {
  // Very small helper (no full JSON parser): looks for "key":true/false
  String k = String("\"") + key + "\":";
  int i = json.indexOf(k);
  if (i < 0) return false;
  i += k.length();
  while (i < (int)json.length() && (json[i] == ' ')) i++;
  if (i >= (int)json.length()) return false;
  if (json.startsWith("true", i))  { out = true;  return true; }
  if (json.startsWith("false", i)) { out = false; return true; }
  return false;
}

static bool jsonGetOkBool(const String& json, bool& ok) {
  return jsonGetBool(json, "ok", ok);
}

static int parseIntAfter(const String& s, const String& marker) {
  int i = s.indexOf(marker);
  if (i < 0) return -1;
  i += marker.length();
  String num;
  while (i < (int)s.length()) {
    char c = s[i];
    if ((c >= '0' && c <= '9') || (c == '-' && num.isEmpty())) num += c;
    else break;
    i++;
  }
  if (num.isEmpty()) return -1;
  return num.toInt();
}

static bool parseStatusSummary(const String& summary, BjParsed& out) {
  int m = summary.indexOf("mode=");
  if (m >= 0) {
    int start = m + 5;
    int end = summary.indexOf("(", start);
    if (end < 0) end = summary.indexOf(" ", start);
    if (end < 0) end = summary.length();
    out.mode = summary.substring(start, end);
    out.mode.toLowerCase();
  } else {
    out.mode = "unknown";
  }

  out.fanPct  = parseIntAfter(summary, "fan=");
  out.targetF = parseIntAfter(summary, "target=");
  out.airF    = parseIntAfter(summary, "air=");
  return true;
}

static String modeToButtonUpper(const String& modeLower) {
  if (modeLower == "heat") return "HEAT";
  if (modeLower == "cool") return "COOL";
  if (modeLower == "dry") return "DRY";
  if (modeLower == "turbo") return "TURBO";
  if (modeLower == "ext-heat") return "EXT-HEAT";
  if (modeLower == "off") return "OFF";
  return "OFF";
}

bool bedjetCmdButtonPOST(const String& nameUpper, int fanStep /*-1 keep*/, int tempF /*-1 keep*/) {
  if (gBedJetBase.isEmpty()) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  // Ensure BedJetWebSchedule is connected to the BedJet over BLE.
  // If not connected, attempt /api/ble/connect once before issuing commands.
  if (gLastBleConnected != 1) {
    int c = 0;
    String b;
    Serial.printf("[BLE] Not connected (state=%d). POST %s/api/ble/connect\n", (int)gLastBleConnected, gBedJetBase.c_str());
    bool ok = httpPostEmpty(gBedJetBase + "/api/ble/connect", &c, &b);
    bool okJson = false;
    bool okVal = false;
    if (b.length()) okJson = jsonGetOkBool(b, okVal);
    Serial.printf("[BLE] connect -> %s (code=%d) body=%s\n", ok ? "OK" : "FAIL", c, b.c_str());

    // Give BLE a moment and refresh /api/state so gLastBleConnected is accurate.
    delay(250);
    syncFromBedJetOnce();

    // If response explicitly said ok:false, or we still aren't connected, bail.
    if ((okJson && !okVal) || gLastBleConnected != 1) {
      Serial.printf("[BLE] Still not connected after connect attempt (state=%d). Command aborted.\n", (int)gLastBleConnected);
      return false;
    }
  }

  String url = gBedJetBase + "/api/cmd/button?name=" + nameUpper;

  if (fanStep >= 0) {
    fanStep = clampInt(fanStep, BJ_FAN_MIN, BJ_FAN_MAX);
    url += "&fan=" + String(fanStep);
  }
  if (tempF >= 0) {
    tempF = clampInt(tempF, BJ_TEMP_MIN_F, BJ_TEMP_MAX_F);
    url += "&temp=" + String(tempF);
  }

  int code = 0;
  String body;
  Serial.printf("[HTTP] POST %s\n", url.c_str());
  bool ok = httpPostEmpty(url, &code, &body);
  Serial.printf("[HTTP] -> %s (code=%d) body=%s\n", ok ? "OK" : "FAIL", code, body.c_str());
  return ok;
}

void enqueueCmdTempF(int tempF) {
  gCmd.kind = CMD_SET_TEMP_F;
  gCmd.tempF = clampInt(tempF, BJ_TEMP_MIN_F, BJ_TEMP_MAX_F);
  gCmd.fanPct = -1;
  gCmd.buttonName = "";
  gCmd.enqueuedAt = millis();
  Serial.printf("[QUEUE] TempF=%d\n", gCmd.tempF);
}

void enqueueCmdFanPct(int pct) {
  gCmd.kind = CMD_SET_FAN_PCT;
  gCmd.fanPct = clampInt(pct, 0, 100);
  gCmd.tempF = -1;
  gCmd.buttonName = "";
  gCmd.enqueuedAt = millis();
  Serial.printf("[QUEUE] FanPct=%d\n", gCmd.fanPct);
}

void enqueueCmdModeButton(const String& upperName) {
  gCmd.kind = CMD_SET_MODE_BUTTON;
  gCmd.buttonName = upperName;
  gCmd.tempF = -1;
  gCmd.fanPct = -1;
  gCmd.enqueuedAt = millis();
  Serial.printf("[QUEUE] Button=%s\n", gCmd.buttonName.c_str());
}

void processPendingCmdOnce() {
  if (gCmd.kind == CMD_NONE) return;

  String modeBtn = gLastBedJetModeButton;
  if (modeBtn.isEmpty()) modeBtn = "OFF";

  if (gCmd.kind == CMD_SET_MODE_BUTTON) {
    modeBtn = gCmd.buttonName;
    if (modeBtn.isEmpty()) modeBtn = "OFF";

    gLastBedJetModeButton = modeBtn;
    Serial.printf("[CMD] Mode button: %s\n", modeBtn.c_str());
    bedjetCmdButtonPOST(modeBtn, -1, -1);
    gLastCmdAt = millis();
  }
  else if (gCmd.kind == CMD_SET_FAN_PCT) {
    int pct = gCmd.fanPct;
    int step = fanPctToStep(pct);

    // If BedJet currently reports OFF/unknown and user sets fan > 0,
    // turn on using default mode so it actually applies.
    if ((gLastBedJetMode == "off" || gLastBedJetMode == "unknown") && pct > 0) {
      modeBtn = gDefaultModeOnTemp;
    }

    Serial.printf("[CMD] Fan pct=%d -> step=%d (modeBtn=%s lastMode=%s)\n",
                  pct, step, modeBtn.c_str(), gLastBedJetMode.c_str());

    bedjetCmdButtonPOST(modeBtn, step, -1);
    gLastCmdAt = millis();
  }
  else if (gCmd.kind == CMD_SET_TEMP_F) {
    int tempF = gCmd.tempF;

    // If BedJet is OFF/unknown, choose default mode so “set temp” turns it on.
    if (gLastBedJetMode == "off" || gLastBedJetMode == "unknown") {
      modeBtn = gDefaultModeOnTemp;
    }

    Serial.printf("[CMD] Temp %dF (modeBtn=%s lastMode=%s)\n",
                  tempF, modeBtn.c_str(), gLastBedJetMode.c_str());

    bedjetCmdButtonPOST(modeBtn, -1, tempF);
    gLastCmdAt = millis();
  }

  // After executing, force a near-immediate poll on next loop cycle
  gLastPoll = 0;
  gCmd.kind = CMD_NONE;
}

void syncFromBedJetOnce() {
  if (gBedJetBase.isEmpty()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  // Poll with a small retry to tolerate weak Wi-Fi.
  String body;
  int code = 0;
  bool ok = false;
  for (int attempt = 0; attempt < 3 && !ok; attempt++) {
    ok = httpGet(gBedJetBase + "/api/state", &body, &code);
    if (!ok) {
      Serial.printf("[POLL] /api/state failed (code=%d) attempt=%d\n", code, attempt + 1);
      delay(250 * (attempt + 1));
    }
  }
  if (!ok) return;

  // Track BLE connection state.
  bool bleConn = false;
  if (jsonGetBool(body, "ble_connected", bleConn)) {
    gLastBleConnected = bleConn ? 1 : 0;
  } else {
    // If key is missing, don't overwrite a previously known value.
    if (gLastBleConnected == -1) gLastBleConnected = 0;
  }

  String summary;
  if (!jsonGetString(body, "status_summary", summary)) {
    Serial.println("[POLL] Missing status_summary");
    return;
  }

  BjParsed p;
  parseStatusSummary(summary, p);

  gLastBedJetMode = p.mode;

  // Update thermostat mode based on BedJet mode.
  MatterThermostat::ThermostatMode_t tmode = MatterThermostat::THERMOSTAT_MODE_OFF;
  if (p.mode == "cool") tmode = MatterThermostat::THERMOSTAT_MODE_COOL;
  else if (p.mode == "heat" || p.mode == "turbo") tmode = MatterThermostat::THERMOSTAT_MODE_HEAT;
  else if (p.mode == "ext-heat") tmode = MatterThermostat::THERMOSTAT_MODE_EMERGENCY_HEAT;
  gThermostat.setMode(tmode);
  gLastSeenThermoMode = (uint8_t)gThermostat.getMode();

  // Local temperature reading from BedJet 'airF' if present.
  if (p.airF >= 0) {
    gThermostat.setLocalTemperature(fToC(p.airF));
  }
  gLastBedJetModeButton = modeToButtonUpper(p.mode);

  Serial.printf("[POLL] %s\n", summary.c_str());
  Serial.printf("[POLL] parsed: mode=%s fanPct=%d targetF=%d airF=%d (modeBtn=%s)\n",
                p.mode.c_str(), p.fanPct, p.targetF, p.airF, gLastBedJetModeButton.c_str());

  gSuppressMatterUpdate = true;  if (p.targetF >= 0) {
    const double minC = fToC(BJ_TEMP_MIN_F);
    const double maxC = fToC(BJ_TEMP_MAX_F);
    const double dead = (double)gThermostat.getDeadBand();

    double tgtC = fToC(p.targetF);
    if (tgtC < minC) tgtC = minC;
    if (tgtC > maxC) tgtC = maxC;

    // Update setpoints in a way that satisfies deadband constraints when controllers care.
    if (p.mode == "cool") {
      gThermostat.setCoolingSetpoint(tgtC);
      // Ensure heating <= cooling - deadband
      double heatC = gThermostat.getHeatingSetpoint();
      if (heatC > (tgtC - dead)) gThermostat.setHeatingSetpoint(tgtC - dead);
    } else if (p.mode == "heat" || p.mode == "ext-heat" || p.mode == "turbo") {
      gThermostat.setHeatingSetpoint(tgtC);
      // Ensure cooling >= heating + deadband
      double coolC = gThermostat.getCoolingSetpoint();
      if (coolC < (tgtC + dead)) gThermostat.setCoolingSetpoint(tgtC + dead);
    } else {
      // Unknown/off: update both conservatively
      gThermostat.setHeatingSetpoint(tgtC);
      gThermostat.setCoolingSetpoint(tgtC + dead);
    }

    gLastSeenHeatSetpointC = gThermostat.getHeatingSetpoint();
    gLastSeenCoolSetpointC = gThermostat.getCoolingSetpoint();
  }

  if (p.fanPct >= 0) {
    uint8_t pct = (uint8_t)clampInt(p.fanPct, 0, 100);
    gFan.setSpeedPercent(pct);
    gLastSeenMatterFanPct = (uint8_t)gFan.getSpeedPercent();
    if (pct == 0) gFan.setMode(MatterFan::FAN_MODE_OFF);
    else gFan.setMode(MatterFan::FAN_MODE_ON);
    gLastSeenMatterFanMode = (uint8_t)gFan.getMode();
  }

  gSuppressMatterUpdate = false;
}
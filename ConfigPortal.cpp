#include "ConfigPortal.h"
#include "Utils.h"

// --------- Config / Preferences / Portal ----------

void prefsLoad() {
  prefs.begin("bjbridge", true);
  gStaSsid = prefs.getString("ssid", "");
  gStaPass = prefs.getString("pass", "");
  gBedJetBase = prefs.getString("base", "");
  gDefaultModeOnTemp = prefs.getString("defmode", "HEAT");
  prefs.end();

  gDefaultModeOnTemp.toUpperCase();
  if (gDefaultModeOnTemp != "COOL") gDefaultModeOnTemp = "HEAT";

  if (!gBedJetBase.isEmpty()) {
    gBedJetBase = normalizeBaseUrl(gBedJetBase);
  }
}

void prefsSaveAll(const String& ssid, const String& pass, const String& base, const String& defmode) {
  prefs.begin("bjbridge", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putString("base", base);
  prefs.putString("defmode", defmode);
  prefs.end();
}

void handleCfgRoot() {
  String page;
  page.reserve(3200);

  page += "<!doctype html><html><head>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'/>";
  page += "<title>BedJet Bridge Setup</title></head>";
  page += "<body style='font-family:system-ui;max-width:820px;margin:24px auto;padding:0 12px'>";
  page += "<h2>BedJet Matter Bridge Setup</h2>";
  page += "<p>Enter Wi-Fi + BedJet base URL, save. Pairing code will be on Serial in normal mode.</p>";

  page += "<form method='POST' action='/save'>";
  page += "<label>Wi-Fi SSID</label><br/>";
  page += "<input name='ssid' style='width:100%;padding:10px' value='" + htmlEsc(gStaSsid) + "'/><br/><br/>";
  page += "<label>Wi-Fi Password</label><br/>";
  page += "<input name='pass' type='password' style='width:100%;padding:10px' value='" + htmlEsc(gStaPass) + "'/><br/><br/>";

  page += "<label>BedJetWebSchedule Base URL</label><br/>";
  page += "<input name='base' style='width:100%;padding:10px' value='" + htmlEsc(gBedJetBase) + "' placeholder='http://192.168.1.253'/><br/><br/>";

  page += "<label>Default mode when BedJet is OFF and you set BedJetTemp / BedJetFan&gt;0</label><br/>";
  page += "<select name='defmode' style='width:100%;padding:10px'>";
  page += "<option value='HEAT'";
  if (gDefaultModeOnTemp == "HEAT") page += " selected";
  page += ">HEAT</option>";
  page += "<option value='COOL'";
  if (gDefaultModeOnTemp == "COOL") page += " selected";
  page += ">COOL</option>";
  page += "</select><br/><br/>";

  page += "<button style='padding:10px 16px'>Save & Restart</button>";
  page += "</form>";

  page += "<hr/><p><small>Tip: Hold BOOT while powering on to force this setup portal.</small></p>";
  page += "</body></html>";

  web.send(200, "text/html", page);
}

void handleCfgSave() {
  if (!web.hasArg("ssid") || !web.hasArg("pass") || !web.hasArg("base") || !web.hasArg("defmode")) {
    web.send(400, "text/plain", "Missing fields");
    return;
  }

  String ssid = web.arg("ssid"); ssid.trim();
  String pass = web.arg("pass");
  String base = web.arg("base");
  String defm = web.arg("defmode"); defm.trim(); defm.toUpperCase();
  if (defm != "COOL") defm = "HEAT";

  base = normalizeBaseUrl(base);

  prefsSaveAll(ssid, pass, base, defm);

  // Helpful serial confirmation before reboot (Serial monitor often misses early boot lines).
  Serial.println("[CFG] Saved configuration:");
  Serial.printf("[CFG]   SSID: %s\n", ssid.c_str());
  Serial.printf("[CFG]   Base: %s\n", base.c_str());
  Serial.printf("[CFG]   DefaultMode: %s\n", defm.c_str());
  Serial.flush();

  web.send(200, "text/plain", "Saved. Restarting... (Serial may disconnect briefly)");
  delay(1200);
  ESP.restart();
}

void startConfigAP() {
  gConfigMode = true;

  String ssid = "BedJetBridge-" + chipIdSuffix();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str());

  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[CFG] AP started: %s  IP=%s\n", ssid.c_str(), ip.toString().c_str());
  Serial.println("[CFG] Open http://192.168.4.1/");

  web.on("/", HTTP_GET, handleCfgRoot);
  web.on("/save", HTTP_POST, handleCfgSave);
  web.begin();
}

void connectSta() {
  // Ensure we fully leave AP mode if we came from the portal.
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, true);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  Serial.printf("[WIFI] Connecting to '%s'...\n", gStaSsid.c_str());
  WiFi.begin(gStaSsid.c_str(), gStaPass.c_str());

  // Give it longer than 15s; some APs take time after a reboot.
  uint32_t start = millis();
  uint32_t lastDot = 0;
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 30000) {
    delay(50);
    if (millis() - lastDot >= 500) {
      lastDot = millis();
      Serial.print(".");
    }
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WIFI] Connected. IP=%s  RSSI=%ddBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.printf("[WIFI] Failed to connect. status=%d\n", (int)WiFi.status());
    Serial.println("[WIFI] Falling back to config portal AP mode.");
    startConfigAP();
  }
}

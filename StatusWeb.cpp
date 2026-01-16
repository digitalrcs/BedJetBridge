#include "StatusWeb.h"
#include "BedJetClient.h"

// --------- Status web ----------

void startStatusWeb() {
  web.on("/", HTTP_GET, []() {
    String msg;
    msg.reserve(1000);
    msg += "BedJet Matter Bridge\n\n";
    msg += "WiFi: " + String(WiFi.status() == WL_CONNECTED ? "connected" : "not-connected") + "\n";
    msg += "IP: " + WiFi.localIP().toString() + "\n";
    msg += "BedJet base: " + gBedJetBase + "\n";
    msg += "Default mode when OFF: " + gDefaultModeOnTemp + "\n";
    msg += "Last BedJet mode: " + gLastBedJetMode + " (button=" + gLastBedJetModeButton + ")\n";
    msg += "Commissioned: " + String(Matter.isDeviceCommissioned() ? "true" : "false") + "\n";
    msg += "Pairing code: " + Matter.getManualPairingCode() + "\n";
    msg += "QR URL: " + Matter.getOnboardingQRCodeUrl() + "\n";
    web.send(200, "text/plain", msg);
  });

  web.on("/button", HTTP_POST, []() {
    if (!web.hasArg("name")) { web.send(400, "text/plain", "Missing name"); return; }
    String n = web.arg("name"); n.trim(); n.toUpperCase();
    Serial.printf("[WEB] Button request: %s\n", n.c_str());
    enqueueCmdModeButton(n);
    web.send(200, "text/plain", "Queued");
  });

  web.begin();
}

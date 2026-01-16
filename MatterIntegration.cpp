#include "MatterIntegration.h"
#include "Utils.h"
#include "BedJetClient.h"
#include <math.h>

// --------- Matter integration ----------

static double clampC(double c, double minC, double maxC) {
  if (c < minC) return minC;
  if (c > maxC) return maxC;
  return c;
}

void startMatterEndpoints() {
  // Thermostat endpoint
  bool ok = gThermostat.begin(MatterThermostat::THERMOSTAT_SEQ_OP_COOLING_HEATING, MatterThermostat::THERMOSTAT_AUTO_MODE_DISABLED);
  Serial.printf("[MATTER] Thermostat endpoint begin: %s\n", ok ? "OK" : "FAIL");

  // Initialize with sane defaults (Celsius)
  const double minC = fToC(BJ_TEMP_MIN_F);
  const double maxC = fToC(BJ_TEMP_MAX_F);
  const double initSetC = clampC(fToC(72), minC, maxC);

  // Local temperature: unknown at boot; start at setpoint
  gThermostat.setLocalTemperature(initSetC);

  // Seed setpoints with a small deadband so controllers in AUTO don't complain.
  const double dead = (double)gThermostat.getDeadBand(); // typically 2.5C
  const double heatC = initSetC;
  const double coolC = clampC(initSetC + dead, minC, maxC);

  gThermostat.setHeatingSetpoint(heatC);
  gThermostat.setCoolingSetpoint(coolC);
  gThermostat.setMode(MatterThermostat::THERMOSTAT_MODE_OFF);

  // Fan endpoint (still separate, useful for voice fan speed)
  bool okFan = gFan.begin();
  Serial.printf("[MATTER] Fan endpoint begin: %s\n", okFan ? "OK" : "FAIL");

  // Matter beginning - last step, after all EndPoints are initialized
  Matter.begin();
  Serial.println("[MATTER] Matter.begin() called (stack init)");

  if (!Matter.isDeviceCommissioned()) {
    Serial.println("[MATTER] Not commissioned yet.");
    Serial.printf("[MATTER] Pairing code: %s\n", Matter.getManualPairingCode().c_str());
    Serial.printf("[MATTER] QR URL: %s\n", Matter.getOnboardingQRCodeUrl().c_str());
  }
}

void checkForThermostatWritesFromController() {
  if (gSuppressMatterUpdate) return;

  // 1) Mode changes
  uint8_t mode = (uint8_t)gThermostat.getMode();
  if (mode != gLastSeenThermoMode) {
    gLastSeenThermoMode = mode;
    Serial.printf("[MATTER->] Thermostat mode write: %s (%u)\n",
                  MatterThermostat::getThermostatModeString(mode), mode);

    switch (mode) {
      case MatterThermostat::THERMOSTAT_MODE_OFF:
        enqueueCmdModeButton("OFF");
        break;
      case MatterThermostat::THERMOSTAT_MODE_COOL:
        enqueueCmdModeButton("COOL");
        break;
      case MatterThermostat::THERMOSTAT_MODE_HEAT:
      case MatterThermostat::THERMOSTAT_MODE_EMERGENCY_HEAT:
        // Map emergency heat to EXT-HEAT if you want
        enqueueCmdModeButton(mode == MatterThermostat::THERMOSTAT_MODE_EMERGENCY_HEAT ? "EXT-HEAT" : "HEAT");
        break;
      default:
        // For unsupported modes (AUTO/FAN_ONLY/DRY/etc), fall back to OFF
        enqueueCmdModeButton("OFF");
        break;
    }
  }

  // 2) Heating setpoint changes
  double heatC = gThermostat.getHeatingSetpoint();
  if (fabs(heatC - gLastSeenHeatSetpointC) >= 0.001) {
    gLastSeenHeatSetpointC = heatC;

    int tempF = clampInt(cToFRound(heatC), BJ_TEMP_MIN_F, BJ_TEMP_MAX_F);
    Serial.printf("[MATTER->] Heating setpoint write: %.2fC (%dF)\n", heatC, tempF);

    // Treat a heating-setpoint write as intent to HEAT at that temp.
    enqueueCmdModeButton("HEAT");
    enqueueCmdTempF(tempF);
  }

  // 3) Cooling setpoint changes
  double coolC = gThermostat.getCoolingSetpoint();
  if (fabs(coolC - gLastSeenCoolSetpointC) >= 0.001) {
    gLastSeenCoolSetpointC = coolC;

    int tempF = clampInt(cToFRound(coolC), BJ_TEMP_MIN_F, BJ_TEMP_MAX_F);
    Serial.printf("[MATTER->] Cooling setpoint write: %.2fC (%dF)\n", coolC, tempF);

    // Treat a cooling-setpoint write as intent to COOL at that temp.
    enqueueCmdModeButton("COOL");
    enqueueCmdTempF(tempF);
  }
}

void checkForFanWritesFromController() {
  if (gSuppressMatterUpdate) return;

  // Controllers may write either the fan mode (ON/OFF) or the speed percent.
  // We mirror thermostat write-handling by polling the Matter getters and comparing
  // against cached values.

  // 1) Mode changes (commonly used for generic "Device" on/off toggles in some controllers)
  uint8_t mode = (uint8_t)gFan.getMode();
  if (mode != gLastSeenMatterFanMode) {
    gLastSeenMatterFanMode = mode;
    Serial.printf("[MATTER->] Fan mode write: %s (%u)\n", MatterFan::getFanModeString(mode), mode);

    if (mode == MatterFan::FAN_MODE_OFF) {
      // Treat OFF as a full BedJet OFF.
      enqueueCmdModeButton("OFF");
      // Also reflect percent to 0 locally (some controllers don't send it)
      // and avoid the percent check re-triggering on a stale nonzero value.
      gSuppressMatterUpdate = true;
      gFan.setSpeedPercent(0);
      gFan.setMode(MatterFan::FAN_MODE_OFF);
      gSuppressMatterUpdate = false;
      gLastSeenMatterFanPct = 0;
      return;
    }
    // If turned ON without a percent change, leave speed as-is; a percent write
    // will follow in many controllers. If it doesn't, we'll handle percent below.
  }

  // 2) Speed percent changes
  uint8_t pct = (uint8_t)clampInt((int)gFan.getSpeedPercent(), 0, 100);
  if (pct != gLastSeenMatterFanPct) {
    gLastSeenMatterFanPct = pct;
    Serial.printf("[MATTER->] Fan speed write: %u%%\n", pct);

    if (pct == 0) {
      // Map 0% to OFF (BedJet expects OFF as a mode button).
      enqueueCmdModeButton("OFF");

      // Reflect mode/speed locally to keep controllers consistent.
      gSuppressMatterUpdate = true;
      gFan.setSpeedPercent(0);
      gFan.setMode(MatterFan::FAN_MODE_OFF);
      gSuppressMatterUpdate = false;
    } else {
      // Apply fan percent; BedJetClient will automatically select a default mode
      // if the device is currently OFF.
      enqueueCmdFanPct((int)pct);

      // Make sure the fan endpoint is "ON" when percent > 0.
      gSuppressMatterUpdate = true;
      gFan.setMode(MatterFan::FAN_MODE_ON);
      gSuppressMatterUpdate = false;
    }

    // Keep fan mode internally consistent for controllers that show an ON/OFF tile.
    if (pct == 0) {
      gLastSeenMatterFanMode = (uint8_t)MatterFan::FAN_MODE_OFF;
    } else {
      gLastSeenMatterFanMode = (uint8_t)MatterFan::FAN_MODE_ON;
    }
  }
}

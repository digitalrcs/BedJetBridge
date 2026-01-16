#include "Globals.h"

// --------- Globals (definitions) ----------
Preferences prefs;
WebServer web(80);

MatterThermostat gThermostat;
MatterFan gFan;

String gStaSsid;
String gStaPass;
String gBedJetBase;            // http://x.x.x.x (no trailing slash)
String gDefaultModeOnTemp;     // HEAT or COOL

bool gConfigMode = false;
uint32_t gLastPoll = 0;
uint32_t gLastCmdAt = 0;

String gLastBedJetMode = "off";         // lowercase
String gLastBedJetModeButton = "OFF";   // uppercase

int8_t gLastBleConnected = -1;

bool gSuppressMatterUpdate = false;
uint8_t gLastSeenThermoMode = (uint8_t)MatterThermostat::THERMOSTAT_MODE_OFF;
double gLastSeenHeatSetpointC = 0.0;
double gLastSeenCoolSetpointC = 0.0;
uint8_t gLastSeenMatterFanPct = 0;
uint8_t gLastSeenMatterFanMode = 0;

PendingCmd gCmd;

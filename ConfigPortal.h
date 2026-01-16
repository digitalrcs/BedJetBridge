#pragma once
#include "Globals.h"

// Preferences-backed config.
void prefsLoad();
void prefsSaveAll();

// WiFi + config portal
void connectSta();
void startConfigAP();

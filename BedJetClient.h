#pragma once
#include "Globals.h"

// BedJetWebSchedule API command.
bool bedjetCmdButtonPOST(const String& buttonUpper, int fanStep /*0..19 or -1*/, int tempF /*F or -1*/);

// Command queue helpers
void enqueueCmdTempF(int tempF);
void enqueueCmdFanPct(int pct);
void enqueueCmdModeButton(const String& buttonUpper);

// Execute queued command (non-blocking-ish; single command at a time)
void processPendingCmdOnce();

// Poll BedJet /api/state and update globals + Matter endpoints
void syncFromBedJetOnce();

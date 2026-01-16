#pragma once
#include "Globals.h"

// Simple HTTP helpers for BedJetWebSchedule API.
bool httpGet(const String& url, String* outBody, int* outCode);
bool httpPostEmpty(const String& url, int* outCode, String* outBody);

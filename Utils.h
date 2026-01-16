#pragma once
#include "Globals.h"

String chipIdSuffix();
String htmlEsc(const String& s);

double fToC(int f);
int cToFRound(double c);

int clampInt(int v, int lo, int hi);

// BedJet fan uses step 0..19; expose converters used by the bridge.
int fanPctToStep(int pct);
int stepToFanPct(int step);

// Normalize user-provided base URLs (trim, ensure scheme, remove trailing slashes).
// Accepts common variations like "Http://" or missing scheme.
String normalizeBaseUrl(String base);

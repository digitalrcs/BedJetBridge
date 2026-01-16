#include "Utils.h"
#include <math.h>

// --------- Utilities ----------

String chipIdSuffix() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[5];
  snprintf(buf, sizeof(buf), "%04X", (uint16_t)(mac & 0xFFFF));
  return String(buf);
}

String htmlEsc(const String& s) {
  String o = s;
  o.replace("&", "&amp;");
  o.replace("<", "&lt;");
  o.replace(">", "&gt;");
  o.replace("\"", "&quot;");
  o.replace("'", "&#39;");
  return o;
}

double fToC(int f) { return ((double)f - 32.0) * 5.0 / 9.0; }

int cToFRound(double c) { return (int)lround((c * 9.0 / 5.0) + 32.0); }

int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

int fanPctToStep(int pct) {
  pct = clampInt(pct, 0, 100);
  int step = (int)lround((pct / 100.0) * BJ_FAN_MAX);
  return clampInt(step, BJ_FAN_MIN, BJ_FAN_MAX);
}

int stepToFanPct(int step) {
  step = clampInt(step, BJ_FAN_MIN, BJ_FAN_MAX);
  return (int)lround((step / (double)BJ_FAN_MAX) * 100.0);
}

String normalizeBaseUrl(String base) {
  base.trim();

  if (base.isEmpty()) return base;

  // Normalize common scheme variations (startsWith is case-sensitive).
  if (base.startsWith("Http://") || base.startsWith("HTTP://")) {
    base = String("http://") + base.substring(7);
  } else if (base.startsWith("Https://") || base.startsWith("HTTPS://")) {
    base = String("https://") + base.substring(8);
  }

  // If scheme is missing, assume http.
  if (!base.startsWith("http://") && !base.startsWith("https://")) {
    base = String("http://") + base;
  }

  // Remove trailing slashes.
  while (base.endsWith("/")) {
    base.remove(base.length() - 1);
  }

  return base;
}

#include "HttpUtil.h"

// --------- HTTP helpers ----------

bool httpGet(const String& url, String* outBody = nullptr, int* outCode = nullptr) {
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  bool began = http.begin(url);
  if (!began) {
    if (outCode) *outCode = -1000;
    Serial.printf("[HTTP] begin() failed: %s\n", url.c_str());
    return false;
  }

  int code = http.GET();
  if (outCode) *outCode = code;
  if (code > 0 && outBody) *outBody = http.getString();
  if (code <= 0) {
    Serial.printf("[HTTP] GET failed code=%d err=%s url=%s\n",
                  code, http.errorToString(code).c_str(), url.c_str());
  }
  http.end();
  return (code >= 200 && code < 300);
}

bool httpPostEmpty(const String& url, int* outCode = nullptr, String* outBody = nullptr) {
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  bool began = http.begin(url);
  if (!began) {
    if (outCode) *outCode = -1000;
    Serial.printf("[HTTP] begin() failed: %s\n", url.c_str());
    return false;
  }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST("");  // IMPORTANT: BedJetWebSchedule expects POST for /api/cmd/button
  if (outCode) *outCode = code;
  if (code > 0 && outBody) *outBody = http.getString();
  if (code <= 0) {
    Serial.printf("[HTTP] POST failed code=%d err=%s url=%s\n",
                  code, http.errorToString(code).c_str(), url.c_str());
  }
  http.end();
  return (code >= 200 && code < 300);
}

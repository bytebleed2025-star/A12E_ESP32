#include "sheets.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ctype.h>
#include <time.h>

#include "config.h"
#include "license.h"
#include "log.h"
#include "net.h"
#include "settings.h"

static unsigned long lastUploadMs = 0;
static unsigned long nextUploadMs = 0;
static bool lastUploadOk = false;
static uint32_t okCount = 0;
static uint32_t failCount = 0;
static bool wasStaConnected = false;

static String urlEncode(const String &input) {
  String out;
  out.reserve(input.length() * 3);
  char buffer[4];

  for (size_t i = 0; i < input.length(); ++i) {
    const unsigned char c = static_cast<unsigned char>(input[i]);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += static_cast<char>(c);
    } else {
      snprintf(buffer, sizeof(buffer), "%%%02X", c);
      out += buffer;
    }
  }

  return out;
}

static String localTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) return String("");

  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

static bool sendRow(float weight, const String &licenseState, bool isTest,
                    String &message, int &httpCode) {
  httpCode = 0;

  if (!netStaConnected()) {
    message = "Router WiFi is not connected";
    return false;
  }

  const SettingsData &s = settingsGet();
  if (!settingsSheetConfigured()) {
    message = "Google Sheets URL/token is not configured";
    return false;
  }

  String url = s.sheetUrl;
  url += "?token=" + urlEncode(s.sheetToken);
  url += "&device=" + urlEncode(licenseDeviceId());
  url += "&weight=" + String(weight, 2);
  url += "&license=" + urlEncode(licenseState);
  url += "&rssi=" + String(netRssi());
  url += "&uptime=" + String(millis() / 1000UL);
  if (isTest) url += "&test=1";

  const String timestamp = localTimestamp();
  if (timestamp.length() > 0) {
    url += "&timestamp=" + urlEncode(timestamp);
  }

  WiFiClientSecure client;
  client.setInsecure();  // see README: replace with a CA for production

  HTTPClient http;
  http.setTimeout(SHEET_HTTP_TIMEOUT_MS);
  http.setConnectTimeout(SHEET_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!http.begin(client, url)) {
    message = "Could not start HTTPS request";
    failCount++;
    lastUploadMs = millis();
    lastUploadOk = false;
    return false;
  }

  httpCode = http.GET();
  String body;
  if (httpCode > 0) body = http.getString();
  http.end();

  const bool ok =
      (httpCode == 200 && body.indexOf("\"ok\":true") >= 0);

  if (ok) {
    message = "Upload OK";
    lastUploadOk = true;
    okCount++;
  } else {
    message = "HTTP " + String(httpCode);
    if (body.length() > 0) {
      String snippet = body.substring(0, 120);
      snippet.replace("\n", " ");
      message += " - " + snippet;
    }
    lastUploadOk = false;
    failCount++;
  }

  lastUploadMs = millis();

  if (isTest) {
    LOG.println("[SHEET] Test upload: " + message);
  } else if (ok) {
    LOG.printf("[SHEET] Uploaded %.2f kg (HTTP %d)\n", weight, httpCode);
  } else {
    LOG.println("[SHEET] Upload failed: " + message);
  }

  return ok;
}

void sheetsBegin() {
  wasStaConnected = netStaConnected();
  nextUploadMs = millis() + 5000UL;

  if (!settingsSheetConfigured()) {
    LOG.println("[SHEET] Not configured - open http://192.168.4.1/config");
  } else if (!netStaConfigured()) {
    LOG.println("[SHEET] Router WiFi not configured - uploads paused");
  } else {
    LOG.println("[SHEET] Upload interval: " + String(settingsGet().uploadIntervalSec) + " s");
  }
}

void sheetsResetSchedule() {
  nextUploadMs = millis() + 2000UL;
}

void sheetsLoop(float weight, bool weightValid) {
  const bool connected = netStaConnected();
  if (connected && !wasStaConnected) {
    wasStaConnected = true;
    nextUploadMs = millis() + 3000UL;
  } else if (!connected) {
    wasStaConnected = false;
  }

  if (!connected || !settingsSheetConfigured()) return;
  if (licenseIsLocked()) return;
  if (!weightValid) return;
  if (static_cast<long>(millis() - nextUploadMs) < 0) return;

  const uint32_t intervalSec = settingsGet().uploadIntervalSec;
  nextUploadMs = millis() + intervalSec * 1000UL;

  String message;
  int httpCode = 0;
  sendRow(weight, String(licenseStateName()), false, message, httpCode);
}

bool sheetsTest(String &message, int &httpCode) {
  return sendRow(0.0f, "test", true, message, httpCode);
}

long sheetsLastUploadAgeMs() {
  if (lastUploadMs == 0) return -1;
  return static_cast<long>(millis() - lastUploadMs);
}

bool sheetsLastUploadOk() { return lastUploadOk; }

uint32_t sheetsUploadOkCount() { return okCount; }

uint32_t sheetsUploadFailCount() { return failCount; }

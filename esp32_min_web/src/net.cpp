#include "net.h"

#include <WiFi.h>
#include <time.h>

#include "config.h"
#include "log.h"
#include "settings.h"

static unsigned long rebootAtMs = 0;
static unsigned long staStartedMs = 0;
static unsigned long lastReconnectMs = 0;
static bool lastConnected = false;
static bool ntpStarted = false;

static bool staConfigured() {
  return settingsGet().staSsid.length() > 0;
}

static void appendJsonEscaped(String &out, const String &value) {
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      char esc[7];
      snprintf(esc, sizeof(esc), "\\u%04x", static_cast<unsigned char>(c));
      out += esc;
    } else {
      out += c;
    }
  }
}

static void startSta() {
  if (!staConfigured()) {
    LOG.println("[NET] Router WiFi not configured - AP-only mode");
    return;
  }

  const SettingsData &s = settingsGet();
  staStartedMs = millis();
  lastReconnectMs = millis();
  ntpStarted = false;

  LOG.println("[NET] Connecting to router \"" + s.staSsid + "\"...");
  if (s.staPass.length() > 0) {
    WiFi.begin(s.staSsid.c_str(), s.staPass.c_str());
  } else {
    WiFi.begin(s.staSsid.c_str());
  }
}

void netBegin() {
  const SettingsData &s = settingsGet();

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname("a12e-scale");
  WiFi.setAutoReconnect(true);

  const bool apOk = (s.apPass.length() >= 8)
                        ? WiFi.softAP(s.apSsid.c_str(), s.apPass.c_str())
                        : WiFi.softAP(s.apSsid.c_str());

  if (!apOk) {
    LOG.println("[NET] ERROR: softAP failed");
  } else {
    LOG.println("[NET] AP SSID: " + s.apSsid);
    LOG.println("[NET] AP IP: " + WiFi.softAPIP().toString());
  }

  configTzTime(SHEET_TIMEZONE, "pool.ntp.org", "time.google.com");
  startSta();
  lastConnected = false;
}

void netLoop() {
  if (rebootAtMs != 0 && static_cast<long>(millis() - rebootAtMs) >= 0) {
    LOG.println("[NET] Rebooting to apply AP settings...");
    delay(100);
    ESP.restart();
  }

  const bool connected = WiFi.status() == WL_CONNECTED;

  if (connected != lastConnected) {
    lastConnected = connected;
    if (connected) {
      LOG.println("[NET] Router connected: " + WiFi.localIP().toString() +
                  "  RSSI " + String(WiFi.RSSI()) + " dBm");
      configTzTime(SHEET_TIMEZONE, "pool.ntp.org", "time.google.com");
      ntpStarted = true;
    } else if (staConfigured()) {
      LOG.println("[NET] Router disconnected - AP still active");
      ntpStarted = false;
    }
  }

  if (staConfigured() && !connected &&
      millis() - lastReconnectMs > 30000UL) {
    lastReconnectMs = millis();
    LOG.println("[NET] Retrying router connection...");
    WiFi.reconnect();
  }

  if (connected && !ntpStarted) {
    configTzTime(SHEET_TIMEZONE, "pool.ntp.org", "time.google.com");
    ntpStarted = true;
  }
}

void netRestartSta() {
  LOG.println("[NET] Applying new router settings...");
  WiFi.disconnect(false, false);
  startSta();
  lastConnected = false;
}

void netScheduleReboot(uint32_t delayMs) {
  rebootAtMs = millis() + delayMs;
  if (rebootAtMs == 0) rebootAtMs = 1;
}

bool netStaConnected() { return WiFi.status() == WL_CONNECTED; }

bool netStaConfigured() { return staConfigured(); }

String netStaIP() {
  return netStaConnected() ? WiFi.localIP().toString() : String("");
}

String netApIP() { return WiFi.softAPIP().toString(); }

String netStaStatus() {
  if (!staConfigured()) return "not configured";
  if (netStaConnected()) return "connected";
  if (millis() - staStartedMs < 15000UL) return "connecting";
  return "offline";
}

int netRssi() { return netStaConnected() ? WiFi.RSSI() : 0; }

String netScanJson() {
  const int count = WiFi.scanNetworks(false, true);
  if (count < 0) {
    return F("{\"ok\":false,\"error\":\"WiFi scan failed\"}");
  }

  String json;
  json.reserve(128 + count * 64);
  json += F("{\"ok\":true,\"networks\":[");

  size_t emitted = 0;
  for (int i = 0; i < count && emitted < 20; ++i) {
    const String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;

    if (emitted > 0) json += ',';
    json += F("{\"ssid\":\"");
    appendJsonEscaped(json, ssid);
    json += F("\",\"rssi\":");
    json += String(WiFi.RSSI(i));
    json += F(",\"secure\":");
    json += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? F("false") : F("true");
    json += '}';
    emitted++;
  }

  json += F("]}");
  WiFi.scanDelete();
  return json;
}

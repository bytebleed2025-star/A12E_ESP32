#include "settings.h"

#include <Preferences.h>

#include "config.h"
#include "log.h"

#if __has_include("sta_secrets.h")
#include "sta_secrets.h"
#else
#include "sta_secrets.example.h"
#endif

static const char *NVS_NAMESPACE = "a12e_cfg";

static Preferences prefs;
static SettingsData settings;

static bool looksLikePlaceholder(const String &value) {
  return value.length() == 0 || value.startsWith("YOUR_") ||
         value.indexOf("XXXXXXXX") >= 0 ||
         value == "CHANGE_ME_TO_A_LONG_RANDOM_TOKEN";
}

static String defaultStaSsid() {
  return looksLikePlaceholder(String(STA_WIFI_SSID)) ? String("") : String(STA_WIFI_SSID);
}

static String defaultStaPass() {
  return looksLikePlaceholder(String(STA_WIFI_PASSWORD)) ? String("") : String(STA_WIFI_PASSWORD);
}

static String defaultSheetUrl() {
  return looksLikePlaceholder(String(SHEET_WEBAPP_URL)) ? String("") : String(SHEET_WEBAPP_URL);
}

static String defaultSheetToken() {
  return looksLikePlaceholder(String(SHEET_WEBAPP_TOKEN)) ? String("") : String(SHEET_WEBAPP_TOKEN);
}

void settingsBegin() {
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    LOG.println("[CFG] WARNING: NVS unavailable - settings will not persist");
  }

  const auto loadString = [](const char *key, const String &fallback) {
    return prefs.isKey(key) ? prefs.getString(key, "") : fallback;
  };

  settings.apSsid = loadString("ap_ssid", String(WIFI_AP_SSID));
  settings.apPass = loadString("ap_pass", String(WIFI_AP_PASSWORD));
  settings.staSsid = loadString("sta_ssid", defaultStaSsid());
  settings.staPass = loadString("sta_pass", defaultStaPass());
  settings.sheetUrl = loadString("sheet_url", defaultSheetUrl());
  settings.sheetToken = loadString("sheet_tok", defaultSheetToken());
  settings.uploadIntervalSec = prefs.getUInt("interval", SHEET_UPLOAD_INTERVAL_MS / 1000UL);

  if (settings.apSsid.length() == 0) settings.apSsid = WIFI_AP_SSID;
  if (settings.uploadIntervalSec < 10) settings.uploadIntervalSec = 10;
  if (settings.uploadIntervalSec > 3600) settings.uploadIntervalSec = 3600;

  LOG.println("[CFG] AP SSID: " + settings.apSsid);
  LOG.println("[CFG] STA SSID: " + (settings.staSsid.length() ? settings.staSsid : "(not configured)"));
  LOG.println("[CFG] Google Sheets: " + String(settingsSheetConfigured() ? "configured" : "not configured"));
}

const SettingsData &settingsGet() { return settings; }

bool settingsStaConfigured() { return settings.staSsid.length() > 0; }

bool settingsSheetConfigured() {
  return settings.sheetUrl.startsWith("https://") && settings.sheetToken.length() > 0;
}

bool settingsSave(const SettingsData &next, String &error, bool &apChanged,
                  bool &staChanged, bool &sheetChanged) {
  if (next.apSsid.length() < 1 || next.apSsid.length() > 32) {
    error = "AP SSID must be 1-32 characters";
    return false;
  }
  if (next.apPass.length() > 0 &&
      (next.apPass.length() < 8 || next.apPass.length() > 63)) {
    error = "AP password must be 8-63 characters, or empty for open";
    return false;
  }
  if (next.staSsid.length() > 32) {
    error = "Router SSID is too long";
    return false;
  }
  if (next.staPass.length() > 0 &&
      (next.staPass.length() < 8 || next.staPass.length() > 63)) {
    error = "Router password must be 8-63 characters";
    return false;
  }
  if (next.sheetUrl.length() > 0) {
    if (next.sheetUrl.length() > 250 || !next.sheetUrl.startsWith("https://")) {
      error = "Google Sheets URL must start with https://";
      return false;
    }
  }
  if (next.sheetToken.length() > 128) {
    error = "Google Sheets token is too long";
    return false;
  }
  if (next.uploadIntervalSec < 10 || next.uploadIntervalSec > 3600) {
    error = "Upload interval must be 10-3600 seconds";
    return false;
  }

  apChanged = next.apSsid != settings.apSsid || next.apPass != settings.apPass;
  staChanged = next.staSsid != settings.staSsid || next.staPass != settings.staPass;
  sheetChanged = next.sheetUrl != settings.sheetUrl ||
                 next.sheetToken != settings.sheetToken ||
                 next.uploadIntervalSec != settings.uploadIntervalSec;

  prefs.putString("ap_ssid", next.apSsid);
  prefs.putString("ap_pass", next.apPass);
  prefs.putString("sta_ssid", next.staSsid);
  prefs.putString("sta_pass", next.staPass);
  prefs.putString("sheet_url", next.sheetUrl);
  prefs.putString("sheet_tok", next.sheetToken);
  prefs.putUInt("interval", next.uploadIntervalSec);

  settings = next;

  LOG.printf("[CFG] Saved (AP %s, STA %s, sheet %s)\n",
             apChanged ? "changed" : "same",
             staChanged ? "changed" : "same",
             sheetChanged ? "changed" : "same");
  return true;
}

/**
 * @file settings.h
 * @brief Persistent AP / STA / Google Sheets settings (NVS).
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

struct SettingsData {
  String apSsid;
  String apPass;
  String staSsid;
  String staPass;
  String sheetUrl;
  String sheetToken;
  uint32_t uploadIntervalSec;
};

void settingsBegin();
const SettingsData &settingsGet();

/**
 * Validate and persist new settings.
 * @param next        complete settings (merge secrets before calling)
 * @param error       reason on failure
 * @param apChanged   true when AP SSID/password changed (needs reboot)
 * @param staChanged  true when STA SSID/password changed (reconnect)
 * @param sheetChanged true when Google Sheets URL/token or interval changed
 */
bool settingsSave(const SettingsData &next, String &error, bool &apChanged,
                  bool &staChanged, bool &sheetChanged);

bool settingsStaConfigured();
bool settingsSheetConfigured();

#endif // SETTINGS_H

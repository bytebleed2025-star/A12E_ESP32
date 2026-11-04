/**
 * @file sta_secrets.example.h
 * @brief Copy this file to sta_secrets.h and fill in your own values.
 *
 * sta_secrets.h is gitignored. Never commit real WiFi passwords or the
 * Google Apps Script token.
 */

#ifndef STA_SECRETS_H
#define STA_SECRETS_H

// Router WiFi (2.4 GHz only - ESP32 cannot connect to 5 GHz)
#define STA_WIFI_SSID "YOUR_ROUTER_2G4_SSID"
#define STA_WIFI_PASSWORD "YOUR_ROUTER_PASSWORD"

// Google Apps Script Web App URL (ends with /exec)
#define SHEET_WEBAPP_URL "https://script.google.com/macros/s/XXXXXXXXXXXX/exec"

// Shared secret checked by the Apps Script (must match Code.gs)
#define SHEET_WEBAPP_TOKEN "CHANGE_ME_TO_A_LONG_RANDOM_TOKEN"

#endif // STA_SECRETS_H

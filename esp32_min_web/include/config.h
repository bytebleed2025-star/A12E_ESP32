/**
 * @file config.h
 * @brief Configuration for the A12E Scale Monitor (minimal firmware)
 *
 * This project is standalone and does NOT share files with the main
 * A12E_ESP32 firmware.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// WiFi Access Point
// ============================================================================

// AP SSID shown to phones / tablets
#define WIFI_AP_SSID "A12E_SCALE_MONITOR"

// AP password - minimum 8 characters. Set to "" for an open network.
#define WIFI_AP_PASSWORD "33333333"

// ============================================================================
// Web server
// ============================================================================

#define WEB_SERVER_PORT 80

// How often the browser polls /api/status (milliseconds)
#define STATUS_UPDATE_INTERVAL 500

// ============================================================================
// Scale / display RS232 link
// ============================================================================
//
// Same electrical setup as the main board:
//   scale/display TX  -> MAX232 -> GPIO21 (ESP32 RX)
//   scale/display RX  <- MAX232 <- GPIO22 (ESP32 TX)   (optional for reading)
//   GND common
//
// If you wire the scale's TTL output directly (no MAX232), connect only:
//   scale TX -> GPIO21, GND -> GND  (never connect RS232 levels directly!)

#define SCALE_BAUD_RATE 9600
#define SCALE_RX_PIN 21
#define SCALE_TX_PIN 22
#define SCALE_UART_NUM 1

// A frame is considered lost/stale after this many milliseconds without data.
#define SCALE_TIMEOUT_MS 3000

// ============================================================================
// Google Sheets upload (AP+STA mode)
// ============================================================================

// The board always keeps its own AP. If the STA (router) credentials below are
// configured, it also connects to your router and uploads readings.
#define SHEET_UPLOAD_INTERVAL_MS 60000UL  // 1 row per minute while scale is valid
#define SHEET_HTTP_TIMEOUT_MS 10000       // HTTP connect/read timeout
#define SHEET_TIMEZONE "WIB-7"            // POSIX TZ for NTP (Indonesia = WIB-7)

// ============================================================================
// Debug serial (USB)
// ============================================================================

#define DEBUG_BAUD_RATE 115200

// ============================================================================
// Frame buffer
// ============================================================================

#define FRAME_BUFFER_SIZE 48

#endif // CONFIG_H

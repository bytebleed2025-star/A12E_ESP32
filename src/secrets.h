/**
 * @file secrets.h
 * @brief WiFi and network configuration
 * 
 * IMPORTANT: This file contains sensitive credentials.
 * - Add to .gitignore to prevent accidental commits
 * - Never share this file publicly
 */

#ifndef SECRETS_H
#define SECRETS_H

// ============================================================================
// WiFi Access Point Configuration
// ============================================================================

// AP SSID (Network Name) - visible to phones
#define WIFI_SSID "A12E_WEIGH SCALE_SSID1"

// AP Password - minimum 8 characters
#define WIFI_PASSWORD "11111111"

// ============================================================================
// Optional: WiFi Station Mode (if connecting to existing router)
// ============================================================================

// Uncomment and configure if you want to connect to existing WiFi
// #define WIFI_SSID_STATION "Your_Router_SSID"
// #define WIFI_PASSWORD_STATION "Your_Router_Password"

// ============================================================================
// Web Server Configuration
// ============================================================================

// Port for web server (default 80)
#define WEB_SERVER_PORT 80

// ============================================================================
// Serial Communication
// ============================================================================

// RS232 Baud Rate
#define SERIAL_BAUD_RATE 9600

// Debug Serial Baud Rate (USB)
#define DEBUG_BAUD_RATE 115200

// ============================================================================
// Hardware Pin Configuration
// ============================================================================

// Relay Pin 1 (GPIO16)
#define RELAY_PIN_1 16

// Relay Pin 2 (GPIO17)
#define RELAY_PIN_2 17

// Manual Mode Latching Push Button (GPIO13)
// HIGH (button ON/true) → Manual Mode (webpage ON/OFF controls relays)
#define MANUAL_MODE_PIN 13

// Auto Mode Momentary Push Button (GPIO14)
// Pressed (HIGH) + MANUAL_MODE_PIN LOW → Auto Mode (weight thresholds control relays)
#define AUTO_MODE_PIN 14

// RS232 RX Pin (GPIO21)
#define RS232_RX_PIN 21

// RS232 TX Pin (GPIO22)
#define RS232_TX_PIN 22

// UART Number for RS232
#define UART_NUM 1

// ============================================================================
// Weight Threshold Configuration (in kg)
// ============================================================================

// Relay 1 activation threshold
#define WEIGHT_THRESHOLD_RELAY1 20.0

// Relay 2 activation threshold
#define WEIGHT_THRESHOLD_RELAY2 200.0

// ============================================================================
// Buffer Configuration
// ============================================================================

// Serial RX buffer size
#define BUFFER_SIZE 128

// ============================================================================
// Web Interface Update Frequency
// ============================================================================

// Auto-refresh interval (milliseconds)
#define STATUS_UPDATE_INTERVAL 1000  // 1 second

#endif // SECRETS_H

/**
 * ============================================================================
 *  A12E Scale Monitor - minimal firmware (standalone)
 * ============================================================================
 *
 *  What it does:
 *    - Reads weight frames from the scale / display over RS232 (HardwareSerial)
 *    - Serves a small web page showing the live weight
 *    - Runs its own WiFi Access Point
 *
 *  What it does NOT do:
 *    - No relays, no thresholds, no modes, no LittleFS, no config file
 *
 *  This is a separate project from the main A12E_ESP32 firmware.
 *  Nothing in the parent project is used or modified.
 *
 *  Board    : ESP32 DevKit v1 (esp32dev)
 *  Scale TX : MAX232 -> GPIO21 (RX)
 *  Scale RX : MAX232 <- GPIO22 (TX)   [not required for read-only]
 *  GND      : common
 *
 *  Build    : pio run -d esp32_min_web
 *  Upload   : pio run -d esp32_min_web -t upload
 *  Monitor  : pio device monitor -d esp32_min_web
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "license.h"
#include "log.h"
#include "settings.h"
#include "net.h"
#include "sheets.h"
#include "web_ui.h"
#include "monitor_page.h"
#include "debug_page.h"
#include "config_page.h"

#define FW_VERSION "1.2.0"

// ============================================================================
// Globals
// ============================================================================

HardwareSerial ScaleSerial(SCALE_UART_NUM);
WebServer server(WEB_SERVER_PORT);

static float currentWeight = 0.0f;       // last valid weight in kg
static bool weightValid = false;         // at least one valid frame received
static char frameBuffer[FRAME_BUFFER_SIZE];
static size_t frameLen = 0;
static bool frameOverflow = false;

static char lastRawFrame[FRAME_BUFFER_SIZE] = "-";
static unsigned long lastFrameMs = 0;    // millis() of last valid frame (0 = none)
static uint32_t frameCount = 0;
static uint32_t badFrameCount = 0;
static unsigned long lastLogMs = 0;

// ============================================================================
// Embedded web page (no LittleFS / uploadfs needed)
// ============================================================================



// ============================================================================
// Scale frame parsing
// ============================================================================

static bool isScaleOnline() {
  return lastFrameMs != 0 && (millis() - lastFrameMs) <= SCALE_TIMEOUT_MS;
}

static bool isFrameStart(char c) {
  // Frames start with 'W' (weight) or a space, as in the A12E scale protocol.
  return c == 'W' || c == 'w' || c == ' ';
}

/**
 * Turn a raw frame into a weight.
 * Works with frames like "W 00004.3 kg" and also tolerates small variations
 * in length / spacing by extracting the numeric part.
 */
static void processFrame(const char *frame) {
  snprintf(lastRawFrame, sizeof(lastRawFrame), "%s", frame);

  String numeric;
  numeric.reserve(16);

  bool dotSeen = false;
  for (const char *p = frame; *p != '\0'; ++p) {
    const char c = *p;
    if (c >= '0' && c <= '9') {
      numeric += c;
    } else if (c == '.' && !dotSeen) {
      dotSeen = true;
      numeric += c;
    } else if (c == '-' && numeric.length() == 0) {
      numeric += c;
    }
  }

  if (numeric.length() == 0 || numeric == "-" || numeric == ".") {
    badFrameCount++;
    LOG.print("[SCALE] Invalid frame: ");
    LOG.println(frame);
    return;
  }

  const float value = numeric.toFloat();
  if (isnan(value) || isinf(value) || fabsf(value) > 100000.0f) {
    badFrameCount++;
    LOG.print("[SCALE] Out-of-range frame: ");
    LOG.println(frame);
    return;
  }

  currentWeight = value;
  weightValid = true;
  lastFrameMs = millis();
  frameCount++;

  LOG.printf("[SCALE] Weight: %.1f kg  raw=\"%s\"\n", currentWeight, frame);
}

static void feedScaleChar(char c) {
  if (c == '\r' || c == '\n') {
    if (frameLen > 0) {
      frameBuffer[frameLen] = '\0';
      if (frameOverflow) {
        badFrameCount++;
      } else {
        processFrame(frameBuffer);
      }
    }
    frameLen = 0;
    frameOverflow = false;
    return;
  }

  if (frameLen == 0 && !isFrameStart(c)) {
    return; // ignore noise before the frame start
  }

  if (frameLen < FRAME_BUFFER_SIZE - 1) {
    frameBuffer[frameLen++] = c;
  } else {
    frameOverflow = true;
  }
}

static void readScale() {
  while (ScaleSerial.available() > 0) {
    feedScaleChar(static_cast<char>(ScaleSerial.read()));
  }
}

// ============================================================================
// HTTP handlers
// ============================================================================

static void appendJsonEscaped(String &out, const char *s) {
  for (const char *p = s; *p != '\0'; ++p) {
    const char c = *p;
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      char esc[8];
      snprintf(esc, sizeof(esc), "\\u%04x", static_cast<unsigned char>(c));
      out += esc;
    } else {
      out += c;
    }
  }
}

static void handleStatus() {
  const bool online = isScaleOnline();
  const bool locked = licenseIsLocked();

  String json;
  json.reserve(640);
  json += F("{\"weight\":");
  json += String(locked ? 0.0f : currentWeight, 1);
  json += F(",\"valid\":");
  json += (online && weightValid && !locked) ? F("true") : F("false");
  json += F(",\"age\":");
  if (lastFrameMs == 0) {
    json += F("-1");
  } else {
    json += String(static_cast<long>(millis() - lastFrameMs));
  }
  json += F(",\"frames\":");
  json += String(frameCount);
  json += F(",\"badFrames\":");
  json += String(badFrameCount);
  json += F(",\"uptime\":");
  json += String(millis() / 1000UL);
  json += F(",\"raw\":\"");
  appendJsonEscaped(json, lastRawFrame);
  json += F("\"");
  json += F(",\"deviceId\":\"");
  appendJsonEscaped(json, licenseDeviceId().c_str());
  json += F("\",\"license\":\"");
  json += licenseStateName();
  json += F("\",\"trialRemaining\":");
  json += String(licenseTrialRemainingSec());
  json += F(",\"trialTotal\":");
  json += String(licenseTrialTotalSec());
  json += F(",\"locked\":");
  json += locked ? F("true") : F("false");
  json += F(",\"staConnected\":");
  json += netStaConnected() ? F("true") : F("false");
  json += F(",\"staConfigured\":");
  json += netStaConfigured() ? F("true") : F("false");
  json += F(",\"staStatus\":\"");
  appendJsonEscaped(json, netStaStatus().c_str());
  json += F("\",\"staIp\":\"");
  appendJsonEscaped(json, netStaIP().c_str());
  json += F("\",\"rssi\":");
  json += String(netRssi());
  json += F(",\"cloudConfigured\":");
  json += settingsSheetConfigured() ? F("true") : F("false");
  json += F(",\"lastUploadAge\":");
  json += String(sheetsLastUploadAgeMs());
  json += F(",\"lastUploadOk\":");
  json += sheetsLastUploadOk() ? F("true") : F("false");
  json += F(",\"uploadOk\":");
  json += String(sheetsUploadOkCount());
  json += F(",\"uploadFail\":");
  json += String(sheetsUploadFailCount());
  json += F("}");

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

static LogEntry logSnapshot[LOG_CAPACITY];

static void handleLog() {
  uint32_t since = 0;
  if (server.hasArg("since")) {
    const String value = server.arg("since");
    since = strtoul(value.c_str(), nullptr, 10);
  }

  const size_t copied = logCopySince(since, logSnapshot, LOG_CAPACITY);

  String json;
  json.reserve(96 + copied * (LOG_LINE_LENGTH + 48));
  json += F("{\"next\":");
  json += String(logNextSeq() - 1);
  json += F(",\"dropped\":");
  json += String(logDropped());
  json += F(",\"lines\":[");

  for (size_t i = 0; i < copied; ++i) {
    if (i > 0) json += ',';
    json += F("{\"seq\":");
    json += String(logSnapshot[i].seq);
    json += F(",\"ms\":");
    json += String(logSnapshot[i].ms);
    json += F(",\"text\":\"");
    appendJsonEscaped(json, logSnapshot[i].text);
    json += F("\"}");
  }

  json += F("]}");

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

static void handleDebug() {
  const String page =
      renderPage("A12E Debug Log", "debug", DEBUG_BODY, DEBUG_SCRIPT);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html", page);
}

static bool isApClient() {
  const IPAddress clientIp = server.client().remoteIP();
  return clientIp[0] == 192 && clientIp[1] == 168 && clientIp[2] == 4;
}

static void handleConfig() {
  if (!isApClient()) {
    server.send(403, "text/plain",
                "Config is available only from the device AP (192.168.4.1).");
    return;
  }

  const String page =
      renderPage("A12E Config", "config", CONFIG_BODY, CONFIG_SCRIPT);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html", page);
}

static void handleActivate() {
  String body = F("{\"ok\":false,\"error\":\"Missing key\"}");
  int code = 400;

  if (server.hasArg("key")) {
    String error;
    if (licenseActivate(server.arg("key"), error)) {
      body = F("{\"ok\":true}");
      code = 200;
    } else {
      body = F("{\"ok\":false,\"error\":\"");
      appendJsonEscaped(body, error.c_str());
      body += F("\"}");
      code = 403;
    }
  }

  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

static bool isConfigRequest() {
  return isApClient() && server.header("X-Requested-With") == "A12E";
}

static void handleSettingsGet() {
  if (!isApClient()) {
    server.send(403, "application/json",
                "{\"ok\":false,\"error\":\"Config is only available from the AP\"}");
    return;
  }

  const SettingsData &s = settingsGet();

  String json;
  json.reserve(640);
  json += F("{\"apSsid\":\"");
  appendJsonEscaped(json, s.apSsid.c_str());
  json += F("\",\"apPassSet\":");
  json += s.apPass.length() > 0 ? F("true") : F("false");
  json += F(",\"staSsid\":\"");
  appendJsonEscaped(json, s.staSsid.c_str());
  json += F("\",\"staPassSet\":");
  json += s.staPass.length() > 0 ? F("true") : F("false");
  json += F(",\"sheetUrl\":\"");
  appendJsonEscaped(json, s.sheetUrl.c_str());
  json += F("\",\"sheetTokenSet\":");
  json += s.sheetToken.length() > 0 ? F("true") : F("false");
  json += F(",\"interval\":");
  json += String(s.uploadIntervalSec);
  json += F(",\"apIp\":\"");
  appendJsonEscaped(json, netApIP().c_str());
  json += F("\",\"staStatus\":\"");
  appendJsonEscaped(json, netStaStatus().c_str());
  json += F("\",\"staConnected\":");
  json += netStaConnected() ? F("true") : F("false");
  json += F(",\"staIp\":\"");
  appendJsonEscaped(json, netStaIP().c_str());
  json += F("\",\"rssi\":");
  json += String(netRssi());
  json += F(",\"staConfigured\":");
  json += netStaConfigured() ? F("true") : F("false");
  json += F(",\"sheetConfigured\":");
  json += settingsSheetConfigured() ? F("true") : F("false");
  json += F("}");

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

static void handleSettingsPost() {
  if (!isConfigRequest()) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"Forbidden\"}");
    return;
  }

  SettingsData next = settingsGet();

  next.apSsid = server.arg("apSsid");
  next.apSsid.trim();
  if (server.hasArg("apPass") && server.arg("apPass").length() > 0) {
    next.apPass = server.arg("apPass");
  }

  next.staSsid = server.arg("staSsid");
  next.staSsid.trim();
  if (server.hasArg("staPass") && server.arg("staPass").length() > 0) {
    next.staPass = server.arg("staPass");
  }

  next.sheetUrl = server.arg("sheetUrl");
  next.sheetUrl.trim();
  if (server.hasArg("sheetToken") && server.arg("sheetToken").length() > 0) {
    next.sheetToken = server.arg("sheetToken");
  }

  if (server.hasArg("interval")) {
    next.uploadIntervalSec = server.arg("interval").toInt();
  }

  String error;
  bool apChanged = false;
  bool staChanged = false;
  bool sheetChanged = false;

  if (!settingsSave(next, error, apChanged, staChanged, sheetChanged)) {
    String body = F("{\"ok\":false,\"error\":\"");
    appendJsonEscaped(body, error.c_str());
    body += F("\"}");
    server.send(400, "application/json", body);
    return;
  }

  if (apChanged) {
    netScheduleReboot(1800);
  } else if (staChanged) {
    netRestartSta();
  }

  if (sheetChanged || staChanged) {
    sheetsResetSchedule();
  }

  String body = F("{\"ok\":true,\"reboot\":");
  body += apChanged ? F("true") : F("false");
  body += F("}");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", body);
}

static void handleScan() {
  if (!isConfigRequest()) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"Forbidden\"}");
    return;
  }

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", netScanJson());
}

static void handleSheetTest() {
  if (!isConfigRequest()) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"Forbidden\"}");
    return;
  }

  String message;
  int httpCode = 0;
  const bool ok = sheetsTest(message, httpCode);

  String body = F("{\"ok\":");
  body += ok ? F("true") : F("false");
  body += F(",\"code\":");
  body += String(httpCode);
  body += F(",\"message\":\"");
  appendJsonEscaped(body, message.c_str());
  body += F("\"}");

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", body);
}

static void handleRoot() {
  const String page =
      renderPage("A12E Scale Monitor", "monitor", MONITOR_BODY, MONITOR_SCRIPT);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html", page);
}

static void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
  Serial.begin(DEBUG_BAUD_RATE);
  delay(300);

  LOG.println();
  LOG.println("========================================");
  LOG.println("   A12E SCALE MONITOR (minimal)");
  LOG.println("   Firmware v" FW_VERSION);
  LOG.println("========================================");
  LOG.println("[SCALE] UART" + String(SCALE_UART_NUM) +
                 "  RX=GPIO" + String(SCALE_RX_PIN) +
                 "  TX=GPIO" + String(SCALE_TX_PIN) +
                 "  " + String(SCALE_BAUD_RATE) + " 8N1");

  ScaleSerial.begin(SCALE_BAUD_RATE, SERIAL_8N1, SCALE_RX_PIN, SCALE_TX_PIN);

  settingsBegin();
  licenseBegin();
  netBegin();
  sheetsBegin();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/debug", HTTP_GET, handleDebug);
  server.on("/config", HTTP_GET, handleConfig);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/activate", HTTP_GET, handleActivate);
  server.on("/api/activate", HTTP_POST, handleActivate);
  server.on("/api/settings", HTTP_GET, handleSettingsGet);
  server.on("/api/settings", HTTP_POST, handleSettingsPost);
  server.on("/api/wifi/scan", HTTP_POST, handleScan);
  server.on("/api/sheets/test", HTTP_POST, handleSheetTest);
  server.on("/favicon.ico", HTTP_GET, []() { server.send(204); });
  server.onNotFound(handleNotFound);
  server.begin();

  LOG.println("[WEB] Server started on port " + String(WEB_SERVER_PORT));
  LOG.println("[SYSTEM] Ready - waiting for scale frames");
}

void loop() {
  readScale();
  licenseLoop();
  server.handleClient();
  netLoop();
  sheetsLoop(currentWeight, weightValid && isScaleOnline());

  if (millis() - lastLogMs >= 1000UL) {
    lastLogMs = millis();
    if (isScaleOnline()) {
      LOG.printf("[SCALE] %.1f kg  (frames=%lu, bad=%lu)\n",
                    currentWeight,
                    static_cast<unsigned long>(frameCount),
                    static_cast<unsigned long>(badFrameCount));
    } else {
      LOG.println("[SCALE] No data - check RS232 RX wiring / baud rate");
    }
  }

  delay(1);
}

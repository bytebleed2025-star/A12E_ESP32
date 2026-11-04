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
#include "debug_page.h"

#define FW_VERSION "1.1.0"

// Used to inject STATUS_UPDATE_INTERVAL into the embedded page.
#define STRINGIFY_INNER(x) #x
#define STRINGIFY(x) STRINGIFY_INNER(x)

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

static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="id">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>A12E Scale Monitor</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 16px;
    }
    .card {
      background: #fff;
      border-radius: 20px;
      box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
      padding: 32px;
      max-width: 460px;
      width: 100%;
    }
    h1 { text-align: center; color: #333; font-size: 26px; }
    .subtitle {
      text-align: center; color: #999; font-size: 13px;
      margin: 6px 0 22px;
    }
    .badge-row { text-align: center; margin-bottom: 16px; }
    .badge {
      display: inline-block; padding: 6px 14px; border-radius: 999px;
      background: #e8f5e9; color: #2e7d32;
      font-size: 11px; font-weight: 700;
      letter-spacing: 1px; text-transform: uppercase;
      transition: all 0.3s ease;
    }
    .badge.off { background: #ffebee; color: #c62828; }
    .display {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: #fff;
      border-radius: 16px;
      padding: 36px 20px;
      text-align: center;
      box-shadow: 0 10px 30px rgba(102, 126, 234, 0.3);
    }
    .label {
      font-size: 12px; letter-spacing: 2px; text-transform: uppercase;
      opacity: 0.85; margin-bottom: 10px;
    }
    .value {
      font-family: 'Courier New', monospace;
      font-size: 58px; font-weight: 700;
      letter-spacing: 2px; line-height: 1;
      transition: opacity 0.3s ease;
    }
    .value.stale { opacity: 0.35; }
    .unit { font-size: 22px; font-weight: 600; margin-left: 8px; }
    .meta {
      margin-top: 22px; font-size: 13px; color: #555;
      display: grid; gap: 8px;
    }
    .row { display: flex; justify-content: space-between; gap: 12px; }
    .k { color: #999; white-space: nowrap; }
    .v {
      font-family: 'Courier New', monospace;
      font-weight: 600; text-align: right; word-break: break-all;
    }
    .footer {
      text-align: center; color: #bbb; font-size: 11px;
      margin-top: 22px; line-height: 1.7;
    }
    .debug-link {
      color: #667eea; text-decoration: none; font-weight: 600;
    }
    .debug-link:hover { text-decoration: underline; }
    .lock-note {
      display: none;
      background: #ffebee; color: #c62828;
      border-left: 4px solid #c62828;
      border-radius: 8px; padding: 12px 14px;
      font-size: 13px; font-weight: 600;
      margin-bottom: 16px;
    }
    .license {
      margin-top: 20px; padding-top: 18px;
      border-top: 1px solid #eee;
    }
    .license-head {
      display: flex; justify-content: space-between; align-items: center;
      margin-bottom: 12px;
    }
    .license-title { font-size: 13px; font-weight: 700; color: #333; }
    .pill {
      display: inline-block; padding: 4px 10px; border-radius: 999px;
      background: #eef2ff; color: #4b5bd6;
      font-size: 10px; font-weight: 700;
      letter-spacing: 1px; text-transform: uppercase;
    }
    .pill.licensed { background: #e8f5e9; color: #2e7d32; }
    .pill.expired { background: #ffebee; color: #c62828; }
    .lrow {
      display: flex; justify-content: space-between; align-items: center;
      gap: 10px; font-size: 13px; margin-bottom: 10px;
    }
    .dev-block { margin-bottom: 10px; }
    .dev-block .k { display: block; font-size: 13px; margin-bottom: 6px; }
    .dev-row { display: flex; gap: 8px; align-items: center; }
    .devid {
      flex: 1; min-width: 0; padding: 6px 8px;
      border: 1px solid #e5e7f0; border-radius: 6px;
      background: #f7f8ff; color: #333;
      font-family: 'Courier New', monospace;
      font-size: 13px; font-weight: 700; letter-spacing: 1px;
      text-align: center;
    }
    .btn-copy {
      padding: 6px 10px; border: 1px solid #d7dcef; border-radius: 6px;
      background: #fff; color: #5b6bd6;
      font-size: 10px; font-weight: 700; letter-spacing: 1px;
      cursor: pointer;
    }
    .act-row { display: flex; gap: 8px; margin-top: 4px; }
    .key-input {
      flex: 1; min-width: 0;
      padding: 9px 10px;
      border: 2px solid #eee; border-radius: 8px;
      font-family: 'Courier New', monospace; font-size: 11px;
      resize: none; outline: none;
      transition: border-color 0.3s ease;
    }
    .key-input:focus { border-color: #667eea; }
    .btn-activate {
      padding: 0 14px; border: none; border-radius: 8px;
      background: #667eea; color: #fff;
      font-size: 11px; font-weight: 700; letter-spacing: 1px;
      cursor: pointer; transition: all 0.3s ease;
    }
    .btn-activate:hover:not(:disabled) { background: #5a6fd6; }
    .btn-activate:disabled { opacity: 0.6; cursor: not-allowed; }
    .msg { font-size: 12px; margin-top: 8px; min-height: 16px; }
    .msg.ok { color: #2e7d32; }
    .msg.err { color: #c62828; }
    .licensed-msg {
      display: none;
      background: #e8f5e9; color: #2e7d32;
      border-radius: 8px; padding: 10px 12px;
      font-size: 13px; font-weight: 600;
      margin-top: 8px;
    }
    @media (max-width: 480px) {
      .card { padding: 22px; }
      .value { font-size: 46px; }
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>&#9878;&#65039; A12E Scale Monitor</h1>
    <div class="subtitle" id="host">ESP32 weight monitor</div>

    <div class="badge-row">
      <span class="badge" id="badge">Menunggu timbangan</span>
    </div>

    <div class="lock-note" id="lockNote">
      Trial expired. Enter an activation key below to continue using this device.
    </div>

    <div class="display">
      <div class="label">Timbangan Terukur</div>
      <div class="value" id="valueBox">
        <span id="weight">--</span><span class="unit">kg</span>
      </div>
    </div>

    <div class="meta">
      <div class="row"><span class="k">Raw frame</span><span class="v" id="raw">-</span></div>
      <div class="row"><span class="k">Frames received</span><span class="v" id="frames">0</span></div>
      <div class="row"><span class="k">Last update</span><span class="v" id="age">-</span></div>
      <div class="row"><span class="k">Uptime</span><span class="v" id="uptime">-</span></div>
    </div>

    <div class="license">
      <div class="license-head">
        <span class="license-title">License</span>
        <span class="pill" id="licensePill">Trial</span>
      </div>

      <div class="dev-block">
        <span class="k">Device ID</span>
        <div class="dev-row">
          <input class="devid" id="deviceId" readonly value="...">
          <button class="btn-copy" onclick="copyDeviceId(this)">COPY</button>
        </div>
      </div>

      <div class="lrow">
        <span class="k">Trial left</span>
        <span class="v" id="trialLeft">--</span>
      </div>

      <div id="activateArea">
        <div class="act-row">
          <textarea class="key-input" id="keyInput" rows="2"
                    placeholder="Paste activation key"
                    spellcheck="false" autocomplete="off"></textarea>
          <button class="btn-activate" id="activateBtn"
                  onclick="activateLicense()">ACTIVATE</button>
        </div>
        <div class="msg" id="activateMsg"></div>
      </div>

      <div class="licensed-msg" id="licensedMsg">
        Licensed to this device. Thank you!
      </div>
    </div>

    <div class="footer">
      <div>RS232 &rarr; ESP32 &rarr; Web (read only)</div>
      <div>Develop by GTi_Dev_ShD</div>
      <div><a class="debug-link" href="/debug">Open debug log &rarr;</a></div>
    </div>
  </div>

  <script>
    const POLL_MS = )HTML" STRINGIFY(STATUS_UPDATE_INTERVAL) R"HTML(;
    const $ = function (id) { return document.getElementById(id); };

    $('host').textContent = 'http://' + location.host;

    function fmtUptime(sec) {
      sec = Math.floor(sec);
      const h = Math.floor(sec / 3600);
      const m = Math.floor((sec % 3600) / 60);
      const s = sec % 60;
      return (h > 0 ? h + 'h ' : '') + m + 'm ' + s + 's';
    }

    function setBadge(online, valid, locked) {
      const badge = $('badge');
      if (locked) {
        badge.textContent = 'Trial expired';
        badge.className = 'badge off';
      } else if (online && valid) {
        badge.textContent = 'Live';
        badge.className = 'badge';
      } else if (online) {
        badge.textContent = 'Menunggu timbangan';
        badge.className = 'badge';
      } else {
        badge.textContent = 'ESP32 offline';
        badge.className = 'badge off';
      }
    }

    function fmtTrial(sec) {
      sec = Math.max(0, Math.floor(sec));
      if (sec <= 0) return 'expired';
      const h = Math.floor(sec / 3600);
      const m = Math.floor((sec % 3600) / 60);
      const s = sec % 60;
      if (h > 0) return h + 'h ' + m + 'm';
      if (m > 0) return m + 'm ' + s + 's';
      return s + 's';
    }

    function updateLicenseUI(d) {
      const devEl = $('deviceId');
      const id = d.deviceId || '';
      if (devEl.value !== id) devEl.value = id;

      const pill = $('licensePill');
      const locked = !!d.locked;
      $('lockNote').style.display = locked ? 'block' : 'none';

      if (d.license === 'licensed') {
        pill.textContent = 'Licensed';
        pill.className = 'pill licensed';
        $('trialLeft').textContent = 'Unlimited';
        $('activateArea').style.display = 'none';
        $('licensedMsg').style.display = 'block';
      } else {
        $('licensedMsg').style.display = 'none';
        $('activateArea').style.display = 'block';
        $('trialLeft').textContent = (d.license === 'expired')
          ? 'expired'
          : fmtTrial(d.trialRemaining);
        pill.textContent = (d.license === 'expired') ? 'Expired' : 'Trial';
        pill.className = 'pill ' + ((d.license === 'expired') ? 'expired' : 'trial');
      }
    }

    function showMsg(text, ok) {
      const el = $('activateMsg');
      el.textContent = text;
      el.className = 'msg' + (ok === true ? ' ok' : (ok === false ? ' err' : ''));
    }

    function activateLicense() {
      const input = $('keyInput');
      const btn = $('activateBtn');
      const key = input.value.trim();

      if (!key) {
        showMsg('Paste the activation key first.', false);
        return;
      }

      btn.disabled = true;
      input.disabled = true;
      showMsg('Checking key...', null);

      fetch('/api/activate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'key=' + encodeURIComponent(key)
      })
        .then(function (r) {
          return r.json().then(function (d) { return { ok: r.ok, data: d }; });
        })
        .then(function (res) {
          if (res.ok && res.data.ok) {
            input.value = '';
            showMsg('Activated. Thank you!', true);
            poll();
          } else {
            showMsg(res.data.error || 'Invalid key for this device.', false);
          }
        })
        .catch(function () {
          showMsg('Connection error. Try again.', false);
        })
        .finally(function () {
          btn.disabled = false;
          input.disabled = false;
        });
    }

    function copyDeviceId(btn) {
      const el = $('deviceId');
      const old = btn.textContent;
      el.readOnly = false;
      el.select();
      el.setSelectionRange(0, el.value.length);
      let copied = false;
      try {
        copied = document.execCommand('copy');
      } catch (e) {
        copied = false;
      }
      el.readOnly = true;
      btn.textContent = copied ? 'COPIED' : 'SELECT';
      setTimeout(function () { btn.textContent = old; }, 1500);
    }

    function poll() {
      fetch('/api/status', { cache: 'no-store' })
        .then(function (r) {
          if (!r.ok) throw new Error('HTTP ' + r.status);
          return r.json();
        })
        .then(function (d) {
          const weightEl = $('weight');
          const valueBox = $('valueBox');
          if (d.valid) {
            weightEl.textContent = d.weight.toFixed(1);
            valueBox.className = 'value';
          } else {
            weightEl.textContent = '--';
            valueBox.className = 'value stale';
          }

          $('raw').textContent = d.raw || '-';
          $('frames').textContent = d.frames;
          $('age').textContent = d.age < 0
            ? 'no data'
            : (d.age / 1000).toFixed(1) + ' s ago';
          $('uptime').textContent = fmtUptime(d.uptime);
          updateLicenseUI(d);

          setBadge(true, d.valid, d.locked);
        })
        .catch(function () {
          $('weight').textContent = '--';
          $('valueBox').className = 'value stale';
          setBadge(false, false, false);
        });
    }

    poll();
    setInterval(poll, POLL_MS);
  </script>
</body>
</html>)HTML";

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
  json.reserve(384);
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
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", DEBUG_HTML);
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

static void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ============================================================================
// Setup
// ============================================================================

static void startAccessPoint() {
  WiFi.mode(WIFI_AP);

  const bool ok = (strlen(WIFI_AP_PASSWORD) >= 8)
                      ? WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD)
                      : WiFi.softAP(WIFI_AP_SSID);

  LOG.println("[WIFI] Starting Access Point...");
  if (!ok) {
    LOG.println("[WIFI] ERROR: softAP failed");
    return;
  }

  const IPAddress ip = WiFi.softAPIP();
  LOG.println("[WIFI] SSID: " + String(WIFI_AP_SSID));
  LOG.println("[WIFI] Password: " + String(strlen(WIFI_AP_PASSWORD) ? WIFI_AP_PASSWORD : "(open)"));
  LOG.println("[WIFI] Open http://" + ip.toString() + " on your phone");
}

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

  startAccessPoint();
  licenseBegin();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/debug", HTTP_GET, handleDebug);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/activate", HTTP_GET, handleActivate);
  server.on("/api/activate", HTTP_POST, handleActivate);
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

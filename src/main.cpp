#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "secrets.h"

// Add these two headers for brownout control
//#include "soc/soc.h"
//#include "soc/rtc_cntl_reg.h"

// RS232 Serial initialization
HardwareSerial RS232Serial(UART_NUM);

// Global variables
char rxBuffer[BUFFER_SIZE];
int bufferIndex = 0;

float currentWeight = 0.0;
bool relay1Status = false;
bool relay2Status = false;

// Runtime threshold values (adjustable via web UI)
float thresholdRelay1 = WEIGHT_THRESHOLD_RELAY1;
float thresholdRelay2 = WEIGHT_THRESHOLD_RELAY2;

// Auto/manual mode per relay (default: auto)
bool autoMode1 = true;
bool autoMode2 = true;

// Web server
AsyncWebServer server(WEB_SERVER_PORT);

// ============================================================================
// Configuration Persistence
// ============================================================================

#define CONFIG_FILE "/config.json"

void saveConfig();

void loadConfig() {
  if (!LittleFS.exists(CONFIG_FILE)) {
    Serial.println("[CONFIG] No config file found, using defaults");
    saveConfig();
    return;
  }

  File configFile = LittleFS.open(CONFIG_FILE, "r");
  if (!configFile) {
    Serial.println("[CONFIG] ERROR: Failed to open config file");
    return;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  if (!error) {
    if (doc.containsKey("threshold1")) thresholdRelay1 = doc["threshold1"].as<float>();
    if (doc.containsKey("threshold2")) thresholdRelay2 = doc["threshold2"].as<float>();
    if (doc.containsKey("autoMode1")) autoMode1 = doc["autoMode1"].as<bool>();
    if (doc.containsKey("autoMode2")) autoMode2 = doc["autoMode2"].as<bool>();
    Serial.println("[CONFIG] Loaded thresholds: Relay1=" + String(thresholdRelay1) + "kg, Relay2=" + String(thresholdRelay2) + "kg");
    Serial.println("[CONFIG] Modes: Relay1=" + String(autoMode1 ? "auto" : "manual") + ", Relay2=" + String(autoMode2 ? "auto" : "manual"));
  } else {
    Serial.println("[CONFIG] ERROR: Failed to parse config JSON");
  }

  configFile.close();
}

void saveConfig() {
  StaticJsonDocument<256> doc;
  doc["threshold1"] = thresholdRelay1;
  doc["threshold2"] = thresholdRelay2;
  doc["autoMode1"] = autoMode1;
  doc["autoMode2"] = autoMode2;

  File configFile = LittleFS.open(CONFIG_FILE, "w");
  if (!configFile) {
    Serial.println("[CONFIG] ERROR: Failed to open config file for writing");
    return;
  }

  serializeJson(doc, configFile);
  configFile.close();

  Serial.println("[CONFIG] Saved config");
}

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Print system info to serial monitor
 */
void printSystemInfo() {
  Serial.println("\n========================================");
  Serial.println("    ESP32 SCALE CONTROL SYSTEM v1.0");
  Serial.println("========================================");
  Serial.print("Chip Model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("Chip Revision: ");
  Serial.println(ESP.getChipRevision());
  Serial.print("Flash Size: ");
  Serial.print(ESP.getFlashChipSize() / 1024 / 1024);
  Serial.println(" MB");
  Serial.print("Free Heap: ");
  Serial.print(ESP.getFreeHeap() / 1024);
  Serial.println(" KB");
  Serial.println("========================================\n");
}

/**
 * @brief Initialize relay pins and set initial state
 */
void initializeRelays() {
  Serial.println("[RELAYS] Initializing relay pins...");
  pinMode(RELAY_PIN_1, OUTPUT);
  pinMode(RELAY_PIN_2, OUTPUT);
  
  // Set initial state to LOW (off)
  digitalWrite(RELAY_PIN_1, LOW);
  digitalWrite(RELAY_PIN_2, LOW);
  
  Serial.println("[RELAYS] Relay 1 (GPIO" + String(RELAY_PIN_1) + ") - OFF");
  Serial.println("[RELAYS] Relay 2 (GPIO" + String(RELAY_PIN_2) + ") - OFF");
}

/**
 * @brief Initialize RS232 serial communication
 */
void initializeRS232() {
  Serial.println("[RS232] Initializing RS232 communication...");
  RS232Serial.begin(SERIAL_BAUD_RATE, SERIAL_8N1, RS232_RX_PIN, RS232_TX_PIN);
  Serial.println("[RS232] Baud Rate: " + String(SERIAL_BAUD_RATE));
  Serial.println("[RS232] RX Pin: GPIO" + String(RS232_RX_PIN));
  Serial.println("[RS232] TX Pin: GPIO" + String(RS232_TX_PIN));
}

/**
 * @brief Initialize LittleFS filesystem
 */
void initializeLittleFS() {
  Serial.println("[FS] Mounting LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("[FS] ERROR: LittleFS mount failed!");
    return;
  }
  
  Serial.println("[FS] LittleFS mounted successfully");
  
  // Load persisted configuration
  loadConfig();
  
  // List files
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  if (!file) {
    Serial.println("[FS] WARNING: No files found in LittleFS");
  } else {
    Serial.println("[FS] Files in LittleFS:");
    while (file) {
      Serial.println("     - " + String(file.name()) + " (" + String(file.size()) + " bytes)");
      file = root.openNextFile();
    }
  }
}

/**
 * @brief Initialize WiFi Access Point
 */
void initializeWiFiAP() {
  Serial.println("[WiFi] Setting up Access Point...");
  WiFi.mode(WIFI_AP);
  
  bool apStarted = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  
  if (!apStarted) {
    Serial.println("[WiFi] ERROR: Failed to start AP!");
    return;
  }
  
  IPAddress IP = WiFi.softAPIP();
  Serial.println("[WiFi] Access Point started successfully!");
  Serial.println("[WiFi] SSID: " + String(WIFI_SSID));
  Serial.println("[WiFi] Password: " + String(WIFI_PASSWORD));
  Serial.println("[WiFi] IP Address: " + IP.toString());
  Serial.println("[WiFi] Connect your phone to WiFi and open http://" + IP.toString());
}

// ============================================================================
// Web Server Handlers
// ============================================================================

/**
 * @brief Handle status request - returns JSON with current state
 */
void handleStatus(AsyncWebServerRequest *request) {
  StaticJsonDocument<256> doc;
  doc["weight"] = currentWeight;
  doc["relay1"] = relay1Status;
  doc["relay2"] = relay2Status;
  doc["relay1_threshold"] = thresholdRelay1;
  doc["relay2_threshold"] = thresholdRelay2;
  doc["autoMode1"] = autoMode1;
  doc["autoMode2"] = autoMode2;
  
  String response;
  serializeJson(doc, response);
  
  request->send(200, "application/json", response);
}

/**
 * @brief Handle relay control request
 * Parameters: relay (1 or 2), state (0 or 1)
 */
void handleRelay(AsyncWebServerRequest *request) {
  if (request->hasParam("relay") && request->hasParam("state")) {
    int relay = request->getParam("relay")->value().toInt();
    int state = request->getParam("state")->value().toInt();
    
    Serial.println("[WEB] Relay control request - Relay: " + String(relay) + ", State: " + String(state));
    
    if (relay == 1) {
      digitalWrite(RELAY_PIN_1, state ? HIGH : LOW);
      relay1Status = (state ? true : false);
      Serial.println("[RELAYS] Relay 1 turned " + String(relay1Status ? "ON" : "OFF"));
    } 
    else if (relay == 2) {
      digitalWrite(RELAY_PIN_2, state ? HIGH : LOW);
      relay2Status = (state ? true : false);
      Serial.println("[RELAYS] Relay 2 turned " + String(relay2Status ? "ON" : "OFF"));
    }
  }
  
  handleStatus(request);
}

/**
 * @brief Handle threshold update request
 * Parameters: relay (1 or 2), value (float in kg)
 */
void handleSetThreshold(AsyncWebServerRequest *request) {
  if (request->hasParam("relay") && request->hasParam("value")) {
    int relay = request->getParam("relay")->value().toInt();
    float value = request->getParam("value")->value().toFloat();

    Serial.println("[WEB] Threshold update - Relay: " + String(relay) + ", Value: " + String(value) + " kg");

    if (relay == 1) {
      thresholdRelay1 = value;
      Serial.println("[CONFIG] Threshold 1 set to " + String(thresholdRelay1) + " kg");
    } else if (relay == 2) {
      thresholdRelay2 = value;
      Serial.println("[CONFIG] Threshold 2 set to " + String(thresholdRelay2) + " kg");
    }

    saveConfig();
  }

  handleStatus(request);
}

/**
 * @brief Handle mode switch request
 * Parameters: relay (1 or 2), mode ("auto" or "manual")
 */
void handleSetMode(AsyncWebServerRequest *request) {
  if (request->hasParam("relay") && request->hasParam("mode")) {
    int relay = request->getParam("relay")->value().toInt();
    String mode = request->getParam("mode")->value();
    bool isAuto = (mode == "auto");

    Serial.println("[WEB] Mode switch - Relay: " + String(relay) + ", Mode: " + mode);

    if (relay == 1) {
      autoMode1 = isAuto;
      if (autoMode1) {
        bool newState = (currentWeight < thresholdRelay1);
        digitalWrite(RELAY_PIN_1, newState ? HIGH : LOW);
        relay1Status = newState;
      }
    } else if (relay == 2) {
      autoMode2 = isAuto;
      if (autoMode2) {
        bool newState = (currentWeight < thresholdRelay2);
        digitalWrite(RELAY_PIN_2, newState ? HIGH : LOW);
        relay2Status = newState;
      }
    }

    saveConfig();
  }

  handleStatus(request);
}

/**
 * @brief Handle 404 errors
 */
void handleNotFound(AsyncWebServerRequest *request) {
  Serial.println("[WEB] 404 - Not Found: " + request->url());
  request->send(404, "text/plain", "File Not Found");
}

/**
 * @brief Initialize web server routes
 */
void initializeWebServer() {
  Serial.println("[WEB] Initializing web server...");
  
  // API endpoints (must be registered before static handler)
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/relay", HTTP_GET, handleRelay);
  server.on("/api/threshold", HTTP_GET, handleSetThreshold);
  server.on("/api/mode", HTTP_GET, handleSetMode);
  
  // Serve static files from LittleFS (index.html as default)
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  
  // 404 handler
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("[WEB] Web server started on port " + String(WEB_SERVER_PORT));
}

// ============================================================================
// RS232 Frame Processing
// ============================================================================

/**
 * @brief Process weight frame from RS232 scale
 * Frame format: W n 0 0 0 0 0 4 . 3 k g
 * Positions:   0 1 2 3 4 5 6 7 8 9 10 11
 */
void processFrame(const char *frame) {
  if (frame[0] == 'W' || frame[0] == 'w') {
    String weightStr = "";
    
    // Extract weight digits from positions 2-7
    for (int i = 2; i <= 7; i++) {
      if (frame[i] != ' ') {
        weightStr += frame[i];
      }
    }
    
    // Add decimal point and decimal digit
    weightStr += ".";
    weightStr += frame[9];
    
    currentWeight = weightStr.toFloat();
    
    Serial.print("[SCALE] Weight: ");
    Serial.print(currentWeight);
    Serial.println(" kg");

    // Auto-control relays based on weight thresholds (only in auto mode)
    // In auto mode: weight below threshold = ON, weight at/above threshold = OFF
    if (autoMode1) {
      bool newRelay1Status = (currentWeight < thresholdRelay1);
      if (newRelay1Status != relay1Status) {
        relay1Status = newRelay1Status;
        digitalWrite(RELAY_PIN_1, relay1Status ? HIGH : LOW);
        Serial.println("[RELAYS] Relay 1 auto: " + String(relay1Status ? "ON" : "OFF") + " (weight " + String(currentWeight) + "kg, threshold " + String(thresholdRelay1) + "kg)");
      }
    }
    
    if (autoMode2) {
      bool newRelay2Status = (currentWeight < thresholdRelay2);
      if (newRelay2Status != relay2Status) {
        relay2Status = newRelay2Status;
        digitalWrite(RELAY_PIN_2, relay2Status ? HIGH : LOW);
        Serial.println("[RELAYS] Relay 2 auto: " + String(relay2Status ? "ON" : "OFF") + " (weight " + String(currentWeight) + "kg, threshold " + String(thresholdRelay2) + "kg)");
      }
    }
  }
}

/**
 * @brief Read and process RS232 data
 */
void readRS232Data() {
  while (RS232Serial.available()) {
    char c = RS232Serial.read();

    // Detect frame start
    if ((c == 'W' || c == 'w' || c == ' ') && bufferIndex == 0) {
      bufferIndex = 1;
      rxBuffer[0] = c;
    }
    else if (bufferIndex > 0) {
      if (c == '\r') {
        // Frame end detected
        if (bufferIndex == 11) {
          rxBuffer[bufferIndex] = '\0';
          processFrame(rxBuffer);
        }
        // Reset buffer
        bufferIndex = 0;
        memset(rxBuffer, 0, BUFFER_SIZE);
      }
      else if (c != '\n') {
        // Add character to buffer
        if (bufferIndex < BUFFER_SIZE - 1) {
          rxBuffer[bufferIndex++] = c;
        }
      }
    }
  }
}

// ============================================================================
// Setup and Loop
// ============================================================================

void setup() {

  // Disable brownout detector (diagnostic only!)
  // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  // Initialize debug serial (USB)
  Serial.begin(DEBUG_BAUD_RATE);
  delay(1000);
  
  // Print system information
  printSystemInfo();
  
  // Initialize hardware
  initializeRelays();
  initializeRS232();
  initializeLittleFS();
  initializeWiFiAP();
  initializeWebServer();
  
  Serial.println("[SYSTEM] Setup complete - System ready!");
}

void loop() {
  // Read and process RS232 data (non-blocking)
  readRS232Data();
  
  // Small delay to prevent watchdog issues
  delay(1);
}

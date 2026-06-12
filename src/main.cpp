#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "secrets.h"

// RS232 Serial initialization
HardwareSerial RS232Serial(UART_NUM);

// Global variables
char rxBuffer[BUFFER_SIZE];
int bufferIndex = 0;

float currentWeight = 0.0;
bool relay1Status = false;
bool relay2Status = false;

// Web server
AsyncWebServer server(WEB_SERVER_PORT);

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
  StaticJsonDocument<200> doc;
  doc["weight"] = currentWeight;
  doc["relay1"] = relay1Status;
  doc["relay2"] = relay2Status;
  doc["relay1_threshold"] = WEIGHT_THRESHOLD_RELAY1;
  doc["relay2_threshold"] = WEIGHT_THRESHOLD_RELAY2;
  
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
  
  // Serve static files from LittleFS (index.html as default)
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  
  // API endpoints
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/relay", HTTP_GET, handleRelay);
  
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

    // Auto-control relays based on weight thresholds
    bool newRelay1Status = (currentWeight > WEIGHT_THRESHOLD_RELAY1);
    bool newRelay2Status = (currentWeight > WEIGHT_THRESHOLD_RELAY2);
    
    // Update Relay 1 if status changed
    if (newRelay1Status != relay1Status) {
      relay1Status = newRelay1Status;
      digitalWrite(RELAY_PIN_1, relay1Status ? HIGH : LOW);
      Serial.println("[RELAYS] Relay 1 auto-activated: " + String(relay1Status ? "ON" : "OFF"));
    }
    
    // Update Relay 2 if status changed
    if (newRelay2Status != relay2Status) {
      relay2Status = newRelay2Status;
      digitalWrite(RELAY_PIN_2, relay2Status ? HIGH : LOW);
      Serial.println("[RELAYS] Relay 2 auto-activated: " + String(relay2Status ? "ON" : "OFF"));
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

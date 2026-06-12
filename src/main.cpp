#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

HardwareSerial RS232Serial(1);

const int relayPin1 = 16;
const int relayPin2 = 17;

#define BUFFER_SIZE 128
char rxBuffer[BUFFER_SIZE];
int bufferIndex = 0;

float currentWeight = 0.0;
bool relay1Status = false;
bool relay2Status = false;

const char* ssid = "ESP32_Scale";
const char* password = "12345678";

AsyncWebServer server(80);

void handleStatus(AsyncWebServerRequest *request) {
  StaticJsonDocument<200> doc;
  doc["weight"] = currentWeight;
  doc["relay1"] = relay1Status;
  doc["relay2"] = relay2Status;
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void handleRelay(AsyncWebServerRequest *request) {
  if (request->hasParam("relay") && request->hasParam("state")) {
    int relay = request->getParam("relay")->value().toInt();
    int state = request->getParam("state")->value().toInt();
    
    if (relay == 1) {
      digitalWrite(relayPin1, state ? HIGH : LOW);
      relay1Status = (state ? true : false);
    } else if (relay == 2) {
      digitalWrite(relayPin2, state ? HIGH : LOW);
      relay2Status = (state ? true : false);
    }
  }
  handleStatus(request);
}

void processFrame(const char *frame) {
  if (frame[0] == 'W' || frame[0] == 'w') {
    String weightStr = "";
    for (int i = 2; i <= 7; i++) {
      if (frame[i] != ' ') {
        weightStr += frame[i];
      }
    }
    weightStr += ".";
    weightStr += frame[9];
    
    currentWeight = weightStr.toFloat();
    Serial.print("Weight: ");
    Serial.print(currentWeight);
    Serial.println(" kg");

    relay1Status = (currentWeight > 100.0);
    relay2Status = (currentWeight > 200.0);
    
    digitalWrite(relayPin1, relay1Status ? HIGH : LOW);
    digitalWrite(relayPin2, relay2Status ? HIGH : LOW);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  RS232Serial.begin(9600, SERIAL_8N1, 21, 22);

  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  digitalWrite(relayPin1, LOW);
  digitalWrite(relayPin2, LOW);

  // Initialize LittleFS
  Serial.println("Mounting LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed!");
    return;
  }
  Serial.println("LittleFS mounted");

  // Setup WiFi AP
  Serial.println("Starting WiFi AP...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("IP: ");
  Serial.println(IP);
  Serial.print("SSID: ");
  Serial.println(ssid);

  // Serve static files
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  
  // API endpoints
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/relay", HTTP_GET, handleRelay);
  
  server.begin();
  Serial.println("Server started");
}

void loop() {
  while (RS232Serial.available()) {
    char c = RS232Serial.read();

    if ((c == 'W' || c == 'w' || c == ' ') && bufferIndex == 0) {
      bufferIndex = 1;
      rxBuffer[0] = c;
    }
    else if (bufferIndex > 0) {
      if (c == '\r') {
        if (bufferIndex == 11) {
          rxBuffer[bufferIndex] = '\0';
          processFrame(rxBuffer);
          bufferIndex = 0;
          memset(rxBuffer, 0, BUFFER_SIZE);
        } else {
          bufferIndex = 0;
          memset(rxBuffer, 0, BUFFER_SIZE);
        }
      }
      else if (c != '\n') {
        if (bufferIndex < BUFFER_SIZE - 1) {
          rxBuffer[bufferIndex++] = c;
        }
      }
    }
  }
  delay(1);
}
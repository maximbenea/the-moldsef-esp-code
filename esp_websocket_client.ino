
#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// #include "certs.h"

#ifndef STASSID
#define STASSID "your_SSID"
#define STAPSK "your_password"
#endif

#define LED_MINTY_PIN D1
#define LED_FRUTY_PIN D2

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

// WebSocket event handler
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  // Declare JSON document outside switch to avoid jump-to-case-label error
  DynamicJsonDocument doc(1024);
  
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WebSocket] Disconnected");
      Serial.println("Attempting to reconnect...");
      break;
    case WStype_CONNECTED:
      Serial.println("[WebSocket] Connected to server successfully!");
      Serial.println("Ready to receive scent updates");
      break;
    case WStype_TEXT:
      Serial.printf("[WebSocket] Received: %s\n", payload);
      
      deserializeJson(doc, payload);
      
      if (doc.containsKey("message")) {
        String scent = doc["message"];
        Serial.println("Detected scent: " + scent);
        
        // Control LEDs based on scent - for future development
        digitalWrite(LED_FRUTY_PIN, LOW);
        digitalWrite(LED_MINTY_PIN, LOW);
        if (scent == "fruity") {
          digitalWrite(LED_FRUTY_PIN, HIGH);
          Serial.println("LED: Fruity scent detected - GREEN LED ON");
        } else if (scent == "minty") {
          digitalWrite(LED_MINTY_PIN, HIGH);
          Serial.println("LED: Minty scent detected - BLUE LED ON");
        }
      }
      break;
    case WStype_ERROR:
      Serial.println("[WebSocket] Error occurred");
      Serial.printf("Error details: %s\n", payload);
      break;
    case WStype_PING:
      Serial.println("[WebSocket] Ping received");
      break;
    case WStype_PONG:
      Serial.println("[WebSocket] Pong received");
      break;
  }
}

void setup() {

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  pinMode(LED_MINTY_PIN, OUTPUT);
  pinMode(LED_FRUTY_PIN, OUTPUT);
  
  Serial.println();
  Serial.println();
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(STASSID, STAPSK);
  Serial.println("setup() done connecting to ssid '" STASSID "'");
  
  // Initialize WebSocket
  webSocket.beginSSL("fastapi-backend-i18f.onrender.com", 443, "/ws/esp8266");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(30000, 3000, 2); // Send ping every 30s, expect pong within 3s, disconnect after 2 missed 
}

void loop() {
  // Check WiFi connection
  if (WiFiMulti.run() == WL_CONNECTED) {
    webSocket.loop();
    
    // Send periodic heartbeat to keep connection alive
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 60000) {
      webSocket.sendTXT("ping");
      lastHeartbeat = millis();
      Serial.println("[WebSocket] Heartbeat sent");
    }
    
    // Debug connection status
    static unsigned long lastStatusCheck = 0;
    if (millis() - lastStatusCheck > 10000) {
      Serial.println("[WebSocket] Connection active");
      lastStatusCheck = millis();
    }
  } else {
    Serial.println("WiFi not connected, retrying...");
    delay(1000);
  }
}

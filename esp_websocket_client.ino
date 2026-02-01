

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Stepper.h>

// #include "certs.h"

#ifndef STASSID
#define STASSID "your_SSID"
#define STAPSK "your_password"
#endif

// Definitions for stepper

const int STEPS_PER_REV = 2048;

#define IN1 D1
#define IN2 D2
#define IN3 D3
#define IN4 D4

Stepper myStepper(STEPS_PER_REV, IN1, IN3, IN2, IN4);

// Limit switch 
#define HOME_SWITCH_PIN D5  // Connect between D5 and GND

// Variables
long currentPosition = 0;
long targetPosition = 0;    
bool isMoving = false;      
int motorSpeed = 10; // RPM change later for speed variation

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

/*Scent list
Woody Fruity Chemical Minty Sweet Popcorn Lemon Pungent Decayed
*/

void runHome() {
  Serial.println("[Stepper] Starting setup ...");
  
  myStepper.setSpeed(motorSpeed);

  if (digitalRead(HOME_SWITCH_PIN) == LOW) {
    Serial.println("[Stepper] Already at home!");
    currentPosition = 0;
    targetPosition = 0;
    return;
  }

  // If your motor goes down, change 1 to -1
  while (digitalRead(HOME_SWITCH_PIN) == HIGH) {
    myStepper.step(1); 
    delay(2); 
    yield();
  }

  myStepper.step(-50);

  currentPosition = 0;
  targetPosition = 0;
  isMoving = false;

  Serial.println("[Stepper] Setup Complete. Position set to 0");
}


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
      break;
    case WStype_TEXT:

      if (isMoving) {
        Serial.println("[Debug] Motor is BUSY. Ignoring message.");
        return;
      }

      deserializeJson(doc, payload);
      
      if (doc.containsKey("message")) {
        String scent = doc["message"];
        Serial.println("[Result] Detected scent: " + scent);
        
        long newTarget = currentPosition; 

        // note modify to more effective mapping 
        if (scent == "minty") {
          newTarget = 1000;
        } else if (scent == "fruity") {
          newTarget = 2000;
        } else if (scent == "woody") {
          newTarget = 3000;
        } else if (scent == "chemical") {
          newTarget = 4000;
        } else if (scent == "sweet") {
          newTarget = 5000;
        } else if (scent == "popcorn") {
          newTarget = 6000;
        } else if (scent == "lemon") {
          newTarget = 7000;
        } else if (scent == "pungent") {
          newTarget = 8000;
        } else if (scent == "decayed") {
          newTarget = 9000;
        }

        if (newTarget != currentPosition) {
          targetPosition = newTarget;
          isMoving = true;
          Serial.print("[Debug] New Target Set: ");
          Serial.println(targetPosition);
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

  // Setup Pins
  pinMode(HOME_SWITCH_PIN, INPUT_PULLUP);

  // Setup WiFi
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(STASSID, STAPSK);

  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Initialize WebSocket
  webSocket.beginSSL("fastapi-backend-i18f.onrender.com", 443, "/ws/esp8266");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  
  // Pinging
  webSocket.enableHeartbeat(60000, 3000, 2); // Send ping every 60s, expect pong within 3s, disconnect after 2 missed 

  /* Go to home position onece, on startup
  / This happens once after WiFi connects, before listening to WebSockets */
  runHome();
}


void loop() {
  webSocket.loop();

  if (isMoving) {
    if (currentPosition < targetPosition) {
      myStepper.step(1); // Move forward
      currentPosition++;
    } 
    else if (currentPosition > targetPosition) {
      myStepper.step(-1); // Move backward
      currentPosition--;
    } 
    else {
      isMoving = false;
      Serial.println("[Debug] Target Reached.");
    }
    
    // Tiny delay, adjust this to make it faster/slower
    delay(2); 
  }
}

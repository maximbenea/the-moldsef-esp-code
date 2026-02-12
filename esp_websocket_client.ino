#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Stepper.h>
#include <Servo.h>

// WiFi Credentials
#ifndef STASSID
#define STASSID "Pixel 7a"
#define STAPSK "12345678"
#endif

// 28BYJ-48 is 2048 steps per rev. Gear ratio is 7:1.
// 2048 * 7 = 14336. 
#define MOTOR_STEPS_PER_REV 14336   
#define SLOT_COUNT 12

// Pin Definitions
#define IN1 D1
#define IN2 D2
#define IN3 D3
#define IN4 D4

#define HOME_SWITCH_PIN D5 
#define SERVO_PIN D8
#define SPRAY_PIN D7 

Stepper motor(2048, IN1, IN3, IN2, IN4); 

Servo servo;

// Global Variables
long currentStepPos = 0; 
long slotSteps[SLOT_COUNT];
bool isMoving = false; 

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

void powerDownMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void sprayServo() {
  servo.attach(SERVO_PIN);
  delay(100);

  servo.write(80);
  delay(1000); 
  

  // Turns it on
  digitalWrite(SPRAY_PIN, LOW);
  delay(500); 

  // Keep it on fot 5s
  digitalWrite(SPRAY_PIN, HIGH);
  delay(5000);

  // Turn it off with two low impulses 
  digitalWrite(SPRAY_PIN, LOW);
  delay(500);
  digitalWrite(SPRAY_PIN, LOW);
  delay(500);

  digitalWrite(SPRAY_PIN, HIGH);

  servo.write(0);
  delay(500);

  servo.detach();
}

void homeMotor() {
  Serial.println("[HOME] Starting calibration...");
  
  isMoving = true;

  servo.attach(SERVO_PIN);
  delay(100);

  // Mandatory so it does not conflict with anything
  servo.write(0);
  delay(500);
  servo.detach();

  motor.setSpeed(13); 

  while (digitalRead(HOME_SWITCH_PIN) == HIGH) {
    motor.step(-1); 
    delay(2); 
    yield();
  }
  motor.step(450); 
  
  currentStepPos = 0;
  Serial.println("[HOME] Calibration Complete.");
  
  powerDownMotor();
  isMoving = false; // Unlock
}

void safeMotorStep(long stepsToMove) {
  if (stepsToMove == 0) return;

  int direction = (stepsToMove > 0) ? 1 : -1;
  long stepsLeft = abs(stepsToMove);
  
  motor.setSpeed(13); 

  while(stepsLeft > 0) {
    int chunk = (stepsLeft > 50) ? 50 : stepsLeft;
    motor.step(chunk * direction);
    stepsLeft -= chunk;

    // Crucial for keeping conection alive
    yield(); 
  }
  
}

void moveToSlot(int slot) {
  // If already moving, ignore new commands
  if (isMoving) {
    return;
  }

  if (slot < 0 || slot >= SLOT_COUNT) return;
  
  // Lock the motor
  isMoving = true;

  long target = slotSteps[slot];
  long delta = target - currentStepPos;

  servo.attach(SERVO_PIN);
  delay(100);
  servo.write(0);
  delay(500);
  servo.detach(); 

  safeMotorStep(delta);
  currentStepPos = target; 

  sprayServo();
  powerDownMotor();

  isMoving = false;
}

void runDebugSequence() {
  homeMotor(); 
  delay(1000);

  for (int i = 0; i < SLOT_COUNT-1; i++) {
    moveToSlot(i);
    delay(1000);

  }

  // After finishing all slots 
  homeMotor();

}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WebSocket] Disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("[WebSocket] Connected!");
      break;
    case WStype_TEXT:
       if (isMoving) return;

      { 
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
           Serial.println(error.f_str());
          return;
        }
        
        if (doc.containsKey("message")) {
          const char* scent = doc["message"];
          Serial.println(scent);

          int targetSlot = -1;
          
          if (strcmp(scent, "lavender") == 0) targetSlot = 0;
          else if (strcmp(scent, "ocean") == 0) targetSlot = 1;
          else if (strcmp(scent, "lemon") == 0) targetSlot = 2;
          else if (strcmp(scent, "wood") == 0) targetSlot = 3;
          else if (strcmp(scent, "orange") == 0) targetSlot = 4;
          else if (strcmp(scent, "strawberry") == 0) targetSlot = 5;
          else if (strcmp(scent, "rose") == 0) targetSlot = 6;
          else if (strcmp(scent, "mint") == 0) targetSlot = 7;
          
          if (targetSlot != -1) 
            moveToSlot(targetSlot);
        }
      }
      break;
    case WStype_PING:
      break;
    case WStype_PONG:
    case WStype_ERROR:
      Serial.println("[WebSocket] Error occurred");
      Serial.printf("Error details: %s\n", payload);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Setup Pins
  pinMode(HOME_SWITCH_PIN, INPUT_PULLUP);
  
  // Setup Spray Pin
  pinMode(SPRAY_PIN, OUTPUT);
  digitalWrite(SPRAY_PIN, HIGH); // Ensure On on startup

  // Default state UP (0) per your request
  servo.attach(SERVO_PIN);
  servo.write(0); 
  servo.detach();

  // Precompute slot positions
  for (int i = 0; i < SLOT_COUNT; i++) {
    slotSteps[i] = (long)((float)MOTOR_STEPS_PER_REV * (float)i / (float)SLOT_COUNT);
  }

  //runDebugSequence();
  
  // Do an initial home to set positions
  homeMotor();

  // Setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFiMulti.addAP(STASSID, STAPSK);

  // Serial.print("Connecting to WiFi");
  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(200);
  }
  Serial.println("\nWiFi connected");

  // Initialize WebSocket
  webSocket.beginSSL("fastapi-backend-i18f.onrender.com", 443, "/ws/esp8266");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  //webSocket.enableHeartbeat(15000, 3000, 2);
}

void loop() {
  if (WiFiMulti.run() == WL_CONNECTED) {
    webSocket.loop();
  } else {
    // Serial.println("[WiFi] Connection lost, retrying...");
    delay(1000); // Don't spam retries too fast
  }

  // 2. Feed the Hardware Watchdog
  yield(); 
}
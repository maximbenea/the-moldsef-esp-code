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
#define STASSID "StarNet - Benea"
#define STAPSK "069657564"
#endif

// ================= HARDWARE SETTINGS =================
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
#define SPRAY_PIN D7  // New Pin for Spray Mechanism

// Stepper Object
Stepper motor(2048, IN1, IN3, IN2, IN4); 

// Servo Object
Servo servo;

// Global Variables
long currentStepPos = 0; 
long slotSteps[SLOT_COUNT];
bool isMoving = false; // Safety flag

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

// ================= FUNCTIONS =================


// NEW: Cuts power to stepper coils to prevent overheating/resetting
void powerDownMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// NEW FUNCTION: Spray Mechanism
void sprayServo() {
  Serial.println("[SPRAY] Activating D7...");

  delay(100); 
  servo.write(80);
  delay(1000); 
  
  digitalWrite(SPRAY_PIN, HIGH); // Turn ON
  delay(2000);                   // Wait 2 seconds
  digitalWrite(SPRAY_PIN, LOW);  // Turn OFF

  servo.write(0);
  delay(500);
  
  Serial.println("[SPRAY] Deactivated.");
}

// 1. Calibration Routine
void homeMotor() {
  Serial.println("[HOME] Starting calibration...");
  
  // Lock movement during homing
  isMoving = true;

  // Move Servo UP (0) to avoid hitting anything
  servo.write(0);
  delay(500);

  motor.setSpeed(13); 

  Serial.print("[HOME] Finding limit switch...");
  while (digitalRead(HOME_SWITCH_PIN) == HIGH) {
    motor.step(-1); 
    delay(2); 
    yield();
  }
  Serial.println(" HIT!");

  // Back off
  Serial.println("[HOME] Backing off...");
  motor.step(150); 
  
  currentStepPos = 0;
  Serial.println("[HOME] Calibration Complete.");
  
  // Leave Servo UP (Safe state)
  servo.write(0); 
  powerDownMotor();
  isMoving = false; // Unlock
}

// 2. Safe Movement Function
void safeMotorStep(long stepsToMove) {
  if (stepsToMove == 0) return;

  int direction = (stepsToMove > 0) ? 1 : -1;
  long stepsLeft = abs(stepsToMove);
  
  motor.setSpeed(12); 

  while(stepsLeft > 0) {
    int chunk = (stepsLeft > 50) ? 50 : stepsLeft;
    motor.step(chunk * direction);
    stepsLeft -= chunk;
    yield(); 
  }
  
}

// 3. Move to Specific Slot (Updated logic)
void moveToSlot(int slot) {
  // SAFETY CHECK: If already moving, ignore new commands
  if (isMoving) {
    Serial.println("[WARN] Motor is busy. Ignoring command.");
    return;
  }

  if (slot < 0 || slot >= SLOT_COUNT) return;
  
  // Lock the motor
  isMoving = true;

  long target = slotSteps[slot];
  long delta = target - currentStepPos;

  Serial.print("[MOVE] Slot ");
  Serial.print(slot);
  Serial.print(" | Steps: ");
  Serial.println(delta);

  // 1. Ensure Servo is UP (0) before moving platform
  servo.write(0);
  delay(500); 

  // 2. Move Stepper to target
  safeMotorStep(delta);
  currentStepPos = target; 

  // 3. Servo DOWN (80)
  // Wait for servo to fully go down

  // 4. ACTIVATE SPRAY (D7)
  powerDownMotor();
  sprayServo();
  powerDownMotor();

  // Unlock the motor
  isMoving = false;
  Serial.println("[MOVE] Done. Waiting for next command.");
}

// ================= DEBUG TEST FUNCTION =================
void runDebugSequence() {
  Serial.println("\n\n=== STARTING DEBUG SEQUENCE ===");
  
  // 1. Start by Homing
  homeMotor(); 
  delay(1000);

  // 2. Run through all slots
  for (int i = 0; i < SLOT_COUNT-1; i++) {
    Serial.print("--- Testing Slot: ");
    Serial.print(i);
    Serial.println(" ---");

    // Move to the slot (Includes Spray and Return to 0)
    moveToSlot(i);

    Serial.println("[TEST] Waiting 1 second before next slot...");
    delay(1000);
  }

  // 3. Finished all slots, go Home
  Serial.println("--- All slots visited. Returning Home. ---");
  homeMotor();

  Serial.println("=== DEBUG SEQUENCE FINISHED ===\n\n");
}

// ================= WEBSOCKET HANDLER =================
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  DynamicJsonDocument doc(1024);
  
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WebSocket] Disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("[WebSocket] Connected!");
      break;
    case WStype_TEXT:
      if (isMoving) {
        Serial.println("[BUSY] Ignoring WebSocket command.");
        return;
      }

      deserializeJson(doc, payload);
      
      if (doc.containsKey("message")) {
        String scent = doc["message"];
        Serial.println("[CMD] Scent: " + scent);
        
        int targetSlot = -1;

        if (scent == "minty") targetSlot = 0;
        else if (scent == "fruity") targetSlot = 1;
        else if (scent == "woody") targetSlot = 2;
        else if (scent == "chemical") targetSlot = 3;
        else if (scent == "sweet") targetSlot = 4;
        else if (scent == "popcorn") targetSlot = 5;
        else if (scent == "lemon") targetSlot = 6;
        else if (scent == "pungent") targetSlot = 7;
        else if (scent == "decayed") targetSlot = 8;
        
        if (targetSlot != -1) {
          moveToSlot(targetSlot);
        }
      }
      break;
    case WStype_PING:
    case WStype_PONG:
    case WStype_ERROR:
      Serial.println("[WebSocket] Error occurred");
      Serial.printf("Error details: %s\n", payload);
      break;
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(500);

  // Setup Pins
  pinMode(HOME_SWITCH_PIN, INPUT_PULLUP);
  
  // Setup Spray Pin
  pinMode(SPRAY_PIN, OUTPUT);
  digitalWrite(SPRAY_PIN, LOW); // Ensure OFF at startup

  servo.attach(SERVO_PIN);
  servo.write(0); // Default state UP (0) per your request

  // Precompute slot positions
  for (int i = 0; i < SLOT_COUNT; i++) {
    slotSteps[i] = (long)((float)MOTOR_STEPS_PER_REV * (float)i / (float)SLOT_COUNT);
  }

  // ============================================
  // RUN DEBUG SEQUENCE ON BOOT (Uncomment to test)
  // ============================================
  // runDebugSequence();
  
  // Do an initial home to set positions
  homeMotor();

  // Setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFiMulti.addAP(STASSID, STAPSK);

  Serial.print("Connecting to WiFi");
  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Initialize WebSocket
  webSocket.beginSSL("fastapi-backend-i18f.onrender.com", 443, "/ws/esp8266");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2);
}

void loop() {
  if (WiFiMulti.run() == WL_CONNECTED) {
    webSocket.loop();
  } else {
    Serial.println("[WiFi] Connection lost, retrying...");
    delay(1000); // Don't spam retries too fast
  }

  // 2. Feed the Hardware Watchdog
  yield(); 
}
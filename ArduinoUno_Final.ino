#include <AFMotor.h>
#include <Servo.h>

// --- MOTOR CONFIGURATION ---
AF_DCMotor motor1(1); AF_DCMotor motor2(2);
AF_DCMotor motor3(3); AF_DCMotor motor4(4);

// --- SERVO CONFIGURATION ---
Servo panMotor;
Servo tiltMotor;
const int PAN_PIN = 9;   
const int TILT_PIN = 10; 

// Updated Initial Positions to match your ESP32-CAM center commands (75, 135)
int currP = 75; 
int targetP = 75;
int currT = 135;  
int targetT = 135;

unsigned long lastTick = 0;
unsigned long lastSignal = 0;
const int stepSpeed = 15; 
const unsigned long failSafeTimeout = 10000;

void setup() {
  Serial.begin(9600);
  setSpeedAll(255);
  stopAll();
  
  // 1. Set targets to center immediately
  targetP = 75;
  targetT = 135;
  currP = 75;
  currT = 135;

  // 2. Briefly attach to "lock" them in center, then detach to prevent twitching
  panMotor.attach(PAN_PIN);
  tiltMotor.attach(TILT_PIN);
  panMotor.write(currP);
  tiltMotor.write(currT);
  
  delay(1000); // Give servos time to reach center
  panMotor.detach();
  tiltMotor.detach();

  // 3. Flush the Serial buffer to clear any ESP32 bootloader noise
  while(Serial.available() > 0) { Serial.read(); }
  
  lastSignal = millis();
}

void loop() {
  // 1. Command Processor - Only process if 2 seconds have passed since boot
  // This ignores the high-speed "garbage" data sent by ESP32 during its startup
  if (millis() > 2000 && Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    if (data.length() > 0) {
      lastSignal = millis();
      char c = data[0];
      int v = data.substring(1).toInt();
      
      if (c == 'P') targetP = constrain(v, 35, 115);
      else if (c == 'T') targetT = constrain(v, 90, 180);
      else handleDrive(c);
    }
  }

  // 2. Fail-Safe Routine
  if (millis() - lastSignal > failSafeTimeout) {
    targetP = 75; // Return to your defined center
    targetT = 135; 
    stopAll();     
  }

  // 3. Movement Engine
  if (millis() - lastTick > stepSpeed) {
    updateS(panMotor, currP, targetP, PAN_PIN);
    updateS(tiltMotor, currT, targetT, TILT_PIN);
    lastTick = millis();
  }
}

void updateS(Servo &s, int &c, int t, int p) {
  if (c != t) {
    if (!s.attached()) s.attach(p);
    if (t > c) c++; else c--;
    s.write(c);
  } else {
    // Keep attached for a short moment after reaching target to ensure stability
    // but ultimately detach to save power and stop "humming"
    if (s.attached()) {
       static unsigned long lastMove = 0;
       if(millis() - lastMove > 500) s.detach();
    }
  }
}

void handleDrive(char c) {
  switch(c) {
    case 'F': motor2.run(FORWARD); motor4.run(FORWARD); motor1.run(BACKWARD); motor3.run(BACKWARD); break;
    case 'B': motor2.run(BACKWARD); motor4.run(BACKWARD); motor1.run(FORWARD); motor3.run(FORWARD); break;
    case 'L': motor2.run(FORWARD); motor3.run(BACKWARD); motor1.run(FORWARD); motor4.run(BACKWARD); break;
    case 'R': motor1.run(BACKWARD); motor4.run(FORWARD); motor2.run(BACKWARD); motor3.run(FORWARD); break;
    case 'S': stopAll(); break;
  }
}

void stopAll() {
  motor1.run(RELEASE); motor2.run(RELEASE); motor3.run(RELEASE); motor4.run(RELEASE);
}

void setSpeedAll(int s) {
  motor1.setSpeed(s); motor2.setSpeed(s); motor3.setSpeed(s); motor4.setSpeed(s);
}
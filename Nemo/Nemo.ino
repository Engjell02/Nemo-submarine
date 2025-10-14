#include <Servo.h>

// Pins
const int joyVertPin = A0;   
const int joyHorizPin = A1;  
const int leftSwPin = 31;    
const int rightSwPin = 32;   
const int waterPin = 30;     
const int irPin = A2;        
const int servoPin = 5;      
// TB6612 pins
const int STBY = 7;
const int AIN1 = 8;
const int AIN2 = 9;
const int PWMA = 2;
const int BIN1 = 10;
const int BIN2 = 11;
const int PWMB = 3;

Servo syringeServo;


const int releaseAngle = 50;   // min
const int fillAngle = 130;    //  max
const int neutralAngle = 90;  // middle position

// Servo movement settings
int currentServoPosition = neutralAngle;
const int servoSpeed = 1;     // degrees per update
const int servoUpdateDelay = 20; // ms between updates

// Water sensor calibration
bool waterSensorCalibrated = false;
bool waterSensorNormalState = HIGH;

// Timing
unsigned long lastPrint = 0;
const unsigned long printInterval = 500;
unsigned long lastServoUpdate = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("=== Submarine Controller Starting ===");

  
  // Configure joystick buttons with pullup
  pinMode(leftSwPin, INPUT_PULLUP);
  pinMode(rightSwPin, INPUT_PULLUP);
  pinMode(waterPin, INPUT_PULLUP);
  
  // TB6612 setup
  pinMode(STBY, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  digitalWrite(STBY, HIGH);
  stopMotors();
  
  // Initialize servo
  syringeServo.attach(servoPin);
  syringeServo.write(neutralAngle);
  currentServoPosition = neutralAngle;
  
  // Calibrate water sensor
  calibrateWaterSensor();
  
  Serial.println("Ready!\n");
}

void loop() {
  unsigned long currentTime = millis();
  
  //  MOTOR CONTROL
  int rawV = analogRead(joyVertPin);
  int rawH = analogRead(joyHorizPin);
  
  int speedA = map(rawV, 0, 1023, 255, -255);
  int speedB = map(rawH, 0, 1023, -255, 255);
  
  // Apply deadzone (joy drift)
  if (abs(speedA) < 15) speedA = 0;
  if (abs(speedB) < 15) speedB = 0;
  
  setMotorA(speedA);
  setMotorB(speedB);
  
  // === SERVO CONTROL (HOLD TO MOVE) ===
  bool leftPressed = (digitalRead(leftSwPin) == LOW);
  bool rightPressed = (digitalRead(rightSwPin) == LOW);
  
  // === IR SAFETY CHECK ===
  /*bool isObstacleClose() {
    int irRaw = analogRead(irPin);

    // You will need to test and adjust this threshold value.
    // Let's assume raw value < 200 = ~10cm or less (you will fine-tune this).
    const int irThreshold = 200;

    return irRaw < irThreshold;
  }*/

  // Update servo position based on button state
  if (currentTime - lastServoUpdate >= servoUpdateDelay) {
    lastServoUpdate = currentTime;
    
    if (leftPressed && !rightPressed) {
      //if(!isObstacleClose()) {

        // Move towards fill angle (sink)
      if (currentServoPosition < fillAngle) {
      currentServoPosition = min(currentServoPosition + servoSpeed, fillAngle);
      syringeServo.write(currentServoPosition);
      Serial.println("SINKING: Servo → " + String(currentServoPosition));
      }
      /*}else {
        Serial.println("⚠ OBSTACLE DETECTED - SINK BLOCKED");
      } */
    }
    else if (rightPressed && !leftPressed) {
      // Move towards release angle (rise)
      if (currentServoPosition > releaseAngle) {
        currentServoPosition = max(currentServoPosition - servoSpeed, releaseAngle);
        syringeServo.write(currentServoPosition);
        Serial.println("RISING: Servo → " + String(currentServoPosition));
      }
    }else{ // Stop servo movement
      currentServoPosition = 90;
      syringeServo.write(currentServoPosition);
    }
    
  }
  // === WATER SENSOR CHECK ===
  bool waterDetected = (digitalRead(waterPin) != waterSensorNormalState);
  if (waterDetected && waterSensorCalibrated) {
    emergencySurface();
  }
  
  // === PERIODIC STATUS PRINT ===
  if (currentTime - lastPrint >= printInterval) {
    lastPrint = currentTime;
    
    int irRaw = analogRead(irPin);
    
    Serial.print("Motors[A:");
    Serial.print(speedA);
    Serial.print(" B:");
    Serial.print(speedB);
    Serial.print("] Servo:");
    Serial.print(currentServoPosition);
    Serial.print("° Buttons[L:");
    Serial.print(leftPressed ? "ON" : "off");
    Serial.print(" R:");
    Serial.print(rightPressed ? "ON" : "off");
    Serial.print("] IR:");
    Serial.print(irRaw);
    Serial.print(" Water:");
    Serial.println(waterDetected ? "DETECTED!" : "ok");
  }
  
  delay(10);
}

// === WATER SENSOR CALIBRATION ===
void calibrateWaterSensor() {
  Serial.println("\n*** Water Sensor Calibration ***");
  Serial.println("Ensure NO water is touching the sensor!");
  Serial.print("Calibrating");
  
  int highCount = 0;
  int lowCount = 0;
  
  for (int i = 0; i < 30; i++) {
    if (digitalRead(waterPin) == HIGH) highCount++;
    else lowCount++;
    Serial.print(".");
    delay(100);
  }
  
  waterSensorNormalState = (highCount > lowCount) ? HIGH : LOW;
  waterSensorCalibrated = true;
  
  Serial.println("\nCalibration done! Normal state: " + String(waterSensorNormalState == HIGH ? "HIGH" : "LOW"));
  delay(1000);
}

// === MOTOR CONTROL FUNCTIONS ===
void setMotorA(int speed) {
  if (speed > 0) {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
    analogWrite(PWMA, constrain(speed, 0, 255));
  } else if (speed < 0) {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
    analogWrite(PWMA, constrain(-speed, 0, 255));
  } else {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
    analogWrite(PWMA, 0);
  }
}

void setMotorB(int speed) {
  if (speed > 0) {
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
    analogWrite(PWMB, constrain(speed, 0, 255));
  } else if (speed < 0) {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
    analogWrite(PWMB, constrain(-speed, 0, 255));
  } else {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);
    analogWrite(PWMB, 0);
  }
}

void stopMotors() {
  setMotorA(0);
  setMotorB(0);
}

// === EMERGENCY SURFACE ===
void emergencySurface() {
  Serial.println("\n!!! WATER DETECTED - EMERGENCY SURFACE !!!");
  currentServoPosition = releaseAngle;
  syringeServo.write(releaseAngle);
  stopMotors();
  
  // Flash warning
  for (int i = 0; i < 10; i++) {
    Serial.println("!!! EMERGENCY !!!");
    delay(200);
  }
}
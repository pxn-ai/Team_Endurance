/*#include <Arduino.h>
#include <string.h>
#include <stdlib.h>

// --- PIN DEFINITIONS ---
#define ENA_PIN 4  
#define IN1_PIN 22  
#define IN2_PIN 23 
#define ENB_PIN 5  
#define IN3_PIN 24  
#define IN4_PIN 25  
#define LEFT_ENC_A 18
#define LEFT_ENC_B 19
#define RIGHT_ENC_A 20
#define RIGHT_ENC_B 21

// --- GLOBALS ---
volatile long leftEncoderTicks = 0;
volatile long rightEncoderTicks = 0;
int baseSpeed = 110;
int minSpeed = 100;   
int maxSpeed = 150;  

// --- ENCODER ISRs ---
void leftEncoderISR() { if (digitalRead(LEFT_ENC_B) == HIGH) leftEncoderTicks++; else leftEncoderTicks--; }
void rightEncoderISR() { if (digitalRead(RIGHT_ENC_B) == HIGH) rightEncoderTicks--; else rightEncoderTicks++; }

// --- MOTOR DRIVER ---
class MotorDriver {
public:
    void init() {
        pinMode(ENA_PIN, OUTPUT); pinMode(IN1_PIN, OUTPUT); pinMode(IN2_PIN, OUTPUT);
        pinMode(ENB_PIN, OUTPUT); pinMode(IN3_PIN, OUTPUT); pinMode(IN4_PIN, OUTPUT);
        stop();
    }
    void drive(int leftSpeed, int rightSpeed) {
        leftSpeed = constrain(leftSpeed, -255, 255);
        rightSpeed = constrain(rightSpeed, -255, 255);
        digitalWrite(IN1_PIN, leftSpeed >= 0 ? HIGH : LOW);
        digitalWrite(IN2_PIN, leftSpeed >= 0 ? LOW : HIGH);
        analogWrite(ENA_PIN, abs(leftSpeed));
        digitalWrite(IN3_PIN, rightSpeed >= 0 ? HIGH : LOW);
        digitalWrite(IN4_PIN, rightSpeed >= 0 ? LOW : HIGH);
        analogWrite(ENB_PIN, abs(rightSpeed));
    }
    void stop() { drive(0, 0); }
};

// --- SENSOR ---
class RaykhaS8 {
private:
    bool detectHigh; 
public:
    RaykhaS8(bool d) : detectHigh(d) {}
    void init() { DDRC = 0x00; PORTC = 0xFF; }
    uint8_t readRaw() {
        uint8_t raw = PINC;
        uint8_t data = 0;
        for (int i = 0; i < 8; i++) if (raw & (1 << i)) data |= (1 << (7 - i));
        return detectHigh ? data : ~data;
    }
    float getError() {
        uint8_t raw = readRaw();
        if (raw == 0) return 999.0f; 
        int active = 0; float total = 0;
        float weights[8] = {-3.5, -2.5, -1.5, -0.5, 0.5, 1.5, 2.5, 3.5};
        for (int i = 0; i < 8; i++) if (raw & (1u << i)) { total += weights[i]; active++; }
        if (active >= 6) return 888.0f; 
        return total / active;
    }
};

struct MotorSpeeds { int left; int right; };

// --- PID ---
class RobotPID {
public:
    RobotPID() : Kp(15.0), Ki(0.0), Kd(5.0), prev(0), integral(0) {}
    void setTunings(float p, float i, float d) { Kp = p; Ki = i; Kd = d; }
    void reset() { integral = 0; prev = 0; }
    MotorSpeeds calculate(float error, int base, int minS, int maxS) {
        float P = Kp * error;
        integral += error;
        float D = Kd * (error - prev);
        prev = error;
        int corr = (int)(P + (Ki * integral) + D);
        return {constrain(base + corr, minS, maxS), constrain(base - corr, minS, maxS)};
    }
private:
    float Kp, Ki, Kd, prev, integral;
};

RaykhaS8 trackSensor(false);
RobotPID myPID;
MotorDriver motor;
float lastValidError = 0;

void setup() {
    Serial.begin(115200);
    motor.init();
    trackSensor.init();
    attachInterrupt(digitalPinToInterrupt(LEFT_ENC_A), leftEncoderISR, RISING);
    attachInterrupt(digitalPinToInterrupt(RIGHT_ENC_A), rightEncoderISR, RISING);
    delay(2000);
}

void loop() {
    float error = trackSensor.getError();

    // --- JUNCTION LOGIC ---
    if (error == 888.0f && baseSpeed > 0) {
        // 1. Peek forward to check if the line continues
        motor.drive(baseSpeed, baseSpeed);
        delay(150); 
        
        uint8_t checkLine = trackSensor.readRaw();
        
        if (checkLine == 0) { 
            // CASE A: NO MORE LINE AHEAD -> STOP AT T-JUNCTION END
            motor.stop();
            baseSpeed = 0; 
            Serial.println("ACK:STOPPED_AT_FINAL_T_JUNCTION");
        } else {
            // CASE B: LINE CONTINUES -> TURN LEFT
            Serial.println("ACK:TURNING_LEFT_AT_JUNCTION");
            motor.drive(-140, 140); // Hard Pivot Left
            delay(300); // Clear the horizontal junction
            while (!(trackSensor.readRaw() & 0b00011110)); // Spin until center hits
            myPID.reset();
        }
    }
    
    // --- LOST LINE (Dead End or 90 Turn) ---
    else if (error >= 999.0f && baseSpeed > 0) {
        if (lastValidError > 0) motor.drive(130, -130);
        else motor.drive(-130, 130);
        myPID.reset();
    }

    // --- NORMAL DRIVING ---
    else if (baseSpeed > 0) {
        lastValidError = error;
        MotorSpeeds s = myPID.calculate(error, baseSpeed, minSpeed, maxSpeed);
        motor.drive(s.left, s.right);
    } 
    else {
        motor.stop();
    }
    delay(10);
}*/

#include <Arduino.h>
#include <string.h>
#include <stdlib.h>

// --- PIN DEFINITIONS ---
#define ENA_PIN 4  
#define IN1_PIN 22  
#define IN2_PIN 23 
#define ENB_PIN 5  
#define IN3_PIN 24  
#define IN4_PIN 25  
#define LEFT_ENC_A 18
#define LEFT_ENC_B 19
#define RIGHT_ENC_A 20
#define RIGHT_ENC_B 21

// --- GLOBALS ---
volatile long leftEncoderTicks = 0;
volatile long rightEncoderTicks = 0;
int baseSpeed = 130;
int minSpeed = 100;   
int maxSpeed = 180;  
bool robotActive = true; // Added to handle the stop state

// --- ENCODER ISRs ---
void leftEncoderISR() { if (digitalRead(LEFT_ENC_B) == HIGH) leftEncoderTicks++; else leftEncoderTicks--; }
void rightEncoderISR() { if (digitalRead(RIGHT_ENC_B) == HIGH) rightEncoderTicks--; else rightEncoderTicks++; }

// --- MOTOR DRIVER ---
class MotorDriver {
public:
    void init() {
        pinMode(ENA_PIN, OUTPUT); pinMode(IN1_PIN, OUTPUT); pinMode(IN2_PIN, OUTPUT);
        pinMode(ENB_PIN, OUTPUT); pinMode(IN3_PIN, OUTPUT); pinMode(IN4_PIN, OUTPUT);
        stop();
    }
    void drive(int leftSpeed, int rightSpeed) {
        leftSpeed = constrain(leftSpeed, -255, 255);
        rightSpeed = constrain(rightSpeed, -255, 255);
        digitalWrite(IN1_PIN, leftSpeed >= 0 ? HIGH : LOW);
        digitalWrite(IN2_PIN, leftSpeed >= 0 ? LOW : HIGH);
        analogWrite(ENA_PIN, abs(leftSpeed));
        digitalWrite(IN3_PIN, rightSpeed >= 0 ? HIGH : LOW);
        digitalWrite(IN4_PIN, rightSpeed >= 0 ? LOW : HIGH);
        analogWrite(ENB_PIN, abs(rightSpeed));
    }
    void stop() { drive(0, 0); }
};

// --- SENSOR ---
class RaykhaS8 {
private:
    bool detectHigh; 
public:
    RaykhaS8(bool d) : detectHigh(d) {}
    void init() { DDRC = 0x00; PORTC = 0xFF; }
    uint8_t readRaw() {
        uint8_t raw = PINC;
        uint8_t data = 0;
        for (int i = 0; i < 8; i++) if (raw & (1 << i)) data |= (1 << (7 - i));
        return detectHigh ? data : ~data;
    }
    float getError() {
        uint8_t raw = readRaw();
        if (raw == 0) return 999.0f; 
        int active = 0; float total = 0;
        float weights[8] = {-3.5, -2.5, -1.5, -0.5, 0.5, 1.5, 2.5, 3.5};
        for (int i = 0; i < 8; i++) if (raw & (1u << i)) { total += weights[i]; active++; }
        
        // Junction detection threshold
        if (active >= 6) return 888.0f; 
        
        return total / active;
    }
};

struct MotorSpeeds { int left; int right; };

// --- PID ---
class RobotPID {
public:
    RobotPID() : Kp(15.0), Ki(0.0), Kd(5.0), prev(0), integral(0) {}
    void setTunings(float p, float i, float d) { Kp = p; Ki = i; Kd = d; }
    void reset() { integral = 0; prev = 0; }
    MotorSpeeds calculate(float error, int base, int minS, int maxS) {
        float P = Kp * error;
        integral += error;
        float D = Kd * (error - prev);
        prev = error;
        int corr = (int)(P + (Ki * integral) + D);
        return {constrain(base + corr, minS, maxS), constrain(base - corr, minS, maxS)};
    }
private:
    float Kp, Ki, Kd, prev, integral;
};

RaykhaS8 trackSensor(false);
RobotPID myPID;
MotorDriver motor;
float lastValidError = 0;

void setup() {
    Serial.begin(115200);
    motor.init();
    trackSensor.init();
    attachInterrupt(digitalPinToInterrupt(LEFT_ENC_A), leftEncoderISR, RISING);
    attachInterrupt(digitalPinToInterrupt(RIGHT_ENC_A), rightEncoderISR, RISING);
    
    Serial.println("SYSTEM_READY: Following straight lines, stopping at junctions.");
    delay(2000);
}

void loop() {
    // If the robot has been told to stop, keep motors off
    if (!robotActive) {
        motor.stop();
        return;
    }

    float error = trackSensor.getError();

    // --- JUNCTION LOGIC ---
    if (error == 888.0f) {
        // Stop the robot immediately
        motor.stop();
        robotActive = false; // Disable movement
        
        Serial.println("EVENT: JUNCTION_DETECTED");
        Serial.println("ACTION: ROBOT_STOPPED");
    } 
    
    // --- LOST LINE (Dead End or Sharp 90) ---
    else if (error >= 999.0f) {
        // This is usually a sharp turn or end of line.
        // We still use your previous pivot logic to find the line again.
        if (lastValidError > 0) motor.drive(130, -130);
        else motor.drive(-130, 130);
        myPID.reset();
    }

    // --- NORMAL STRAIGHT LINE FOLLOWING ---
    else {
        lastValidError = error;
        MotorSpeeds s = myPID.calculate(error, baseSpeed, minSpeed, maxSpeed);
        motor.drive(s.left, s.right);
    }

    delay(10);
}
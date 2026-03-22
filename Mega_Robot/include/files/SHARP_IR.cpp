#include <Arduino.h>

// ─── Pin Definitions ───────────────────────────────────────────────
const int frontIR = A0;
const int rightIR = A1;

// Motor Pins (Arduino Mega PWM pins)
const int enA = 4;   // Left motor speed
const int in1 = 22;
const int in2 = 23;
const int enB = 5;   // Right motor speed
const int in3 = 24;
const int in4 = 25;

// ─── PID Variables ─────────────────────────────────────────────────
double setpoint   = 8.0;   // Target distance from right wall (cm)
double Kp         = 2.5;
double Ki         = 0.01;
double Kd         = 1.2;
double error, lastError = 0, integral = 0, derivative, output;

// ─── Robot Settings ────────────────────────────────────────────────
const int baseSpeed          = 120;  // Base PWM speed (0–255)
const int obstacleThreshold  = 8;    // Front obstacle distance (cm)
const int turnDuration        = 400;  // ms to rotate during obstacle avoidance

// ─── Forward Declarations ──────────────────────────────────────────
float getDistance(int pin);
void  drive(int left, int right);
void  turnRight();
void  stopMotors();

// ───────────────────────────────────────────────────────────────────
void setup() {
  pinMode(enA, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(enB, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);

  Serial.begin(115200);
  Serial.println("=== Wall Follower Ready ===");
}

// ─── Distance from GP2Y0A51SK0F (2–15 cm) ─────────────────────────
// Averages 5 ADC readings to reduce noise, then applies empirical fit:
//   L (cm) = 2.34 / (Vo - 0.1)
float getDistance(int pin) {
  long sum = 0;
  for (int i = 0; i < 5; i++) {
    sum += analogRead(pin);
    delayMicroseconds(200); // small gap between reads
  }
  float volts = (sum / 5.0) * (5.0 / 1023.0);

  // Clamp volts to avoid divide-by-zero or negative results
  volts = constrain(volts, 0.15, 3.2);

  float dist = 2.34 / (volts - 0.1);

  // Clamp output to sensor's valid range
  return constrain(dist, 2.0, 15.0);
}

// ───────────────────────────────────────────────────────────────────
void loop() {
  float distFront = getDistance(frontIR);
  float distRight = getDistance(rightIR);

  // ── 1. Obstacle Avoidance ────────────────────────────────────────
  if (distFront < obstacleThreshold) {
    stopMotors();
    delay(100);        // brief pause before turning
    turnRight();
    stopMotors();
    delay(100);        // settle after turn

    // Reset PID state so there's no jump when wall following resumes
    integral  = 0;
    lastError = 0;

    Serial.println("OBSTACLE: Turned Right");
  }

  // ── 2. PID Wall Following ────────────────────────────────────────
  else {
    error      = setpoint - distRight;
    integral  += error;
    integral   = constrain(integral, -50, 50);   // anti-windup
    derivative = error - lastError;
    output     = (Kp * error) + (Ki * integral) + (Kd * derivative);
    lastError  = error;

    int speedLeft  = constrain(baseSpeed + (int)output, 0, 255);
    int speedRight = constrain(baseSpeed - (int)output, 0, 255);

    drive(speedLeft, speedRight);

    // ── Serial Debug (Serial Plotter friendly) ───────────────────
    Serial.print("Front:");    Serial.print(distFront);
    Serial.print("\tRight:");  Serial.print(distRight);
    Serial.print("\tError:");  Serial.print(error);
    Serial.print("\tOut:");    Serial.print(output);
    Serial.print("\tL:");      Serial.print(speedLeft);
    Serial.print("\tR:");      Serial.println(speedRight);
  }

  delay(50);  // ~20 Hz loop rate
}

// ─── Drive both motors forward at given PWM speeds ─────────────────
void drive(int left, int right) {
  // Left motor — forward
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  analogWrite(enA, left);

  // Right motor — forward
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
  analogWrite(enB, right);
}

// ─── Rotate in place clockwise (left fwd, right back) ──────────────
void turnRight() {
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);   // Left  → forward
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);  // Right → backward

  analogWrite(enA, 150);
  analogWrite(enB, 150);

  delay(turnDuration);      // rotate for fixed time, then caller stops
}

// ─── Stop both motors ──────────────────────────────────────────────
void stopMotors() {
  analogWrite(enA, 0);
  analogWrite(enB, 0);
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
}
#include <Arduino.h>
#include <ESP32Servo.h>

Servo baseServo;
Servo gripServo;

// ⚠️ Use safe GPIOs for ESP32-S3
int basePin = 5;

int gripPin = 7;

int cam_servo = 6; // used to rotate the camera (initial heading should be 90 degrees)

// Positions
int basePick = 60;

int gripOpen = 90;
int gripClose = 40;

int baseDrop = 120;
int armLift = 70;

void cam_left() {
    cam_servo.write(0);
}

void cam_right() {
    cam_servo.write(180);
}

void cam_front() {
    cam_servo.write(90);
}

void setup() {
  Serial.begin(115200);

  baseServo.attach(basePin);

  gripServo.attach(gripPin);

  // Initial position
  baseServo.write(90);

  gripServo.write(gripOpen);

  delay(2000);
}

void loop() {

  // Move to pick position
  baseServo.write(basePick);
  
  delay(1000);

  // Close gripper
  gripServo.write(gripClose);
  delay(1000);

  
  // Move to drop
  baseServo.write(baseDrop);
  delay(1000);

 

  // Release
  gripServo.write(gripOpen);
  delay(1000);

  // Return center
  baseServo.write(90);
  delay(1000);
}

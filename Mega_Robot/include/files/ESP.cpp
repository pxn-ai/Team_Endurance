#include <Arduino.h>
#include <ESP32Servo.h>

Servo baseServo;
Servo gripServo;
Servo camServo;

// ⚠️ Use safe GPIOs for ESP32-S3
int basePin = 4;
int gripPin = 5;
int camPin = 6;

// Positions
int basePick = 60;
int gripOpen = 90;
int gripClose = 40;
int baseDrop = 120;
int armLift = 70;

void setup()
{
  Serial.begin(115200); // Communicate with Arduino Mega's Serial2

  baseServo.attach(basePin);
  gripServo.attach(gripPin);
  camServo.attach(camPin);

  // Initial Rest positions
  baseServo.write(90);
  gripServo.write(gripOpen);
  camServo.write(90); // Front facing
}

void loop()
{
  if (Serial.available())
  {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "CAM_FRONT")
    {
      camServo.write(90);
    }
    else if (cmd == "CAM_RIGHT")
    {
      camServo.write(180);
    }
    else if (cmd == "CAM_LEFT")
    {
      camServo.write(0);
    }
    else if (cmd == "PICK")
    {
      baseServo.write(basePick);
      delay(1000);
      gripServo.write(gripClose);
      delay(1000);
      baseServo.write(armLift); // Lift so it doesn't drag
      delay(1000);
      Serial.println("ARM_DONE");
    }
    else if (cmd == "DROP")
    {
      baseServo.write(baseDrop);
      delay(1000);
      gripServo.write(gripOpen);
      delay(1000);
      baseServo.write(90); // Return center
      delay(1000);
      Serial.println("ARM_DONE");
    }
  }
}

#include <Arduino.h>

void setup() {
  // 1. Connection to your computer via USB
  Serial.begin(115200); 
  
  // 2. Connection to your Raspberry Pi
  Serial1.begin(115200); 

  // Short delay to let the serial ports stabilize
  delay(500);
  
  Serial.println("--- Serial Bridge Active ---");
  Serial.println("Listening for Pi commands on Serial1...");
}

void loop() {
  // Read from Pi -> Print to Computer
  if (Serial1.available()) {
    char incomingFromPi = Serial1.read();
    Serial.write(incomingFromPi); 
  }

  // Read from Computer -> Send to Pi
  if (Serial.available()) {
    char outgoingToPi = Serial.read();
    Serial1.write(outgoingToPi);
  }
}
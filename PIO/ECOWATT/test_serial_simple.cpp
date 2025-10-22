// Simple serial test for Wokwi
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("========================================");
  Serial.println("SIMPLE SERIAL TEST - ESP32");
  Serial.println("If you see this, serial is working!");
  Serial.println("========================================");
}

void loop() {
  static int counter = 0;
  Serial.print("Loop counter: ");
  Serial.println(counter++);
  delay(1000);
}

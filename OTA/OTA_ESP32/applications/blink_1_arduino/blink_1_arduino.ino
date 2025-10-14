#include <Arduino.h>
#include "ota_manager.h"

// WiFi credentials
const char* ssid = "HydroBK";
const char* password = "Hydrolink123";

// OTA server URL
const char* serverURL = "http://10.175.81.2:5001";

// Application version and info
const String APP_VERSION = "2.0.1";
const String APP_NAME = "LED Blink 0.2 Second";

// LED pin
const int LED_PIN = 2;

// OTA Manager instance
OTAManager ota(ssid, password, serverURL, APP_VERSION);

void setup() {
    Serial.begin(115200);
    Serial.println("=== " + APP_NAME + " ===");
    Serial.println("Application Version: " + APP_VERSION);
    
    // Initialize LED
    pinMode(LED_PIN, OUTPUT);
    
    // Initialize OTA
    ota.begin();
    
    // Initial OTA check
    ota.checkForUpdate();
    
    Serial.println("Setup complete. Starting main application...");
}

void loop() {
    // Main application: Blink LED every 1 second
    digitalWrite(LED_PIN, HIGH);
    Serial.println("[" + APP_NAME + "] LED ON");
    delay(200);
    
    digitalWrite(LED_PIN, LOW);
    Serial.println("[" + APP_NAME + "] LED OFF");
    delay(200);
    
    // Check for OTA updates every 30 seconds (60 loop cycles)
    static int otaCheckCounter = 0;
    otaCheckCounter++;
    if (otaCheckCounter >= 60) {
        otaCheckCounter = 0;
        ota.checkForUpdate();
    }
}
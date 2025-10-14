// Minimal ESP32 OTA Example v1.0.4

#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

#define WIFI_SSID     "HydroBK"
#define WIFI_PASSWORD "Hydrolink123"
#define SERVER_URL    "http://10.78.228.2:5001"
#define DEVICE_ID     "ESP32_EcoWatt_Smart"
#define FW_VERSION    "1.0.4"

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\nMinimal ESP32 OTA Example (v" FW_VERSION ")");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected. IP: " + WiFi.localIP().toString());
  Serial.println("Firmware Version: " FW_VERSION);
}

void loop() {
  static unsigned long lastOTA = 0;
  if (millis() - lastOTA > 3600000) { // Every 1 hour
    lastOTA = millis();
    Serial.println("Checking for OTA update...");

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(String(SERVER_URL) + "/ota/check");
      http.addHeader("Content-Type", "application/json");
      String payload = "{\"device_id\":\"" DEVICE_ID "\",\"current_version\":\"" FW_VERSION "\"}";
      int httpCode = http.POST(payload);

      if (httpCode == 200) {
        String response = http.getString();
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);
        if (!error && doc["update_available"] == true) {
          Serial.println("OTA update available! (simulate download/apply here)");
          // Insert your OTA download/apply logic here if needed
        } else {
          Serial.println("No OTA update available.");
        }
      } else {
        Serial.printf("OTA check failed, HTTP code: %d\n", httpCode);
      }
      http.end();
    } else {
      Serial.println("WiFi not connected, skipping OTA check.");
    }
  }
  delay(1000);
}

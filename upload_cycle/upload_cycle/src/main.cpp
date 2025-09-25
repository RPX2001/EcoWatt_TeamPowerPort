#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "HydroBK";
const char* password = "Hydrolink123";
const char* serverURL = "http://10.40.99.2:5001/process";  // Updated IP and port

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.print("Connected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Flask server URL: ");
  Serial.println(serverURL);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");
    
    // Create JSON payload
    DynamicJsonDocument doc(1024);
    doc["number"] = random(1, 100); // Send random number between 1-100
    doc["sensor_id"] = "ESP32_001";
    doc["timestamp"] = millis();
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    Serial.println("Sending data to Flask server...");
    Serial.print("Payload: ");
    Serial.println(jsonString);
    
    // Send POST request
    int httpResponseCode = http.POST(jsonString);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.print("Response: ");
      Serial.println(response);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      Serial.println("Check if Flask server is running at " + String(serverURL));
    }
    
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
  
  delay(10000); // Send data every 10 seconds
}

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "protocol_adapter.h"

#define API_KEY "your_api_key"
#define API_URL_Read "http://20.15.114.131:8080/api/inverter/read"
#define API_URL_write "http://20.15.114.131:8080/api/inverter/write"

void protocol_adapter::sendFrame(const String& dataframe) {
    HTTPClient http;
    http.begin(API_URL);
    http.addHeader("accept", "*/*");
    http.addHeader("Authorization", API_KEY);
    http.addHeader("Content-Type", "application/json");

    // Create JSON payload
    StaticJsonDocument<128> doc;
    doc["frame"] = dataframe;
    String payload;
    serializeJson(doc, payload);

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Response: " + response);
    } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
    }
    http.end();
}
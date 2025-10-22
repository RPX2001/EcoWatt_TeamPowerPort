/**
 * @file wokwi_mock.cpp
 * @brief Implementation of Wokwi simulation mocks
 * 
 * Provides simulated HTTP/MQTT responses and sensor readings for testing
 * in the Wokwi ESP32 simulator without requiring actual network connectivity.
 */

#ifdef WOKWI_SIMULATION

#include "wokwi_mock.h"

// Global mock instances
WokwiMockHTTP mockHTTP;
WokwiMockMQTT mockMQTT;

// ============================================================================
// WokwiMockHTTP Implementation
// ============================================================================

WokwiMockHTTP::WokwiMockHTTP() : lastStatusCode(200) {
    lastResponse = "{}";
}

bool WokwiMockHTTP::begin() {
    Serial.println("[WOKWI MOCK] HTTP client initialized (simulated)");
    return true;
}

int WokwiMockHTTP::POST(const char* endpoint, const String& payload) {
    Serial.printf("[WOKWI MOCK] HTTP POST to %s\n", endpoint);
    Serial.printf("[WOKWI MOCK] Payload: %s\n", payload.c_str());
    
    lastResponse = generateMockResponse(endpoint, "POST");
    lastStatusCode = 200;
    
    Serial.printf("[WOKWI MOCK] Response: %s\n", lastResponse.c_str());
    return lastStatusCode;
}

int WokwiMockHTTP::GET(const char* endpoint) {
    Serial.printf("[WOKWI MOCK] HTTP GET from %s\n", endpoint);
    
    lastResponse = generateMockResponse(endpoint, "GET");
    lastStatusCode = 200;
    
    Serial.printf("[WOKWI MOCK] Response: %s\n", lastResponse.c_str());
    return lastStatusCode;
}

String WokwiMockHTTP::getString() {
    return lastResponse;
}

void WokwiMockHTTP::end() {
    Serial.println("[WOKWI MOCK] HTTP connection closed (simulated)");
}

String WokwiMockHTTP::generateMockResponse(const char* endpoint, const char* method) {
    String response = "";
    
    // Diagnostics endpoint
    if (strstr(endpoint, "/diagnostics") != nullptr) {
        response = "{\"status\":\"success\",\"message\":\"Diagnostics received (simulated)\",\"stored\":true}";
    }
    // Security stats endpoint
    else if (strstr(endpoint, "/security/stats") != nullptr) {
        response = "{\"status\":\"success\",\"total_requests\":42,\"successful_auths\":40,\"failed_auths\":2,\"avg_auth_time_ms\":15.3}";
    }
    // Aggregation stats endpoint
    else if (strstr(endpoint, "/aggregation/stats") != nullptr) {
        response = "{\"status\":\"success\",\"total_aggregations\":100,\"avg_compression_ratio\":0.35,\"total_bytes_saved\":15000}";
    }
    // Fault injection endpoints
    else if (strstr(endpoint, "/fault") != nullptr) {
        if (strstr(endpoint, "/enable") != nullptr) {
            response = "{\"status\":\"success\",\"message\":\"Fault injection enabled (simulated)\"}";
        } else if (strstr(endpoint, "/disable") != nullptr) {
            response = "{\"status\":\"success\",\"message\":\"Fault injection disabled (simulated)\"}";
        } else if (strstr(endpoint, "/status") != nullptr) {
            response = "{\"status\":\"success\",\"enabled\":false,\"injected_faults\":0}";
        } else if (strstr(endpoint, "/reset") != nullptr) {
            response = "{\"status\":\"success\",\"message\":\"Fault counters reset (simulated)\"}";
        }
    }
    // OTA endpoints
    else if (strstr(endpoint, "/ota") != nullptr) {
        if (strstr(endpoint, "/check") != nullptr) {
            response = "{\"status\":\"success\",\"update_available\":false,\"current_version\":\"1.0.4\"}";
        } else if (strstr(endpoint, "/chunk") != nullptr) {
            response = "{\"status\":\"success\",\"chunk_received\":true}";
        } else if (strstr(endpoint, "/verify") != nullptr) {
            response = "{\"status\":\"success\",\"signature_valid\":true}";
        } else if (strstr(endpoint, "/complete") != nullptr) {
            response = "{\"status\":\"success\",\"update_completed\":true}";
        }
    }
    // Command endpoints
    else if (strstr(endpoint, "/command") != nullptr) {
        if (strstr(endpoint, "/queue") != nullptr) {
            response = "{\"status\":\"success\",\"command_id\":\"sim-cmd-001\"}";
        } else if (strstr(endpoint, "/poll") != nullptr) {
            response = "{\"status\":\"success\",\"commands_pending\":0,\"commands\":[]}";
        } else if (strstr(endpoint, "/result") != nullptr) {
            response = "{\"status\":\"success\",\"result_recorded\":true}";
        }
    }
    // Default response
    else {
        response = "{\"status\":\"success\",\"message\":\"Simulated response\"}";
    }
    
    return response;
}

// ============================================================================
// WokwiMockMQTT Implementation
// ============================================================================

WokwiMockMQTT::WokwiMockMQTT() : isConnected(false) {}

bool WokwiMockMQTT::begin(const char* broker, int port) {
    Serial.printf("[WOKWI MOCK] MQTT client initialized (broker: %s:%d - simulated)\n", broker, port);
    return true;
}

bool WokwiMockMQTT::connect(const char* clientId) {
    Serial.printf("[WOKWI MOCK] MQTT connected as '%s' (simulated)\n", clientId);
    isConnected = true;
    return true;
}

bool WokwiMockMQTT::publish(const char* topic, const char* payload) {
    Serial.printf("[WOKWI MOCK] MQTT publish to '%s': %s\n", topic, payload);
    lastTopic = String(topic);
    lastPayload = String(payload);
    return true;
}

bool WokwiMockMQTT::subscribe(const char* topic) {
    Serial.printf("[WOKWI MOCK] MQTT subscribed to '%s' (simulated)\n", topic);
    return true;
}

bool WokwiMockMQTT::connected() {
    return isConnected;
}

void WokwiMockMQTT::loop() {
    // No-op in simulation - no actual message processing
}

// ============================================================================
// Global Helper Functions
// ============================================================================

bool initWokwiMocks() {
    Serial.println("\n╔════════════════════════════════════════════════════════════╗");
    Serial.println("║              WOKWI SIMULATION MODE ACTIVE                 ║");
    Serial.println("╚════════════════════════════════════════════════════════════╝");
    Serial.println("[WOKWI] All network operations are mocked");
    Serial.println("[WOKWI] HTTP/MQTT responses are simulated");
    Serial.println("[WOKWI] Sensor readings are generated randomly\n");
    
    bool success = mockHTTP.begin() && mockMQTT.begin("test.mosquitto.org", 1883);
    
    if (success) {
        Serial.println("[WOKWI] ✓ Mock initialization complete\n");
    } else {
        Serial.println("[WOKWI] ✗ Mock initialization failed\n");
    }
    
    return success;
}

float simulateSensorReading(const char* sensorType) {
    // Generate realistic simulated sensor values
    float value = 0.0;
    
    if (strcmp(sensorType, "current") == 0) {
        // Simulate current: 0.1 - 5.0 A with some variation
        value = 0.5 + (random(0, 450) / 100.0);
    }
    else if (strcmp(sensorType, "voltage") == 0) {
        // Simulate voltage: 220-240V with small fluctuations
        value = 230.0 + (random(-10, 10) / 10.0);
    }
    else if (strcmp(sensorType, "power") == 0) {
        // Simulate power: 100-1000W
        value = 300.0 + random(0, 700);
    }
    else if (strcmp(sensorType, "temperature") == 0) {
        // Simulate temperature: 20-35°C
        value = 25.0 + (random(-5, 10) / 10.0);
    }
    else if (strcmp(sensorType, "frequency") == 0) {
        // Simulate frequency: 49.8-50.2 Hz
        value = 50.0 + (random(-2, 2) / 10.0);
    }
    else if (strcmp(sensorType, "power_factor") == 0) {
        // Simulate power factor: 0.85-0.99
        value = 0.90 + (random(0, 9) / 100.0);
    }
    else {
        // Default random value
        value = random(0, 100) / 10.0;
    }
    
    return value;
}

void printWokwiSimulationBanner() {
    Serial.println("\n");
    Serial.println("╔════════════════════════════════════════════════════════════╗");
    Serial.println("║                                                            ║");
    Serial.println("║              EcoWatt - WOKWI SIMULATION MODE              ║");
    Serial.println("║                                                            ║");
    Serial.println("║  All network operations are simulated for testing         ║");
    Serial.println("║  HTTP responses and MQTT messages are mocked             ║");
    Serial.println("║  Sensor readings are randomly generated                   ║");
    Serial.println("║                                                            ║");
    Serial.println("╔════════════════════════════════════════════════════════════╝");
    Serial.println("\n");
}

#endif // WOKWI_SIMULATION

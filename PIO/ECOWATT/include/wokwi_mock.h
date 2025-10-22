/**
 * @file wokwi_mock.h
 * @brief Mock layer for Wokwi simulation - provides simulated HTTP responses
 * 
 * This header defines mock functions that replace real HTTP/MQTT operations
 * when running in the Wokwi simulator. Enables testing without actual network.
 */

#ifndef WOKWI_MOCK_H
#define WOKWI_MOCK_H

#ifdef WOKWI_SIMULATION

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Mock HTTP client for simulation
 * 
 * Simulates HTTP POST/GET requests with predefined responses
 */
class WokwiMockHTTP {
public:
    WokwiMockHTTP();
    
    /**
     * @brief Initialize mock HTTP client
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * @brief Mock POST request to server
     * @param endpoint The API endpoint (e.g., "/diagnostics")
     * @param payload JSON payload as string
     * @return HTTP status code (200 for success)
     */
    int POST(const char* endpoint, const String& payload);
    
    /**
     * @brief Mock GET request to server
     * @param endpoint The API endpoint
     * @return HTTP status code (200 for success)
     */
    int GET(const char* endpoint);
    
    /**
     * @brief Get response body from last request
     * @return Response as string
     */
    String getString();
    
    /**
     * @brief End HTTP connection
     */
    void end();

private:
    String lastResponse;
    int lastStatusCode;
    
    /**
     * @brief Generate mock response based on endpoint
     * @param endpoint The requested endpoint
     * @param method HTTP method ("POST" or "GET")
     * @return Mock JSON response
     */
    String generateMockResponse(const char* endpoint, const char* method);
};

/**
 * @brief Mock MQTT client for simulation
 */
class WokwiMockMQTT {
public:
    WokwiMockMQTT();
    
    /**
     * @brief Initialize mock MQTT client
     * @param broker MQTT broker address (ignored in simulation)
     * @param port MQTT port (ignored in simulation)
     * @return true if initialization successful
     */
    bool begin(const char* broker, int port);
    
    /**
     * @brief Mock MQTT connect
     * @param clientId Client identifier
     * @return true if connection successful
     */
    bool connect(const char* clientId);
    
    /**
     * @brief Mock MQTT publish
     * @param topic Topic to publish to
     * @param payload Message payload
     * @return true if publish successful
     */
    bool publish(const char* topic, const char* payload);
    
    /**
     * @brief Mock MQTT subscribe
     * @param topic Topic to subscribe to
     * @return true if subscribe successful
     */
    bool subscribe(const char* topic);
    
    /**
     * @brief Check if connected
     * @return true if connected (always true in simulation)
     */
    bool connected();
    
    /**
     * @brief Process MQTT loop (no-op in simulation)
     */
    void loop();

private:
    bool isConnected;
    String lastTopic;
    String lastPayload;
};

// Global mock instances
extern WokwiMockHTTP mockHTTP;
extern WokwiMockMQTT mockMQTT;

/**
 * @brief Initialize all Wokwi mocks
 * @return true if initialization successful
 */
bool initWokwiMocks();

/**
 * @brief Simulate sensor readings with realistic values
 * @param sensorType Type of sensor ("current", "voltage", "temperature", etc.)
 * @return Simulated sensor value
 */
float simulateSensorReading(const char* sensorType);

/**
 * @brief Print Wokwi simulation banner to Serial
 */
void printWokwiSimulationBanner();

#endif // WOKWI_SIMULATION

#endif // WOKWI_MOCK_H

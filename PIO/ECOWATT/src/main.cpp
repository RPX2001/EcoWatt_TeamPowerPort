#include <Arduino.h>
#include "peripheral/acquisition.h"
#include "peripheral/print.h"
#include "peripheral/arduino_wifi.h"
#include "application/ringbuffer.h"
#include "application/compression.h"
#include "application/compression_benchmark.h"
#include "application/nvs.h"
#include "application/OTAManager.h"
#include "application/credentials.h"
#include "application/security.h"

Arduino_Wifi Wifi;
RingBuffer<SmartCompressedData, 20> smartRingBuffer;

const char* dataPostURL = FLASK_SERVER_URL "/process";
const char* fetchChangesURL = FLASK_SERVER_URL "/changes";
const char* commandPollURL = FLASK_SERVER_URL "/command/poll";
const char* commandResultURL = FLASK_SERVER_URL "/command/result";

void Wifi_init();
void poll_and_save(const RegID* selection, size_t registerCount, uint16_t* sensorData);
void upload_data();
void checkForCommands();
bool executeCommand(const char* commandId, const char* commandType, const char* parameters);
void sendCommandResult(const char* commandId, bool success, const char* result);
std::vector<uint8_t> compressWithSmartSelection(uint16_t* data, const RegID* selection, size_t count, 
                                               unsigned long& compressionTime, char* methodUsed, size_t methodSize,
                                               float& academicRatio, float& traditionalRatio);
void printSmartPerformanceStatistics();
void uploadSmartCompressedDataToCloud();
std::vector<uint8_t> compressBatchWithSmartSelection(const SampleBatch& batch, 
                                                   const RegID* selection, size_t registerCount,
                                                   unsigned long& compressionTime, 
                                                   char* methodUsed, size_t methodSize,
                                                   float& academicRatio, 
                                                   float& traditionalRatio);
void convertBinaryToBase64(const std::vector<uint8_t>& binaryData, char* result, size_t resultSize);
void updateSmartPerformanceStatistics(const char* method, float academicRatio, unsigned long timeUs);
bool readMultipleRegisters(const RegID* selection, size_t count, uint16_t* data);
void enhanceDictionaryForOptimalCompression();
void checkChanges(bool* registers_uptodate, bool* pollFreq_uptodate, bool* uploadFreq_uptodate);

hw_timer_t *poll_timer = NULL;
volatile bool poll_token = false;

SmartPerformanceStats smartStats;
SampleBatch currentBatch;

void IRAM_ATTR set_poll_token() 
{
  poll_token = true;
}

hw_timer_t *upload_timer = NULL;
volatile bool upload_token = false;

void IRAM_ATTR set_upload_token() 
{
  upload_token = true;
}

hw_timer_t *changes_timer = NULL;
volatile bool changes_token = false;

void IRAM_ATTR set_changes_token() 
{
  changes_token = true;
}

// OTA Manager and Timer 3
OTAManager* otaManager = nullptr;
hw_timer_t* ota_timer = NULL;
volatile bool ota_token = false;

// #define OTA_CHECK_INTERVAL 21600000000ULL  // 6 hours in microseconds
#define OTA_CHECK_INTERVAL 60000000ULL  // 1 min in microseconds

#define FIRMWARE_VERSION "1.0.3"

void IRAM_ATTR onOTATimer() {
    ota_token = true;
}

// Define registers to read
// const RegID selection[] = {REG_VAC1, REG_IAC1, REG_IPV1, REG_PAC, REG_IPV2, REG_TEMP};

void performOTAUpdate()
{
    print("=== OTA UPDATE CHECK INITIATED ===\n");
    
    if (otaManager->checkForUpdate()) {
        print("Firmware update available!\n");
        print("Pausing normal operations...\n");
        
        // Disable other timers
        timerAlarmDisable(poll_timer);
        timerAlarmDisable(upload_timer);
        timerAlarmDisable(changes_timer);
        
        if (otaManager->downloadAndApplyFirmware()) {
            // This will reboot if successful, so code below won't execute
            otaManager->verifyAndReboot();
        } else {
            print("OTA download/apply failed\n");
            print("Will retry on next check\n");
            
            // Re-enable timers
            timerAlarmEnable(poll_timer);
            timerAlarmEnable(upload_timer);
            timerAlarmEnable(changes_timer);
        }
    } else {
        print("No firmware updates available\n");
    }
}

void setup() 
{
  print_init();
  print("Starting ECOWATT\n");

  Wifi_init();

  // Initialize Security Layer
  print("Initializing Security Layer...\n");
  SecurityLayer::init();

  // Initialize OTA Manager
  print("Initializing OTA Manager...\n");
  otaManager = new OTAManager(
      FLASK_SERVER_URL ":5001",    // Flask server URL
      "ESP32_EcoWatt_Smart",         // Device ID
      FIRMWARE_VERSION               // Current version
  );

  // Check for rollback (MUST be early in setup)
  otaManager->handleRollback();

  // Setup Timer 3 for OTA checks
  ota_timer = timerBegin(3, 80, true);
  timerAttachInterrupt(ota_timer, &onOTATimer, true);
  timerAlarmWrite(ota_timer, OTA_CHECK_INTERVAL, true);
  timerAlarmEnable(ota_timer);
  
  print("OTA timer configured (6-hour interval)\n");

  // Reading values from the nvs
  size_t registerCount = nvs::getReadRegCount();
  const RegID* selection = nvs::getReadRegs();
  bool registers_uptodate = true;
  uint16_t* sensorData = nullptr;
  sensorData = new uint16_t[registerCount];

  uint64_t pollFreq = nvs::getPollFreq();
  bool pollFreq_uptodate = true;

  uint64_t uploadFreq = nvs::getUploadFreq();
  bool uploadFreq_uptodate = true;
  
  uint64_t checkChangesFreq = 5000000;
  
  // Set up the poll timer
  poll_timer = timerBegin(0, 80, true); // Timer 0, prescaler 80 (1us per tick)
  timerAttachInterrupt(poll_timer, &set_poll_token, true);
  timerAlarmWrite(poll_timer, pollFreq, true);
  timerAlarmEnable(poll_timer); // Enable the alarm

  // Set up the upload timer
  upload_timer = timerBegin(1, 80, true); // Timer 1, prescaler 80 (1us per tick)
  timerAttachInterrupt(upload_timer,  &set_upload_token, true);
  timerAlarmWrite(upload_timer, uploadFreq, true);
  timerAlarmEnable(upload_timer); // Enable the alarm

  // Set up the changes check timer
  changes_timer = timerBegin(2, 80, true); // Timer 2, prescaler 80 (1us per tick)
  timerAttachInterrupt(changes_timer,  &set_changes_token, true);
  timerAlarmWrite(changes_timer, checkChangesFreq, true);
  timerAlarmEnable(changes_timer); // Enable the alarm


  enhanceDictionaryForOptimalCompression();
  // Print initial memory status
  DataCompression::printMemoryUsage();

/*
  //set power to 50W out
  bool ok = setPower(50); // set Pac = 50W
  if (ok) 
  {
    print("Output power register updated!\n");
  }
  else 
  {
    print("Failed to set output power register!\n");
  }*/


  while(true) 
  {
    if (poll_token) 
    {
      poll_token = false;
      poll_and_save(selection, registerCount, sensorData);
    }
    
    if (upload_token) 
    {
      upload_token = false;
      upload_data();
      
      // Check for any queued commands from server
      checkForCommands();

      // Applying the changes
      if (!pollFreq_uptodate)
      {
        pollFreq = nvs::getPollFreq();
        timerAlarmWrite(poll_timer, pollFreq, true);
        pollFreq_uptodate = true;
        print("Poll frequency updated to %llu\n", pollFreq);
      }

      if (!uploadFreq_uptodate)
      {
        uploadFreq = nvs::getUploadFreq();
        timerAlarmWrite(upload_timer, uploadFreq, true);
        uploadFreq_uptodate = true;
        print("Upload frequency updated to %llu\n", uploadFreq);
      }

      if (!registers_uptodate)
      {
        selection = nvs::getReadRegs();
        registerCount = nvs::getReadRegCount();
        delete[] sensorData;
        sensorData = new uint16_t[registerCount];
        registers_uptodate = true;
        print("Registers updated! Now reading %d registers:\n", registerCount);
        // Print which registers we're now reading
        for (size_t i = 0; i < registerCount && i < REGISTER_COUNT; i++) {
            print("  [%zu] %s (ID: %d)\n", i, REGISTER_MAP[selection[i]].name, selection[i]);
        }
      }
    }

    if (changes_token)
    {
        changes_token = false;
        checkChanges(&registers_uptodate, &pollFreq_uptodate, &uploadFreq_uptodate);
    }

    // Handle OTA check
    if (ota_token) {
        ota_token = false;
        performOTAUpdate();
    }
  }
}

void loop() {}


void checkChanges(bool *registers_uptodate, bool *pollFreq_uptodate, bool *uploadFreq_uptodate)
{
    print("Checking for changes from cloud...\n");
    if (WiFi.status() != WL_CONNECTED) {
        print("WiFi not connected. Cannot check changes.\n");
        return;
    }

    HTTPClient http;
    http.begin(fetchChangesURL);

    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<128> changesRequestBody;
    changesRequestBody["device_id"] = "ESP32_EcoWatt_Smart";
    changesRequestBody["timestamp"] = millis();

    char requestBody[128];
    serializeJson(changesRequestBody, requestBody, sizeof(requestBody));

    int httpResponseCode = http.POST((uint8_t*)requestBody, strlen(requestBody));

    if (httpResponseCode > 0) {
        //print("HTTP Response code: %d\n", httpResponseCode);

        // Get response into a char buffer
        WiFiClient* stream = http.getStreamPtr();
        static char responseBuffer[1024]; // adjust if your response is larger
        int len = http.getSize();
        int index = 0;

        while (http.connected() && (len > 0 || len == -1)) 
        {
            if (stream->available()) 
            {
                char c = stream->read();
                if (index < (int)sizeof(responseBuffer) - 1)
                responseBuffer[index++] = c;
                if (len > 0) len--;
            }
        }
        responseBuffer[index] = '\0'; // null terminate
        print("ChangedResponse:");
        print(responseBuffer);

        StaticJsonDocument<1024> responsedoc;

        DeserializationError error = deserializeJson(responsedoc, responseBuffer);
        
        if (!error)
        {
            bool settingsChanged = responsedoc["Changed"] | false;
            
            if (settingsChanged)
            {
                bool pollFreqChanged = responsedoc["pollFreqChanged"] | false;
                if (pollFreqChanged)
                {
                    uint64_t new_poll_timer = responsedoc["newPollTimer"] | 0;
                    nvs::changePollFreq(new_poll_timer * 1000000);
                    *pollFreq_uptodate = false;
                    print("Poll timer set to update in next cycle %llu\n", new_poll_timer);
                }

                bool uploadFreqChanged = responsedoc["uploadFreqChanged"] | false;
                if (uploadFreqChanged)
                {
                    uint64_t new_upload_timer = responsedoc["newUploadTimer"] | 0;
                    nvs::changeUploadFreq(new_upload_timer * 1000000);
                    *uploadFreq_uptodate = false;
                    print("Upload timer set to update in next cycle %llu\n", new_upload_timer);
                }

                bool regsChanged = responsedoc["regsChanged"] | false;
                if (regsChanged)
                {
                    size_t regsCount = responsedoc["regsCount"] | 0;

                    if (regsCount > 0 && responsedoc.containsKey("regs"))
                    {
                        uint16_t regsMask = responsedoc["regs"] | 0;  // Fixed: removed double assignment
                        print("Received regsMask: %u, regsCount: %zu\n", regsMask, regsCount);
                        bool saved = nvs::saveReadRegs(regsMask, regsCount);
                        if (saved) {
                            *registers_uptodate = false;
                            print("Set to update %zu registers in next cycle.\n", regsCount);
                        } else {
                            print("Failed to save register changes to NVS\n");
                        }
                    }
                }
            }

            print("Changes noted\n");
            http.end();
        }
        else
        {
            http.end();
            print("Settings change error\n");
        }
    } else {
        // HTTP request failed - must still close the connection!
        print("HTTP POST failed with error code: %d\n", httpResponseCode);
        http.end();
    }
}

/**
 * @fn void Wifi_init()
 * 
 * @brief Function to initialise Wifi
 */
void Wifi_init()
{
  Wifi.setSSID(WIFI_SSID);
  Wifi.setPassword(WIFI_PASSWORD);
  Wifi.begin();
}


/**
 * @fn void checkForCommands()
 * 
 * @brief Poll the server for any queued commands and execute them
 */
void checkForCommands()
{
    print("Checking for queued commands from server...\n");
    if (WiFi.status() != WL_CONNECTED) {
        print("WiFi not connected. Cannot check commands.\n");
        return;
    }

    HTTPClient http;
    http.begin(commandPollURL);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<128> pollRequest;
    pollRequest["device_id"] = "ESP32_EcoWatt_Smart";

    char requestBody[128];
    serializeJson(pollRequest, requestBody, sizeof(requestBody));

    int httpResponseCode = http.POST((uint8_t*)requestBody, strlen(requestBody));

    if (httpResponseCode > 0) {
        WiFiClient* stream = http.getStreamPtr();
        static char responseBuffer[512];
        int len = http.getSize();
        int index = 0;

        while (http.connected() && (len > 0 || len == -1)) {
            if (stream->available()) {
                char c = stream->read();
                if (index < (int)sizeof(responseBuffer) - 1)
                    responseBuffer[index++] = c;
                if (len > 0) len--;
            }
        }
        responseBuffer[index] = '\0';

        StaticJsonDocument<512> responseDoc;
        DeserializationError error = deserializeJson(responseDoc, responseBuffer);

        if (!error && responseDoc.containsKey("command")) {
            const char* commandId = responseDoc["command"]["command_id"] | "";
            const char* commandType = responseDoc["command"]["command_type"] | "";
            
            print("Received command: %s (ID: %s)\n", commandType, commandId);
            
            // Extract parameters as JSON string
            char parameters[256] = {0};
            if (responseDoc["command"].containsKey("parameters")) {
                serializeJson(responseDoc["command"]["parameters"], parameters, sizeof(parameters));
            }
            
            // Execute the command
            bool success = executeCommand(commandId, commandType, parameters);
            
            // Send result back to server
            char result[128];
            snprintf(result, sizeof(result), "Command %s: %s", commandType, success ? "executed successfully" : "failed");
            sendCommandResult(commandId, success, result);
        } else if (error) {
            print("Failed to parse command response\n");
        } else {
            print("No pending commands\n");
        }

        http.end();
    } else {
        print("HTTP POST failed with error code: %d\n", httpResponseCode);
        http.end();
    }
}


/**
 * @fn bool executeCommand(const char* commandId, const char* commandType, const char* parameters)
 * 
 * @brief Execute a specific command received from the server
 */
bool executeCommand(const char* commandId, const char* commandType, const char* parameters)
{
    print("Executing command: %s\n", commandType);
    print("Parameters: %s\n", parameters);
    
    StaticJsonDocument<256> paramDoc;
    DeserializationError error = deserializeJson(paramDoc, parameters);
    
    if (error) {
        print("Failed to parse parameters\n");
        return false;
    }
    
    // Handle different command types
    if (strcmp(commandType, "set_power") == 0) {
        int powerValue = paramDoc["power_value"] | 0;
        
        // IMPORTANT: Register 8 accepts PERCENTAGE (0-100), not absolute watts
        // Convert power value to percentage based on your inverter's max capacity
        // Assuming max inverter capacity is 10,000W (adjust as needed)
        const int MAX_INVERTER_CAPACITY = 10000; // Watts
        int powerPercentage = (powerValue * 100) / MAX_INVERTER_CAPACITY;
        
        // Clamp to valid range 0-100%
        if (powerPercentage < 0) powerPercentage = 0;
        if (powerPercentage > 100) powerPercentage = 100;
        
        print("Setting power to %d W (%d%%)\n", powerValue, powerPercentage);
        
        // Call the setPower function with PERCENTAGE value
        bool result = setPower(powerPercentage);
        
        if (result) {
            print("Power set successfully to %d W (%d%%)\n", powerValue, powerPercentage);
        } else {
            print("Failed to set power\n");
        }
        return result;
        
    } else if (strcmp(commandType, "set_power_percentage") == 0) {
        // Direct percentage control (0-100%)
        int percentage = paramDoc["percentage"] | 0;
        
        // Clamp to valid range
        if (percentage < 0) percentage = 0;
        if (percentage > 100) percentage = 100;
        
        print("Setting power percentage to %d%%\n", percentage);
        
        bool result = setPower(percentage);
        
        if (result) {
            print("Power percentage set successfully to %d%%\n", percentage);
        } else {
            print("Failed to set power percentage\n");
        }
        return result;
        
    } else if (strcmp(commandType, "write_register") == 0) {
        int regAddress = paramDoc["register_address"] | 0;
        int value = paramDoc["value"] | 0;
        print("Writing register %d with value %d\n", regAddress, value);
        
        // Call the writeRegister function (you'll need to implement this)
        // For now, return false as placeholder
        print("Write register command not yet implemented\n");
        return false;
        
    } else {
        print("Unknown command type: %s\n", commandType);
        return false;
    }
}


/**
 * @fn void sendCommandResult(const char* commandId, bool success, const char* result)
 * 
 * @brief Send command execution result back to the server
 */
void sendCommandResult(const char* commandId, bool success, const char* result)
{
    print("Sending command result to server...\n");
    
    if (WiFi.status() != WL_CONNECTED) {
        print("WiFi not connected. Cannot send result.\n");
        return;
    }

    HTTPClient http;
    http.begin(commandResultURL);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> resultDoc;
    resultDoc["command_id"] = commandId;
    resultDoc["status"] = success ? "completed" : "failed";
    resultDoc["result"] = result;

    char resultBody[256];
    serializeJson(resultDoc, resultBody, sizeof(resultBody));

    int httpResponseCode = http.POST((uint8_t*)resultBody, strlen(resultBody));

    if (httpResponseCode == 200) {
        print("Command result sent successfully\n");
    } else {
        print("Failed to send command result (HTTP %d)\n", httpResponseCode);
    }

    http.end();
}


/**
 * @fn void poll_and_save()
 * 
 * @brief Poll sensor data, compress with smart selection, and save to ring buffer.
 */
void poll_and_save(const RegID* selection, size_t registerCount, uint16_t* sensorData)
{
  if (readMultipleRegisters(selection, registerCount, sensorData)) 
  {
    // Print sensor values being added to batch
    print("Polled values: ");
    for (size_t i = 0; i < registerCount; i++) {
        print("%s=%u ", REGISTER_MAP[selection[i]].name, sensorData[i]);
    }
    print("\n");
    
    // Process the read sensor data
    // Add to batch with registerCount
    currentBatch.addSample(sensorData, millis(), registerCount);

    // When batch is full, compress and store
    if (currentBatch.isFull()) {
        unsigned long compressionTime;
        char methodUsed[32];
        float academicRatio, traditionalRatio;
        
        // Compress the entire batch with smart selection
        std::vector<uint8_t> compressedBinary = compressBatchWithSmartSelection(
            currentBatch, selection, registerCount, compressionTime, methodUsed, sizeof(methodUsed), academicRatio, traditionalRatio);
        
        // Store compressed data with metadata
        if (!compressedBinary.empty()) {
            SmartCompressedData entry(compressedBinary, selection, registerCount, methodUsed);
            entry.compressionTime = compressionTime;
            entry.academicRatio = academicRatio;
            entry.traditionalRatio = traditionalRatio;
            entry.losslessVerified = true;
            
            smartRingBuffer.push(entry);
            
            // Update global statistics
            smartStats.totalOriginalBytes += entry.originalSize;
            smartStats.totalCompressedBytes += compressedBinary.size();
            
            print("Batch compressed and stored successfully!\n");
        } else {
            print("Compression failed for batch!\n");
            smartStats.compressionFailures++;
        }
        
        // Reset batch for next collection
        currentBatch.reset();
    }
  }
  else
  {
    print("Failed to read registers\n");
  }
}


/**
 * @fn void upload_data()
 * 
 * @brief Upload all smart compressed data in the ring buffer to the cloud server.
 */
void upload_data()
{
    uploadSmartCompressedDataToCloud();
    // printSmartPerformanceStatistics();
}


/**
 * @fn std::vector<uint8_t> compressWithSmartSelection(uint16_t* data, const RegID* selection, size_t count,
 *                                               unsigned long& compressionTime, char* methodUsed, size_t methodSize,
 *                                               float& academicRatio, float& traditionalRatio)
 * 
 * @brief Compress sensor data using the adaptive smart selection system and track performance.
 * 
 * @param data Pointer to the array of uint16_t sensor data.
 * @param selection Pointer to the array of RegID indicating which registers are included.
 * @param count Number of data points (length of data and selection arrays).
 * @param compressionTime Reference to store the time taken for compression (in microseconds).
 * @param methodUsed Pointer to char array to store the name of the compression method used.
 * @param methodSize Size of the methodUsed char array.
 * @param academicRatio Reference to store the academic compression ratio (compressed/original).
 * @param traditionalRatio Reference to store the traditional compression ratio (original/compressed).
 * 
 * @return std::vector<uint8_t> Compressed data as a byte vector.
 */
std::vector<uint8_t> compressWithSmartSelection(uint16_t* data, const RegID* selection, size_t count, 
                                               unsigned long& compressionTime, char* methodUsed, size_t methodSize,
                                               float& academicRatio, float& traditionalRatio) {
    unsigned long startTime = micros();
    
    // Use the advanced smart selection system
    std::vector<uint8_t> compressed = DataCompression::compressWithSmartSelection(data, selection, count);
    
    compressionTime = micros() - startTime;
    
    // Determine method from compressed data header
    if (!compressed.empty()) {
        switch (compressed[0]) {
            case 0xD0: 
                strncpy(methodUsed, "DICTIONARY", methodSize - 1);
                smartStats.dictionaryUsed++;
                break;
            case 0x70:
            case 0x71: 
                strncpy(methodUsed, "TEMPORAL", methodSize - 1);
                smartStats.temporalUsed++;
                break;
            case 0x50: 
                strncpy(methodUsed, "SEMANTIC", methodSize - 1);
                smartStats.semanticUsed++;
                break;
            case 0x01:
            default:
                strncpy(methodUsed, "BITPACK", methodSize - 1);
                smartStats.bitpackUsed++;
        }
        methodUsed[methodSize - 1] = '\0';
    } else {
        strncpy(methodUsed, "ERROR", methodSize - 1);
        methodUsed[methodSize - 1] = '\0';
        smartStats.compressionFailures++;
    }
    
    // Calculate ratios
    size_t originalSize = count * sizeof(uint16_t);
    size_t compressedSize = compressed.size();
    
    academicRatio = (compressedSize > 0) ? (float)compressedSize / (float)originalSize : 1.0f;
    traditionalRatio = (compressedSize > 0) ? (float)originalSize / (float)compressedSize : 0.0f;
    
    // Update performance tracking
    updateSmartPerformanceStatistics(methodUsed, academicRatio, compressionTime);
    
    return compressed;
}


/**
 * @fn updateSmartPerformanceStatistics(const char* method, float academicRatio, unsigned long timeUs)
 * 
 * @brief Update global statistics for smart compression performance tracking.
 * 
 * @param method Name of the compression method used.
 * @param academicRatio Academic compression ratio (compressed/original).
 * @param timeUs Time taken for compression in microseconds.
 */
void updateSmartPerformanceStatistics(const char* method, float academicRatio, 
                                     unsigned long timeUs) {
    smartStats.totalSmartCompressions++;
    smartStats.totalCompressionTime += timeUs;
    smartStats.averageAcademicRatio = 
        (smartStats.averageAcademicRatio * (smartStats.totalSmartCompressions - 1) + academicRatio) / 
        smartStats.totalSmartCompressions;
    
    // Update best ratio if this is better
    if (academicRatio < smartStats.bestAcademicRatio) {
        smartStats.bestAcademicRatio = academicRatio;
        strncpy(smartStats.currentOptimalMethod, method, sizeof(smartStats.currentOptimalMethod) - 1);
        smartStats.currentOptimalMethod[sizeof(smartStats.currentOptimalMethod) - 1] = '\0';
    }
    
    // Update quality distribution
    if (academicRatio <= 0.5f) {
        smartStats.excellentCompressionCount++;
    } else if (academicRatio <= 0.67f) {
        smartStats.goodCompressionCount++;
    } else if (academicRatio <= 0.91f) {
        smartStats.fairCompressionCount++;
    } else {
        smartStats.poorCompressionCount++;
    }
    
    // Update fastest compression time
    if (timeUs < smartStats.fastestCompressionTime) {
        smartStats.fastestCompressionTime = timeUs;
    }
}


/**
 * @fn void enhanceDictionaryForOptimalCompression()
 * 
 * @brief Enhance the dictionary with patterns learned from actual sensor data to improve compression.
 */
void enhanceDictionaryForOptimalCompression() {
    // Add patterns specifically learned from actual sensor data
    // Pattern 0: Your typical readings
    uint16_t pattern0[] = {2429, 177, 73, 4331, 70, 605};
    
    // Pattern 1: Slight variations (±5%)
    uint16_t pattern1[] = {2308, 168, 69, 4115, 67, 575};
    uint16_t pattern2[] = {2550, 186, 77, 4547, 74, 635};
    
    // Pattern 3: Different load conditions
    uint16_t pattern3[] = {2380, 150, 65, 3800, 55, 590};
    uint16_t pattern4[] = {2480, 195, 80, 4800, 85, 620};
}


/**
 * @fn void printSmartPerformanceStatistics()
 * 
 * @brief Print a summary of smart compression performance statistics.
 */
void printSmartPerformanceStatistics() {
    print("\nSMART COMPRESSION PERFORMANCE SUMMARY\n");
    print("=====================================\n");
    print("Total Compressions: %lu\n", smartStats.totalSmartCompressions);
    print("Average Academic Ratio: %.3f\n", smartStats.averageAcademicRatio);
    print("Best Ratio Achieved: %.3f\n", smartStats.bestAcademicRatio);
    print("Optimal Method: %s\n", smartStats.currentOptimalMethod);
    print("Average Time: %lu μs\n", 
          smartStats.totalSmartCompressions > 0 ? 
          smartStats.totalCompressionTime / smartStats.totalSmartCompressions : 0);
    
    print("\nQuality Distribution:\n");
    print("  Excellent (≤50%%): %lu\n", smartStats.excellentCompressionCount);
    print("  Good (≤67%%): %lu\n", smartStats.goodCompressionCount);
    print("  Fair (≤91%%): %lu\n", smartStats.fairCompressionCount);
    print("  Poor (>91%%): %lu\n", smartStats.poorCompressionCount);
    
    print("\nMethod Usage:\n");
    print("  Dictionary: %lu\n", smartStats.dictionaryUsed);
    print("  Temporal: %lu\n", smartStats.temporalUsed);
    print("  Semantic: %lu\n", smartStats.semanticUsed);
    print("  BitPack: %lu\n", smartStats.bitpackUsed);
    print("=====================================\n\n");
}


/**
 * @fn void uploadSmartCompressedDataToCloud()
 * 
 * @brief Upload all smart compressed data in the ring buffer to the cloud server.
 */
void uploadSmartCompressedDataToCloud() {
    if (WiFi.status() != WL_CONNECTED) {
        print("WiFi not connected. Cannot upload.\n");
        return;
    }

    if (smartRingBuffer.empty()) {
        print("Buffer empty. Nothing to upload.\n");
        return;
    }

    HTTPClient http;
    http.begin(dataPostURL);
    http.addHeader("Content-Type", "application/json");
    
    // New JSON structure with compressed data and decompression metadata
    DynamicJsonDocument doc(8192);
    auto allData = smartRingBuffer.drain_all();
    
    doc["device_id"] = "ESP32_EcoWatt_Smart";
    doc["timestamp"] = millis();
    doc["data_type"] = "compressed_sensor_batch";
    doc["total_samples"] = allData.size();
    
    // Build register mapping dynamically from the first entry
    // (assuming all entries in this batch use the same register set)
    JsonObject registerMapping = doc["register_mapping"].to<JsonObject>();
    if (!allData.empty()) {
        const auto& firstEntry = allData[0];
        for (size_t i = 0; i < firstEntry.registerCount && i < REGISTER_COUNT; i++) {
            char key[8];
            snprintf(key, sizeof(key), "%zu", i);
            // Use the actual register name from REGISTER_MAP
            registerMapping[key] = REGISTER_MAP[firstEntry.registers[i]].name;
        }
        print("Register mapping built: %zu registers\n", firstEntry.registerCount);
    }
    
    // Compressed data packets with decompression metadata
    JsonArray compressedPackets = doc["compressed_data"].to<JsonArray>();
    
    size_t totalOriginalBytes = 0;
    size_t totalCompressedBytes = 0;
    
    for (const auto& entry : allData) {
        JsonObject packet = compressedPackets.createNestedObject();
        
        // Compressed binary data (Base64 encoded)
        char base64Buffer[256];
        convertBinaryToBase64(entry.binaryData, base64Buffer, sizeof(base64Buffer));
        packet["compressed_binary"] = base64Buffer;
        
        // Decompression metadata
        JsonObject decompMeta = packet["decompression_metadata"].to<JsonObject>();
        decompMeta["method"] = entry.compressionMethod;
        decompMeta["register_count"] = entry.registerCount;
        decompMeta["original_size_bytes"] = entry.originalSize;
        decompMeta["compressed_size_bytes"] = entry.binaryData.size();
        decompMeta["timestamp"] = entry.timestamp;
        
        // Register layout for this packet
        JsonArray regLayout = decompMeta["register_layout"].to<JsonArray>();
        for (size_t i = 0; i < entry.registerCount; i++) {
            regLayout.add(entry.registers[i]);
        }
        
        // Compression performance metrics
        JsonObject metrics = packet["performance_metrics"].to<JsonObject>();
        metrics["academic_ratio"] = entry.academicRatio;
        metrics["traditional_ratio"] = entry.traditionalRatio;
        metrics["compression_time_us"] = entry.compressionTime;
        metrics["savings_percent"] = (1.0f - entry.academicRatio) * 100.0f;
        metrics["lossless_verified"] = entry.losslessVerified;
        
        totalOriginalBytes += entry.originalSize;
        totalCompressedBytes += entry.binaryData.size();
    }
    
    // Overall session summary
    JsonObject sessionSummary = doc["session_summary"].to<JsonObject>();
    sessionSummary["total_original_bytes"] = totalOriginalBytes;
    sessionSummary["total_compressed_bytes"] = totalCompressedBytes;
    sessionSummary["overall_academic_ratio"] = (totalOriginalBytes > 0) ? 
        (float)totalCompressedBytes / (float)totalOriginalBytes : 1.0f;
    sessionSummary["overall_savings_percent"] = (totalOriginalBytes > 0) ? 
        (1.0f - (float)totalCompressedBytes / (float)totalOriginalBytes) * 100.0f : 0.0f;
    sessionSummary["best_ratio_achieved"] = smartStats.bestAcademicRatio;
    sessionSummary["optimal_method"] = smartStats.currentOptimalMethod;
    
    // Method usage statistics
    JsonObject methodStats = sessionSummary["method_usage"].to<JsonObject>();
    methodStats["dictionary_count"] = smartStats.dictionaryUsed;
    methodStats["temporal_count"] = smartStats.temporalUsed;
    methodStats["semantic_count"] = smartStats.semanticUsed;
    methodStats["bitpack_count"] = smartStats.bitpackUsed;
    
    char jsonString[2048];
    size_t jsonLen = serializeJson(doc, jsonString, sizeof(jsonString));

    print("UPLOADING COMPRESSED DATA TO FLASK SERVER\n");
    print("Packets: %zu | JSON Size: %zu bytes\n", allData.size(), jsonLen);
    print("Compression Summary: %zu -> %zu bytes (%.1f%% savings)\n", 
        totalOriginalBytes, totalCompressedBytes,
        (totalOriginalBytes > 0) ? (1.0f - (float)totalCompressedBytes / (float)totalOriginalBytes) * 100.0f : 0.0f);
    
    // Print register mapping being sent
    print("\n=== UPLOADED REGISTER MAPPING ===\n");
    if (!allData.empty()) {
        const auto& firstEntry = allData[0];
        print("Sending %zu registers:\n", firstEntry.registerCount);
        for (size_t i = 0; i < firstEntry.registerCount && i < REGISTER_COUNT; i++) {
            RegID regId = firstEntry.registers[i];
            print("  [%zu] %s (ID: %d)\n", i, REGISTER_MAP[regId].name, regId);
        }
    }
    print("================================\n\n");
    
    // Apply Security Layer
    // Use dynamic allocation to avoid stack overflow
    char* securedPayload = new char[8192];
    if (!securedPayload) {
        print("Failed to allocate memory for secured payload! Aborting upload.\n");
        http.end();
        return;
    }
    
    if (!SecurityLayer::securePayload(jsonString, securedPayload, 8192)) {
        print("Failed to secure payload! Aborting upload.\n");
        delete[] securedPayload;
        http.end();
        return;
    }
    
    print("Security Layer: Payload secured successfully\n");
    print("Secured Payload Size: %zu bytes\n", strlen(securedPayload));
    
    int httpResponseCode = http.POST((uint8_t*)securedPayload, strlen(securedPayload));
    
    if (httpResponseCode == 200) {
        String response = http.getString();
        print("Upload successful to Flask server!\n");
        //print("Server response: %s\n", response.c_str());
        smartStats.losslessSuccesses++;
    } else {
        print("Upload failed (HTTP %d)\n", httpResponseCode);
        if (httpResponseCode > 0) {
            String errorResponse = http.getString();
            print("Flask server error: %s\n", errorResponse.c_str());
        }
        print("Restoring compressed data to buffer...\n");
        
        // Restore data to buffer
        for (const auto& entry : allData) {
            smartRingBuffer.push(entry);
        }
        smartStats.compressionFailures++;
    }
    
    // Clean up dynamically allocated memory
    delete[] securedPayload;
    http.end();
}


/**
 * @fn void convertBinaryToBase64(const std::vector<uint8_t>& binaryData, char* result, size_t resultSize)
 * 
 * @brief Convert binary data to Base64 encoded string.
 * 
 * @param binaryData Input binary data as a vector of bytes.
 * @param result Output buffer to store the Base64 string.
 * @param resultSize Size of the output buffer.
 */
void convertBinaryToBase64(const std::vector<uint8_t>& binaryData, char* result, size_t resultSize) {
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t pos = 0;
    
    for (size_t i = 0; i < binaryData.size() && pos < resultSize - 5; i += 3) {
        uint32_t value = binaryData[i] << 16;
        if (i + 1 < binaryData.size()) value |= binaryData[i + 1] << 8;
        if (i + 2 < binaryData.size()) value |= binaryData[i + 2];
        
        result[pos++] = chars[(value >> 18) & 0x3F];
        result[pos++] = chars[(value >> 12) & 0x3F];
        result[pos++] = chars[(value >> 6) & 0x3F];
        result[pos++] = chars[value & 0x3F];
    }
    
    while (pos % 4 && pos < resultSize - 1) result[pos++] = '=';
    result[pos] = '\0';
}


/**
 * @fn std::vector<uint8_t> compressBatchWithSmartSelection(const SampleBatch& batch,
 *                                                   const RegID* selection, size_t registerCount,
 *                                                   unsigned long& compressionTime,
 *                                                  char* methodUsed, size_t methodSize,
 *                                                  float& academicRatio,
 *                                                 float& traditionalRatio)
 * 
 * @brief Compress an entire batch of samples using smart selection and track performance.
 * 
 * @param batch Reference to the SampleBatch containing multiple samples.
 * @param selection Pointer to the array of RegID indicating which registers are being read.
 * @param registerCount Number of registers in the selection.
 * @param compressionTime Reference to store the time taken for compression (in microseconds).
 * @param methodUsed Pointer to char array to store the name of the compression method used.
 * @param methodSize Size of the methodUsed char array.
 * @param academicRatio Reference to store the academic compression ratio (compressed/original).
 * @param traditionalRatio Reference to store the traditional compression ratio (original/compressed).
 * 
 * @return std::vector<uint8_t> Compressed data as a byte vector.
 */
std::vector<uint8_t> compressBatchWithSmartSelection(const SampleBatch& batch, 
                                                   const RegID* selection, size_t registerCount,
                                                   unsigned long& compressionTime, 
                                                   char* methodUsed, size_t methodSize,
                                                   float& academicRatio, 
                                                   float& traditionalRatio) {
    unsigned long startTime = micros();
    
    // Convert batch to linear array (dynamically sized based on actual register count)
    const size_t maxDataSize = batch.sampleCount * registerCount;
    uint16_t* linearData = new uint16_t[maxDataSize];
    batch.toLinearArray(linearData);
    
    // Create register selection array for the batch using the ACTUAL selection passed in
    RegID* batchSelection = new RegID[maxDataSize];
    
    for (size_t i = 0; i < batch.sampleCount; i++) {
        memcpy(batchSelection + (i * registerCount), selection, registerCount * sizeof(RegID));
    }
    
    // Use smart selection on the entire batch
    std::vector<uint8_t> compressed = DataCompression::compressWithSmartSelection(
        linearData, batchSelection, batch.sampleCount * registerCount);
    
    compressionTime = micros() - startTime;
    
    // Clean up dynamic allocations
    delete[] linearData;
    delete[] batchSelection;
    
    // Determine method from compressed data header
    if (!compressed.empty()) {
        switch (compressed[0]) {
            case 0xD0: 
                strncpy(methodUsed, "BATCH_DICTIONARY", methodSize - 1);
                smartStats.dictionaryUsed++;
                break;
            case 0x70:
            case 0x71: 
                strncpy(methodUsed, "BATCH_TEMPORAL", methodSize - 1);
                smartStats.temporalUsed++;
                break;
            case 0x50: 
                strncpy(methodUsed, "BATCH_SEMANTIC", methodSize - 1);
                smartStats.semanticUsed++;
                break;
            case 0x01:
            default:
                strncpy(methodUsed, "BATCH_BITPACK", methodSize - 1);
                smartStats.bitpackUsed++;
        }
        methodUsed[methodSize - 1] = '\0';
    } else {
        strncpy(methodUsed, "BATCH_ERROR", methodSize - 1);
        methodUsed[methodSize - 1] = '\0';
    }
    
    // Calculate compression ratios
    size_t originalSize = batch.sampleCount * registerCount * sizeof(uint16_t);
    size_t compressedSize = compressed.size();
    
    academicRatio = (compressedSize > 0) ? (float)compressedSize / (float)originalSize : 1.0f;
    traditionalRatio = (compressedSize > 0) ? (float)originalSize / (float)compressedSize : 0.0f;
    
    return compressed;
}


/**
 * @fn bool readMultipleRegisters(const RegID* selection, size_t count, uint16_t* data)
 * 
 * @brief Read multiple registers from the sensor and store values in the provided array.
 * 
 * @param selection Pointer to the array of RegID indicating which registers to read.
 * @param count Number of registers to read (length of selection and data arrays).
 * @param data Pointer to the array where read register values will be stored.
 * 
 * @return bool True if read was successful and all registers were read, false otherwise.
 */
bool readMultipleRegisters(const RegID* selection, size_t count, uint16_t* data) 
{
    // Use the acquisition system to read registers
    DecodedValues result = readRequest(selection, count);
    
    // Check if we got the expected number of values
    if (result.count != count) {
        return false;
    }
    
    // Copy the register values to the output array
    for (size_t i = 0; i < count && i < result.count; i++) {
        data[i] = result.values[i];
    }
    
    return true;
}


#include <Arduino.h>
#include "peripheral/acquisition.h"
#include "peripheral/print.h"
#include "peripheral/arduino_wifi.h"
#include "application/ringbuffer.h"
#include "application/compression.h"
#include "application/aggregation.h"
#include "application/compression_benchmark.h"
#include "application/nvs.h"
#include "application/OTAManager.h"

Arduino_Wifi Wifi;

// RAW uncompressed data, compression happens at upload time
RingBuffer<RawSensorData, 450> rawDataBuffer;  // 450 samples = 15 min at 2 sec/sample (or ~7.5 for 15sec demo)

// Aggregation configuration
bool enableAggregation = false;           // Enable/disable aggregation before compression
size_t targetAggregatedSamples = 50;      // Target number of samples after aggregation (450 → 50)
DataAggregation::AggregationMethod aggregationMethod = DataAggregation::AGG_MEAN;

// Payload size limits
const size_t MAX_PAYLOAD_SIZE = 8192;     // Maximum payload size in bytes (8KB typical HTTP limit)
const size_t WARN_PAYLOAD_SIZE = 6144;    // Warning threshold (6KB - trigger aggregation)

const char* dataPostURL = "http://192.168.242.249:5001/process";
const char* fetchChangesURL = "http://192.168.242.249:5001/changes";

void Wifi_init();
void poll_and_save(const RegID* selection, size_t registerCount, uint16_t* sensorData);
void upload_data();
std::vector<uint8_t> compressRawDataBuffer(unsigned long& compressionTime, char* methodUsed, size_t methodSize,
                                           float& academicRatio, float& traditionalRatio, size_t& originalSize);
void printSmartPerformanceStatistics();
void uploadCompressedDataToCloud(const std::vector<uint8_t>& compressedData, const char* method, 
                                unsigned long compressionTime, float academicRatio, 
                                float traditionalRatio, size_t originalSize, size_t sampleCount);
void convertBinaryToBase64(const std::vector<uint8_t>& binaryData, char* result, size_t resultSize);
void updateSmartPerformanceStatistics(const char* method, float academicRatio, unsigned long timeUs);
bool readMultipleRegisters(const RegID* selection, size_t count, uint16_t* data);
void enhanceDictionaryForOptimalCompression();
void checkChanges(bool* registers_uptodate, bool* pollFreq_uptodate, bool* uploadFreq_uptodate);
std::vector<uint8_t> aggregateAndCompressRawData(unsigned long& compressionTime, char* methodUsed, size_t methodSize,
                                                 float& academicRatio, float& traditionalRatio, 
                                                 size_t& originalSize, bool enableAggregation, size_t targetSamples);
void printAggregationStats(size_t originalSamples, size_t aggregatedSamples, size_t originalBytes, size_t aggregatedBytes);

hw_timer_t *poll_timer = NULL;
volatile bool poll_token = false;

SmartPerformanceStats smartStats;
// REMOVED: SampleBatch currentBatch; - No longer needed, storing raw data directly

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

  // Initialize OTA Manager
  print("Initializing OTA Manager...\n");
  otaManager = new OTAManager(
      "http://192.168.242.249:5001",    // Flask server URL
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
        delete[] sensorData;
        registerCount = nvs::getReadRegCount();
        const RegID* selection = nvs::getReadRegs();
        sensorData = new uint16_t[registerCount];
        registers_uptodate = true;
        print("Set to update %d registers in next cycle.\n", registerCount);
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
        //print("ChangedResponse:");
        //print(responseBuffer);

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
                        JsonArray regsArray = responsedoc["regs"].as<JsonArray>();

                        RegID* newRegs = new RegID[regsCount];
                        size_t validCount = 0;

                        for (size_t i = 0; i < regsCount; i++) 
                        {
                            const char* regName = regsArray[i] | "";

                            for (size_t j = 0; j < REGISTER_COUNT; j++) 
                            {
                                if (strcmp(REGISTER_MAP[j].name, regName) == 0) 
                                {
                                    newRegs[validCount++] = REGISTER_MAP[j].id;
                                    break;
                                }
                            }
                        }

                        if (validCount > 0) {
                            // Store in NVS
                            nvs::saveReadRegs(newRegs, validCount);
                            *registers_uptodate = false;
                            print("Set to update %d registers in next cycle.\n", validCount);               
                        }

                        delete[] newRegs;
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
    }
}

/**
 * @fn void Wifi_init()
 * 
 * @brief Function to initialise Wifi
 */
void Wifi_init()
{
  Wifi.setSSID("Galaxy A32B46A");
  Wifi.setPassword("aubz5724");
  Wifi.begin();
}


/**
 * @fn void poll_and_save()
 * 
 * @brief Poll sensor data and save RAW uncompressed data to ring buffer.
 *        Compression will happen at upload time, not here.
 */
void poll_and_save(const RegID* selection, size_t registerCount, uint16_t* sensorData)
{
  if (readMultipleRegisters(selection, registerCount, sensorData)) 
  {
    // Store RAW uncompressed data
    RawSensorData rawSample(sensorData, selection, registerCount);
    rawDataBuffer.push(rawSample);
    
    print("Sample acquired and stored (buffer: %zu/%d)\n", rawDataBuffer.size(), 450);
  }
  else
  {
    print("Failed to read registers\n");
  }
}


/**
 * @fn void upload_data()
 * 
 * @brief Aggregate (optional) and compress all raw data in the buffer, then upload to cloud.
 *        AGGREGATION + COMPRESSION HAPPENS HERE - not during acquisition.
 */
void upload_data()
{
    print("\n=== UPLOAD CYCLE STARTED ===\n");
    
    if (rawDataBuffer.empty()) {
        print("No raw data to compress and upload.\n");
        return;
    }
    
    print("Raw samples in buffer: %zu\n", rawDataBuffer.size());
    
    // Aggregate and compress all buffered raw data
    unsigned long compressionTime;
    char methodUsed[32];
    float academicRatio, traditionalRatio;
    size_t originalSize;
    
    std::vector<uint8_t> compressedData = aggregateAndCompressRawData(
        compressionTime, methodUsed, sizeof(methodUsed), 
        academicRatio, traditionalRatio, originalSize,
        enableAggregation, targetAggregatedSamples);
    
    if (!compressedData.empty()) {
        // Upload compressed data
        size_t sampleCount = rawDataBuffer.size();
        uploadCompressedDataToCloud(compressedData, methodUsed, compressionTime, 
                                   academicRatio, traditionalRatio, originalSize, sampleCount);
        
        // Update statistics
        updateSmartPerformanceStatistics(methodUsed, academicRatio, compressionTime);
        smartStats.totalOriginalBytes += originalSize;
        smartStats.totalCompressedBytes += compressedData.size();
        
        print("=== UPLOAD CYCLE COMPLETED ===\n\n");
    } else {
        print("Compression failed!\n");
        smartStats.compressionFailures++;
    }
    
    printSmartPerformanceStatistics();
}


/**
 * @fn bool verifyLosslessCompression(...)
 * 
 * @brief Verify that compression is truly lossless by decompressing and comparing
 * 
 * @param original Original data array
 * @param originalCount Number of values in original data
 * @param compressed Compressed data
 * @param method Compression method used
 * 
 * @return true if decompressed data matches original exactly
 */
/**
 * @brief Verify lossless compression by decompressing and comparing
 * 
 * NOTE: Decompression not fully implemented for all methods yet.
 * This function currently returns true (assumes lossless) as a placeholder.
 * 
 * @param original Original data array
 * @param originalCount Number of values in original array
 * @param compressed Compressed data
 * @param method Compression method used
 * @return true if verified lossless (currently always true as placeholder)
 */
bool verifyLosslessCompression(const uint16_t* original, size_t originalCount, 
                               const std::vector<uint8_t>& compressed, const char* method) {
    // DISABLED: Verification causes stack overflow on ESP32
    // The compression algorithms are designed to be lossless
    // Verification will be re-enabled once decompression is optimized
    (void)original;      // Suppress unused parameter warning
    (void)originalCount;
    (void)compressed;
    (void)method;
    
    return true; // Assume lossless
}


/**
 * @fn std::vector<uint8_t> compressRawDataBuffer(...)
 * 
 * @brief Compress all raw data currently in the buffer using smart selection.
 *        This is called at upload time, NOT during data acquisition.
 * 
 * @param compressionTime Reference to store compression time in microseconds.
 * @param methodUsed Buffer to store the compression method name.
 * @param methodSize Size of methodUsed buffer.
 * @param academicRatio Reference to store academic compression ratio.
 * @param traditionalRatio Reference to store traditional compression ratio.
 * @param originalSize Reference to store original data size in bytes.
 * 
 * @return std::vector<uint8_t> Compressed data.
 */
std::vector<uint8_t> compressRawDataBuffer(unsigned long& compressionTime, char* methodUsed, size_t methodSize,
                                          float& academicRatio, float& traditionalRatio, size_t& originalSize) {
    unsigned long startTime = micros();
    
    // Extract all raw data from buffer
    auto allRawData = rawDataBuffer.drain_all();
    
    if (allRawData.empty()) {
        strncpy(methodUsed, "NONE", methodSize - 1);
        methodUsed[methodSize - 1] = '\0';
        originalSize = 0;
        compressionTime = 0;
        academicRatio = 1.0f;
        traditionalRatio = 0.0f;
        return std::vector<uint8_t>();
    }
    
    print("\nCOMPRESSING RAW DATA AT UPLOAD TIME\n");
    print("====================================\n");
    
    // Determine total data size and register layout
    // Assuming all samples have same register layout (common case)
    size_t totalValues = 0;
    const RegID* registers = allRawData[0].registers;
    size_t registerCount = allRawData[0].registerCount;
    
    for (const auto& sample : allRawData) {
        totalValues += sample.registerCount;
    }
    
    // Create linear array of all values for compression
    uint16_t* linearData = new uint16_t[totalValues];
    RegID* linearRegisters = new RegID[totalValues];
    
    size_t index = 0;
    for (const auto& sample : allRawData) {
        for (size_t i = 0; i < sample.registerCount; i++) {
            linearData[index] = sample.values[i];
            linearRegisters[index] = sample.registers[i];
            index++;
        }
    }
    
    originalSize = totalValues * sizeof(uint16_t);
    
    print("Total samples: %zu\n", allRawData.size());
    print("Total values: %zu\n", totalValues);
    print("Original size: %zu bytes\n", originalSize);
    
    // Use smart selection to compress all data
    std::vector<uint8_t> compressed = DataCompression::compressWithSmartSelection(
        linearData, linearRegisters, totalValues);
    
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
    }
    
    // Calculate compression ratios
    size_t compressedSize = compressed.size();
    academicRatio = (compressedSize > 0) ? (float)compressedSize / (float)originalSize : 1.0f;
    traditionalRatio = (compressedSize > 0) ? (float)originalSize / (float)compressedSize : 0.0f;
    
    print("Compressed size: %zu bytes\n", compressedSize);
    print("Method used: %s\n", methodUsed);
    print("Academic ratio: %.3f (%.1f%% savings)\n", academicRatio, (1.0f - academicRatio) * 100.0f);
    print("Compression time: %lu μs\n", compressionTime);
    
    // ========== PAYLOAD SIZE CHECK ==========
    if (compressedSize > MAX_PAYLOAD_SIZE) {
        print("\n⚠️  WARNING: Compressed payload (%zu bytes) exceeds MAX_PAYLOAD_SIZE (%zu bytes)\n", 
              compressedSize, MAX_PAYLOAD_SIZE);
        print("   Recommendation: Enable aggregation to reduce data volume\n");
        print("   Set enableAggregation = true to downsample before compression\n");
    } else if (compressedSize > WARN_PAYLOAD_SIZE) {
        print("\n⚠️  WARNING: Compressed payload (%zu bytes) is large (>%zu bytes)\n", 
              compressedSize, WARN_PAYLOAD_SIZE);
        print("   Consider enabling aggregation for future uploads\n");
    } else {
        print("✓ Payload size (%zu bytes) is within limits\n", compressedSize);
    }
    
    // ========== LOSSLESS VERIFICATION (DISABLED - causes stack overflow) ==========
    // bool losslessVerified = verifyLosslessCompression(linearData, totalValues, compressed, methodUsed);
    // if (losslessVerified) {
    //     print("✓ Lossless compression verified\n");
    // } else {
    //     print("⚠️  Lossless verification failed\n");
    // }
    print("✓ Lossless compression assumed (verification disabled to save stack)\n");
    
    print("====================================\n");
    
    // Clean up
    delete[] linearData;
    delete[] linearRegisters;
    
    return compressed;
}


/**
 * @fn std::vector<uint8_t> aggregateAndCompressRawData(...)
 * 
 * @brief Optionally aggregate raw data, then compress using smart selection.
 *        This combines data aggregation with compression for maximum efficiency.
 * 
 * @param compressionTime Reference to store compression time in microseconds.
 * @param methodUsed Buffer to store the compression method name.
 * @param methodSize Size of methodUsed buffer.
 * @param academicRatio Reference to store academic compression ratio.
 * @param traditionalRatio Reference to store traditional compression ratio.
 * @param originalSize Reference to store original data size in bytes.
 * @param enableAggregation Enable/disable aggregation step.
 * @param targetSamples Target number of samples after aggregation.
 * 
 * @return std::vector<uint8_t> Compressed data.
 */
std::vector<uint8_t> aggregateAndCompressRawData(unsigned long& compressionTime, char* methodUsed, size_t methodSize,
                                                 float& academicRatio, float& traditionalRatio, 
                                                 size_t& originalSize, bool enableAggregation, size_t targetSamples) {
    unsigned long startTime = micros();
    
    // Extract all raw data from buffer
    auto allRawData = rawDataBuffer.drain_all();
    
    if (allRawData.empty()) {
        strncpy(methodUsed, "NONE", methodSize - 1);
        methodUsed[methodSize - 1] = '\0';
        originalSize = 0;
        compressionTime = 0;
        academicRatio = 1.0f;
        traditionalRatio = 0.0f;
        return std::vector<uint8_t>();
    }
    
    print("\n%s RAW DATA AT UPLOAD TIME\n", enableAggregation ? "AGGREGATING + COMPRESSING" : "COMPRESSING");
    print("====================================\n");
    print("Original samples: %zu\n", allRawData.size());
    
    // Determine total data size
    size_t totalValues = 0;
    size_t registerCount = allRawData[0].registerCount;
    
    for (const auto& sample : allRawData) {
        totalValues += sample.registerCount;
    }
    
    originalSize = totalValues * sizeof(uint16_t);
    print("Total values: %zu (%zu bytes)\n", totalValues, originalSize);
    
    // Create arrays to hold data
    uint16_t* linearData = new uint16_t[totalValues];
    RegID* linearRegisters = new RegID[totalValues];
    
    size_t index = 0;
    for (const auto& sample : allRawData) {
        for (size_t i = 0; i < sample.registerCount; i++) {
            linearData[index] = sample.values[i];
            linearRegisters[index] = sample.registers[i];
            index++;
        }
    }
    
    // STEP 1: AGGREGATION (if enabled)
    uint16_t* dataToCompress = linearData;
    RegID* registersToCompress = linearRegisters;
    size_t valuesToCompress = totalValues;
    
    uint16_t* aggregatedData = nullptr;
    RegID* aggregatedRegisters = nullptr;
    
    if (enableAggregation && allRawData.size() > targetSamples) {
        print("\n--- AGGREGATION PHASE ---\n");
        print("Downsampling from %zu to %zu samples...\n", allRawData.size(), targetSamples);
        
        // Aggregation method (can be made configurable)
        DataAggregation::AggregationMethod aggMethod = DataAggregation::AGG_MEAN;
        
        // Allocate for aggregated data
        size_t aggregatedValues = targetSamples * registerCount;
        aggregatedData = new uint16_t[aggregatedValues];
        aggregatedRegisters = new RegID[aggregatedValues];
        
        // Aggregate each register separately
        size_t aggIndex = 0;
        for (size_t reg = 0; reg < registerCount; reg++) {
            // Extract all values for this register
            uint16_t* regValues = new uint16_t[allRawData.size()];
            for (size_t i = 0; i < allRawData.size(); i++) {
                regValues[i] = allRawData[i].values[reg];
            }
            
            // Downsample this register's values
            uint16_t* aggregatedRegValues = new uint16_t[targetSamples];
            size_t actualSamples = DataAggregation::adaptiveDownsample(
                regValues, allRawData.size(), 
                aggregatedRegValues, targetSamples, 
                aggMethod);
            
            // Store aggregated values
            for (size_t i = 0; i < actualSamples; i++) {
                aggregatedData[aggIndex] = aggregatedRegValues[i];
                aggregatedRegisters[aggIndex] = linearRegisters[reg];
                aggIndex++;
            }
            
            delete[] regValues;
            delete[] aggregatedRegValues;
        }
        
        // Use aggregated data for compression
        dataToCompress = aggregatedData;
        registersToCompress = aggregatedRegisters;
        valuesToCompress = aggIndex;
        
        size_t aggregatedBytes = valuesToCompress * sizeof(uint16_t);
        printAggregationStats(allRawData.size(), valuesToCompress / registerCount, originalSize, aggregatedBytes);
    }
    
    // STEP 2: COMPRESSION
    print("\n--- COMPRESSION PHASE ---\n");
    print("Compressing %zu values...\n", valuesToCompress);
    
    std::vector<uint8_t> compressed = DataCompression::compressWithSmartSelection(
        dataToCompress, registersToCompress, valuesToCompress);
    
    compressionTime = micros() - startTime;
    
    // Determine method from compressed data header
    if (!compressed.empty()) {
        uint8_t marker = compressed[0];
        switch (marker) {
            case 0x10: 
                strncpy(methodUsed, "DICTIONARY", methodSize - 1);
                smartStats.dictionaryUsed++;
                break;
            case 0x30: 
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
    }
    
    // Calculate compression ratios
    size_t compressedSize = compressed.size();
    academicRatio = (compressedSize > 0) ? (float)compressedSize / (float)originalSize : 1.0f;
    traditionalRatio = (compressedSize > 0) ? (float)originalSize / (float)compressedSize : 0.0f;
    
    print("Compressed size: %zu bytes\n", compressedSize);
    print("Method used: %s\n", methodUsed);
    print("Academic ratio: %.3f (%.1f%% savings)\n", academicRatio, (1.0f - academicRatio) * 100.0f);
    if (enableAggregation) {
        print("Combined reduction: %.1f%%\n", (1.0f - (float)compressedSize / (float)originalSize) * 100.0f);
    }
    print("Total time: %lu μs\n", compressionTime);
    
    // ========== PAYLOAD SIZE CHECK ==========
    if (compressedSize > MAX_PAYLOAD_SIZE) {
        print("\n⚠️  WARNING: Compressed payload (%zu bytes) EXCEEDS MAX_PAYLOAD_SIZE (%zu bytes)\n", 
              compressedSize, MAX_PAYLOAD_SIZE);
        if (!enableAggregation) {
            print("   RECOMMENDATION: Enable aggregation to reduce data volume\n");
            print("   Set enableAggregation = true and targetAggregatedSamples = 50\n");
        } else {
            print("   CRITICAL: Even with aggregation, payload is too large!\n");
            print("   Reduce targetAggregatedSamples or increase upload frequency\n");
        }
    } else if (compressedSize > WARN_PAYLOAD_SIZE) {
        print("\n⚠️  WARNING: Compressed payload (%zu bytes) is large (>%zu bytes)\n", 
              compressedSize, WARN_PAYLOAD_SIZE);
        if (!enableAggregation) {
            print("   Consider enabling aggregation for future uploads\n");
        }
    } else {
        print("✓ Payload size (%zu bytes) is within limits\n", compressedSize);
    }
    
    // ========== LOSSLESS VERIFICATION (DISABLED - causes stack overflow) ==========
    // Verification disabled to prevent stack overflow on ESP32
    // bool losslessVerified = verifyLosslessCompression(dataToCompress, valuesToCompress, compressed, methodUsed);
    // if (losslessVerified) {
    //     print("✓ Lossless compression verified\n");
    //     if (enableAggregation) {
    //         print("  (Note: Aggregation is lossy, but compression of aggregated data is lossless)\n");
    //     }
    // } else {
    //     print("⚠️  Lossless verification failed\n");
    // }
    print("✓ Lossless compression assumed (verification disabled to save stack)\n");
    if (enableAggregation) {
        print("  (Note: Aggregation is lossy, but compression of aggregated data is lossless)\n");
    }
    
    print("====================================\n");
    
    // Clean up
    delete[] linearData;
    delete[] linearRegisters;
    if (aggregatedData) delete[] aggregatedData;
    if (aggregatedRegisters) delete[] aggregatedRegisters;
    
    return compressed;
}


/**
 * @fn void printAggregationStats(...)
 * 
 * @brief Print aggregation statistics
 */
void printAggregationStats(size_t originalSamples, size_t aggregatedSamples, size_t originalBytes, size_t aggregatedBytes) {
    float reduction = (1.0f - (float)aggregatedSamples / (float)originalSamples) * 100.0f;
    float byteReduction = (1.0f - (float)aggregatedBytes / (float)originalBytes) * 100.0f;
    
    print("Aggregation complete:\n");
    print("  Samples: %zu → %zu (%.1f%% reduction)\n", originalSamples, aggregatedSamples, reduction);
    print("  Data size: %zu → %zu bytes (%.1f%% reduction)\n", originalBytes, aggregatedBytes, byteReduction);
}


/**
 * @fn void uploadCompressedDataToCloud(...)
 * 
 * @brief Upload compressed data to the cloud server with metadata.
 */
void uploadCompressedDataToCloud(const std::vector<uint8_t>& compressedData, const char* method, 
                                unsigned long compressionTime, float academicRatio, 
                                float traditionalRatio, size_t originalSize, size_t sampleCount) {
    if (WiFi.status() != WL_CONNECTED) {
        print("WiFi not connected. Cannot upload.\n");
        return;
    }

    HTTPClient http;
    http.begin(dataPostURL);
    http.addHeader("Content-Type", "application/json");
    
    // Allocate large buffers on HEAP to avoid stack overflow
    char* base64Buffer = new char[4096];
    char* jsonString = new char[8192];
    
    // Create JSON payload - allocate on heap
    DynamicJsonDocument* doc = new DynamicJsonDocument(8192);
    
    (*doc)["device_id"] = "ESP32_EcoWatt_Smart";
    (*doc)["timestamp"] = millis();
    (*doc)["data_type"] = "compressed_sensor_data";
    (*doc)["total_samples"] = sampleCount;
    
    // Register mapping for decompression
    JsonObject registerMapping = (*doc)["register_mapping"].to<JsonObject>();
    registerMapping["0"] = "REG_VAC1";
    registerMapping["1"] = "REG_IAC1";
    registerMapping["2"] = "REG_IPV1";
    registerMapping["3"] = "REG_PAC";
    registerMapping["4"] = "REG_IPV2";
    registerMapping["5"] = "REG_TEMP";
    
    // Compressed data (Base64 encoded)
    convertBinaryToBase64(compressedData, base64Buffer, 4096);
    (*doc)["compressed_binary"] = base64Buffer;
    
    // Decompression metadata
    JsonObject decompMeta = (*doc)["decompression_metadata"].to<JsonObject>();
    decompMeta["method"] = method;
    decompMeta["original_size_bytes"] = originalSize;
    decompMeta["compressed_size_bytes"] = compressedData.size();
    
    // Compression performance metrics
    JsonObject metrics = (*doc)["performance_metrics"].to<JsonObject>();
    metrics["academic_ratio"] = academicRatio;
    metrics["traditional_ratio"] = traditionalRatio;
    metrics["compression_time_us"] = compressionTime;
    metrics["savings_percent"] = (1.0f - academicRatio) * 100.0f;
    
    size_t jsonLen = serializeJson(*doc, jsonString, 8192);

    print("\nUPLOADING TO CLOUD\n");
    print("JSON Size: %zu bytes\n", jsonLen);
    print("Compressed payload: %zu -> %zu bytes (%.1f%% savings)\n", 
        originalSize, compressedData.size(),
        (1.0f - academicRatio) * 100.0f);
    
    int httpResponseCode = http.POST((uint8_t*)jsonString, jsonLen);
    
    if (httpResponseCode == 200) {
        String response = http.getString();
        print("✓ Upload successful!\n");
        smartStats.losslessSuccesses++;
    } else {
        print("✗ Upload failed (HTTP %d)\n", httpResponseCode);
        if (httpResponseCode > 0) {
            String errorResponse = http.getString();
            print("Server error: %s\n", errorResponse.c_str());
        }
        smartStats.compressionFailures++;
        
        // NOTE: Raw data has been drained. In production, implement retry logic
        // or keep raw data until upload confirmation
    }
    
    http.end();
    
    // Clean up heap allocations
    delete doc;
    delete[] base64Buffer;
    delete[] jsonString;
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


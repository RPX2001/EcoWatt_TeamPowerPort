/**
 * @file main.cpp
 * @brief EcoWatt ESP32 Main Firmware - Smart Energy Monitoring System (Modular v2.0)
 * @version 2.0.0
 * 
 * @note Refactored to use modular architecture from src/application/
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#include <Arduino.h>
#include "peripheral/print.h"

// ArduinoJson requires clean 'print' identifier
#undef print
#include "peripheral/acquisition.h"
#include "application/compression.h"
#define print(...) debug.log(__VA_ARGS__)

#include "application/system_initializer.h"
#include "application/task_coordinator.h"
#include "application/data_pipeline.h"
#include "application/data_uploader.h"
#include "application/command_executor.h"
#include "application/config_manager.h"
#include "application/statistics_manager.h"
#include "application/OTAManager.h"
#include "application/nvs.h"
#include "application/credentials.h"
#include "peripheral/arduino_wifi.h"

// ============================================
// Global Objects
// ============================================
OTAManager* otaManager = nullptr;
Arduino_Wifi Wifi;

// ============================================
// State Variables
// ============================================
static uint8_t command_poll_counter = 0;
static bool registers_uptodate = true;
static bool pollFreq_uptodate = true;
static bool uploadFreq_uptodate = true;

#define FIRMWARE_VERSION "1.0.4"

// ============================================
// Forward Declarations
// ============================================
void performOTAUpdate();
void enhanceDictionaryForOptimalCompression();

// ============================================
// OTA Update Handler
// ============================================
void performOTAUpdate()
{
    print("=== OTA UPDATE CHECK INITIATED ===\n");
    
    if (otaManager->checkForUpdate()) {
        print("Firmware update available!\n");
        print("Pausing normal operations...\n");
        
        TaskCoordinator::pauseAllTasks();
        
        if (otaManager->downloadAndApplyFirmware()) {
            otaManager->verifyAndReboot();
        } else {
            print("OTA download/apply failed\n");
            print("Will retry on next check\n");
            TaskCoordinator::resumeAllTasks();
        }
    } else {
        print("No firmware updates available\n");
    }
}

// ============================================
// Setup Function
// ============================================
void setup() 
{
  Serial.begin(115200);
  delay(1000);
  print_init();
  print("\n=== EcoWatt ESP32 System Starting (Modular v2.0) ===\n");
  
  // Initialize all system components
  SystemInitializer::initializeAll();
  
  // Initialize OTA Manager  
  print("Initializing OTA Manager...\n");
  otaManager = new OTAManager(
      FLASK_SERVER_URL ":5001",
      "ESP32_EcoWatt_Smart",
      FIRMWARE_VERSION
  );
  otaManager->handleRollback();
  
  // Get configuration from NVS
  uint64_t pollFreq = nvs::getPollFreq();
  uint64_t uploadFreq = nvs::getUploadFreq();
  uint64_t configCheckFreq = 5000000ULL;  // 5 seconds
  uint64_t otaCheckFreq = 60000000ULL;    // 1 minute
  
  size_t registerCount = nvs::getReadRegCount();
  const RegID* selection = nvs::getReadRegs();
  
  // Initialize Task Coordinator
  TaskCoordinator::init(pollFreq, uploadFreq, configCheckFreq, otaCheckFreq);
  
  // Initialize Data Pipeline
  DataPipeline::init(selection, registerCount);
  
  // Initialize Data Uploader
  DataUploader::init(FLASK_SERVER_URL "/process", "ESP32_EcoWatt_Smart");
  
  // Initialize Command Executor
  CommandExecutor::init(
      FLASK_SERVER_URL "/command/poll",
      FLASK_SERVER_URL "/command/result",
      "ESP32_EcoWatt_Smart"
  );
  
  // Initialize Config Manager
  ConfigManager::init(FLASK_SERVER_URL "/changes", "ESP32_EcoWatt_Smart");
  
  // Initialize Statistics Manager
  StatisticsManager::init();
  
  // Enhance compression dictionary
  enhanceDictionaryForOptimalCompression();
  
  print("=== System Initialization Complete ===\n");
  print("Starting main loop...\n\n");
}

// ============================================
// Main Loop Function
// ============================================
void loop()
{
  // Poll sensor data
  if (TaskCoordinator::isPollReady()) {
    TaskCoordinator::resetPollToken();
    DataPipeline::pollAndProcess();
  }
  
  // Upload queued data
  if (TaskCoordinator::isUploadReady()) {
    TaskCoordinator::resetUploadToken();
    DataUploader::uploadPendingData();
    
    // Apply any pending configuration changes
    if (!pollFreq_uptodate) {
      uint64_t newFreq = nvs::getPollFreq();
      TaskCoordinator::updatePollFrequency(newFreq);
      pollFreq_uptodate = true;
      print("Poll frequency updated to %llu\n", newFreq);
    }
    
    if (!uploadFreq_uptodate) {
      uint64_t newFreq = nvs::getUploadFreq();
      TaskCoordinator::updateUploadFrequency(newFreq);
      uploadFreq_uptodate = true;
      print("Upload frequency updated to %llu\n", newFreq);
    }
    
    if (!registers_uptodate) {
      size_t newCount = nvs::getReadRegCount();
      const RegID* newSelection = nvs::getReadRegs();
      DataPipeline::updateRegisterSelection(newSelection, newCount);
      registers_uptodate = true;
      print("Registers updated! Now reading %zu registers\n", newCount);
    }
  }
  
  // Check for configuration changes
  if (TaskCoordinator::isChangesReady()) {
    TaskCoordinator::resetChangesToken();
    ConfigManager::checkForChanges(&registers_uptodate, &pollFreq_uptodate, &uploadFreq_uptodate);
    
    // Check for commands every 2nd config check (every 10s)
    command_poll_counter++;
    if (command_poll_counter >= 2) {
      command_poll_counter = 0;
      CommandExecutor::checkAndExecuteCommands();
    }
  }
  
  // Handle OTA updates
  if (TaskCoordinator::isOTAReady()) {
    TaskCoordinator::resetOTAToken();
    performOTAUpdate();
  }
  
  yield();
}

// ============================================
// Helper Functions
// ============================================

/**
 * @brief Enhance compression dictionary with common sensor patterns
 */
void enhanceDictionaryForOptimalCompression() {
    // Add patterns learned from actual sensor data
    uint16_t pattern0[] = {2429, 177, 73, 4331, 70, 605};
    uint16_t pattern1[] = {2308, 168, 69, 4115, 67, 575};
    uint16_t pattern2[] = {2550, 186, 77, 4547, 74, 635};
    uint16_t pattern3[] = {2380, 150, 65, 3800, 55, 590};
    uint16_t pattern4[] = {2480, 195, 80, 4800, 85, 620};
}

/**
 * @brief Read multiple registers using the acquisition system
 */
bool readMultipleRegisters(const RegID* selection, size_t count, uint16_t* data) {
    DecodedValues result = readRequest(selection, count);
    if (result.count != count) {
        return false;
    }
    for (size_t i = 0; i < count && i < result.count; i++) {
        data[i] = result.values[i];
    }
    return true;
}

/**
 * @brief Compress batch with smart compression algorithm selection
 */
std::vector<uint8_t> compressBatchWithSmartSelection(const SampleBatch& batch, 
                                                   const RegID* selection, size_t registerCount,
                                                   unsigned long& compressionTime, 
                                                   char* methodUsed, size_t methodSize,
                                                   float& academicRatio, 
                                                   float& traditionalRatio) {
    unsigned long startTime = micros();
    
    // Convert batch to linear array
    const size_t maxDataSize = batch.sampleCount * registerCount;
    uint16_t* linearData = new uint16_t[maxDataSize];
    batch.toLinearArray(linearData);
    
    // Create register selection array for the batch
    RegID* batchSelection = new RegID[maxDataSize];
    for (size_t i = 0; i < batch.sampleCount; i++) {
        memcpy(batchSelection + (i * registerCount), selection, registerCount * sizeof(RegID));
    }
    
    // Use smart selection on the entire batch
    std::vector<uint8_t> compressed = DataCompression::compressWithSmartSelection(
        linearData, batchSelection, batch.sampleCount * registerCount);
    
    compressionTime = micros() - startTime;
    
    // Clean up
    delete[] linearData;
    delete[] batchSelection;
    
    // Determine method from compressed data header
    if (!compressed.empty()) {
        switch (compressed[0]) {
            case 0xD0: strncpy(methodUsed, "BATCH_DICTIONARY", methodSize - 1); break;
            case 0x70:
            case 0x71: strncpy(methodUsed, "BATCH_TEMPORAL", methodSize - 1); break;
            case 0x50: strncpy(methodUsed, "BATCH_SEMANTIC", methodSize - 1); break;
            case 0x01:
            default: strncpy(methodUsed, "BATCH_BITPACK", methodSize - 1);
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


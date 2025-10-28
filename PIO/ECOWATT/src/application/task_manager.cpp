/**
 * @file task_manager.cpp
 * @brief FreeRTOS dual-core task management implementation
 * 
 * @author Team PowerPort
 * @date 2025-10-28
 */

#include "application/task_manager.h"
#include "peripheral/print.h"
#include "peripheral/acquisition.h"
#include "application/compression.h"
#include "application/ringbuffer.h"
#include "application/data_uploader.h"
#include "application/command_executor.h"
#include "application/config_manager.h"
#include "application/statistics_manager.h"
#include "application/OTAManager.h"
#include "application/nvs.h"
#include <esp_task_wdt.h>

// ============================================
// Static Member Initialization
// ============================================

// Task handles
TaskHandle_t TaskManager::sensorPollTask_h = NULL;
TaskHandle_t TaskManager::compressionTask_h = NULL;
TaskHandle_t TaskManager::uploadTask_h = NULL;
TaskHandle_t TaskManager::commandTask_h = NULL;
TaskHandle_t TaskManager::configTask_h = NULL;
TaskHandle_t TaskManager::statisticsTask_h = NULL;
TaskHandle_t TaskManager::otaTask_h = NULL;
TaskHandle_t TaskManager::watchdogTask_h = NULL;

// Queues
QueueHandle_t TaskManager::sensorDataQueue = NULL;
QueueHandle_t TaskManager::compressedDataQueue = NULL;
QueueHandle_t TaskManager::commandQueue = NULL;

// Synchronization
SemaphoreHandle_t TaskManager::nvsAccessMutex = NULL;
SemaphoreHandle_t TaskManager::wifiClientMutex = NULL;
SemaphoreHandle_t TaskManager::dataPipelineMutex = NULL;

// Configuration
uint32_t TaskManager::pollFrequency = 5000;
uint32_t TaskManager::uploadFrequency = 15000;
uint32_t TaskManager::configFrequency = 5000;
uint32_t TaskManager::otaFrequency = 60000;

// Statistics
TaskStats TaskManager::stats_sensorPoll = {0};
TaskStats TaskManager::stats_compression = {0};
TaskStats TaskManager::stats_upload = {0};
TaskStats TaskManager::stats_command = {0};
TaskStats TaskManager::stats_config = {0};
TaskStats TaskManager::stats_statistics = {0};
TaskStats TaskManager::stats_ota = {0};
TaskStats TaskManager::stats_watchdog = {0};

// System state
bool TaskManager::systemInitialized = false;
bool TaskManager::systemSuspended = false;
uint32_t TaskManager::systemStartTime = 0;

// ============================================
// Initialization
// ============================================

bool TaskManager::init(uint32_t pollFreqMs, uint32_t uploadFreqMs, 
                       uint32_t configFreqMs, uint32_t otaFreqMs) {
    print("[TaskManager] Initializing FreeRTOS dual-core system...\n");
    
    // Store configuration
    pollFrequency = pollFreqMs;
    uploadFrequency = uploadFreqMs;
    configFrequency = configFreqMs;
    otaFrequency = otaFreqMs;
    
    // Create queues
    sensorDataQueue = xQueueCreate(QUEUE_SENSOR_DATA_SIZE, sizeof(SensorSample));
    if (!sensorDataQueue) {
        print("[TaskManager] ERROR: Failed to create sensor data queue\n");
        return false;
    }
    
    compressedDataQueue = xQueueCreate(QUEUE_COMPRESSED_DATA_SIZE, sizeof(CompressedPacket));
    if (!compressedDataQueue) {
        print("[TaskManager] ERROR: Failed to create compressed data queue\n");
        return false;
    }
    
    commandQueue = xQueueCreate(QUEUE_COMMAND_SIZE, sizeof(Command));
    if (!commandQueue) {
        print("[TaskManager] ERROR: Failed to create command queue\n");
        return false;
    }
    
    print("[TaskManager] Queues created successfully\n");
    
    // Create mutexes (with priority inheritance)
    nvsAccessMutex = xSemaphoreCreateMutex();
    if (!nvsAccessMutex) {
        print("[TaskManager] ERROR: Failed to create NVS mutex\n");
        return false;
    }
    
    wifiClientMutex = xSemaphoreCreateMutex();
    if (!wifiClientMutex) {
        print("[TaskManager] ERROR: Failed to create WiFi mutex\n");
        return false;
    }
    
    dataPipelineMutex = xSemaphoreCreateMutex();
    if (!dataPipelineMutex) {
        print("[TaskManager] ERROR: Failed to create data pipeline mutex\n");
        return false;
    }
    
    print("[TaskManager] Mutexes created successfully\n");
    
    systemInitialized = true;
    systemStartTime = millis();
    
    print("[TaskManager] Initialization complete\n");
    return true;
}

// ============================================
// Task Management
// ============================================

void TaskManager::startAllTasks(void* otaManager) {
    if (!systemInitialized) {
        print("[TaskManager] ERROR: System not initialized!\n");
        return;
    }
    
    print("[TaskManager] Starting all FreeRTOS tasks...\n");
    
    // ========================================
    // CORE 1 (APP_CPU) - Sensor & Processing
    // ========================================
    
    // CRITICAL: Sensor Polling Task
    xTaskCreatePinnedToCore(
        sensorPollTask,           // Function
        "SensorPoll",             // Name
        STACK_SENSOR_POLL,        // Stack size
        NULL,                     // Parameters
        PRIORITY_SENSOR_POLL,     // Priority (MAX)
        &sensorPollTask_h,        // Handle
        CORE_SENSORS              // Core 1
    );
    print("[TaskManager] Created: SensorPoll (Core 1, Priority 24)\n");
    
    // HIGH: Compression Task
    xTaskCreatePinnedToCore(
        compressionTask,
        "Compression",
        STACK_COMPRESSION,
        NULL,
        PRIORITY_COMPRESSION,
        &compressionTask_h,
        CORE_SENSORS              // Core 1
    );
    print("[TaskManager] Created: Compression (Core 1, Priority 18)\n");
    
    // LOWEST: Watchdog Task
    xTaskCreatePinnedToCore(
        watchdogTask,
        "Watchdog",
        STACK_WATCHDOG,
        NULL,
        PRIORITY_WATCHDOG,
        &watchdogTask_h,
        CORE_SENSORS              // Core 1
    );
    print("[TaskManager] Created: Watchdog (Core 1, Priority 1)\n");
    
    // ========================================
    // CORE 0 (PRO_CPU) - Network Operations
    // ========================================
    
    // HIGH: Upload Task
    xTaskCreatePinnedToCore(
        uploadTask,
        "Upload",
        STACK_UPLOAD,
        NULL,
        PRIORITY_UPLOAD,
        &uploadTask_h,
        CORE_NETWORK              // Core 0
    );
    print("[TaskManager] Created: Upload (Core 0, Priority 20)\n");
    
    // MEDIUM-HIGH: Command Task
    xTaskCreatePinnedToCore(
        commandTask,
        "Commands",
        STACK_COMMANDS,
        NULL,
        PRIORITY_COMMANDS,
        &commandTask_h,
        CORE_NETWORK              // Core 0
    );
    print("[TaskManager] Created: Commands (Core 0, Priority 16)\n");
    
    // MEDIUM: Config Task
    xTaskCreatePinnedToCore(
        configTask,
        "Config",
        STACK_CONFIG,
        NULL,
        PRIORITY_CONFIG,
        &configTask_h,
        CORE_NETWORK              // Core 0
    );
    print("[TaskManager] Created: Config (Core 0, Priority 12)\n");
    
    // LOW: OTA Task
    xTaskCreatePinnedToCore(
        otaTask,
        "OTA",
        STACK_OTA,
        otaManager,               // Pass OTA manager as parameter
        PRIORITY_OTA,
        &otaTask_h,
        CORE_NETWORK              // Core 0
    );
    print("[TaskManager] Created: OTA (Core 0, Priority 5)\n");
    
    print("[TaskManager] All tasks started successfully!\n");
    print("[TaskManager] System is now running in dual-core mode\n");
}

void TaskManager::suspendAllTasks() {
    print("[TaskManager] Suspending all tasks (except OTA)...\n");
    
    if (sensorPollTask_h) vTaskSuspend(sensorPollTask_h);
    if (compressionTask_h) vTaskSuspend(compressionTask_h);
    if (uploadTask_h) vTaskSuspend(uploadTask_h);
    if (commandTask_h) vTaskSuspend(commandTask_h);
    if (configTask_h) vTaskSuspend(configTask_h);
    if (statisticsTask_h) vTaskSuspend(statisticsTask_h);
    // NOTE: Do NOT suspend otaTask_h - it's the one calling this function!
    if (watchdogTask_h) vTaskSuspend(watchdogTask_h);
    
    systemSuspended = true;
    print("[TaskManager] All tasks suspended (OTA still running)\n");
}

void TaskManager::resumeAllTasks() {
    print("[TaskManager] Resuming all tasks...\n");
    
    if (sensorPollTask_h) vTaskResume(sensorPollTask_h);
    if (compressionTask_h) vTaskResume(compressionTask_h);
    if (uploadTask_h) vTaskResume(uploadTask_h);
    if (commandTask_h) vTaskResume(commandTask_h);
    if (configTask_h) vTaskResume(configTask_h);
    if (statisticsTask_h) vTaskResume(statisticsTask_h);
    if (otaTask_h) vTaskResume(otaTask_h);
    if (watchdogTask_h) vTaskResume(watchdogTask_h);
    
    systemSuspended = false;
    print("[TaskManager] All tasks resumed\n");
}

// ============================================
// CRITICAL TASK: Sensor Polling (Core 1)
// ============================================

void TaskManager::sensorPollTask(void* parameter) {
    print("[SensorPoll] Task started on Core %d\n", xPortGetCoreID());
    
    // Register this task with hardware watchdog
    esp_task_wdt_add(NULL);
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(pollFrequency);
    const uint32_t deadlineUs = 2000000;  // 2s deadline (Modbus takes ~1.8s)
    
    // Get register configuration from NVS
    xSemaphoreTake(nvsAccessMutex, portMAX_DELAY);
    size_t registerCount = nvs::getReadRegCount();
    const RegID* registers = nvs::getReadRegs();
    xSemaphoreGive(nvsAccessMutex);
    
    print("[SensorPoll] Monitoring %zu registers\n", registerCount);
    print("[SensorPoll] Poll frequency: %lu ms\n", pollFrequency);
    print("[SensorPoll] Deadline: %lu us\n", deadlineUs);
    
    while (1) {
        // Wait for exact period (NO drift, NO jitter)
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        uint32_t startTime = micros();
        
        // CRITICAL: Read sensors from Modbus
        SensorSample sample;
        sample.registerCount = registerCount;
        memcpy(sample.registers, registers, registerCount * sizeof(RegID));
        
        DecodedValues result = readRequest(registers, registerCount);
        
        if (result.count == registerCount) {
            // Success - copy values
            memcpy(sample.values, result.values, registerCount * sizeof(uint16_t));
            sample.timestamp = millis();
            
            // Send to compression queue (non-blocking to avoid deadline miss)
            if (xQueueSend(sensorDataQueue, &sample, 0) != pdTRUE) {
                print("[SensorPoll] WARNING: Queue full! Sample dropped\n");
                stats_sensorPoll.deadlineMisses++;
            }
        } else {
            print("[SensorPoll] ERROR: Modbus read failed (%zu/%zu regs)\n", 
                  result.count, registerCount);
        }
        
        uint32_t executionTime = micros() - startTime;
        
        // Record statistics
        recordTaskExecution(stats_sensorPoll, executionTime);
        checkDeadline("SensorPoll", executionTime, deadlineUs, stats_sensorPoll);
        
        // Update stack high water mark
        stats_sensorPoll.stackHighWater = uxTaskGetStackHighWaterMark(NULL);
        
        // Feed hardware watchdog
        esp_task_wdt_reset();
    }
}

// ============================================
// Utility Functions
// ============================================

void TaskManager::recordTaskExecution(TaskStats& stats, uint32_t executionTimeUs) {
    stats.executionCount++;
    stats.totalTimeUs += executionTimeUs;
    stats.lastRunTime = millis();
    
    if (executionTimeUs > stats.maxTimeUs) {
        stats.maxTimeUs = executionTimeUs;
    }
}

void TaskManager::checkDeadline(const char* taskName, uint32_t executionTimeUs, 
                               uint32_t deadlineUs, TaskStats& stats) {
    if (executionTimeUs > deadlineUs) {
        print("[%s] DEADLINE MISS! Execution: %lu us, Deadline: %lu us\n",
              taskName, executionTimeUs, deadlineUs);
        stats.deadlineMisses++;
    }
}

bool TaskManager::isSystemHealthy() {
    // System is healthy if NO tasks have deadline misses
    return (stats_sensorPoll.deadlineMisses == 0) &&
           (stats_upload.deadlineMisses == 0) &&
           (stats_compression.deadlineMisses == 0);
}

void TaskManager::printSystemHealth() {
    uint32_t uptime = (millis() - systemStartTime) / 1000;
    
    print("\n========== SYSTEM HEALTH REPORT ==========\n");
    print("Uptime: %lu seconds\n", uptime);
    print("Free Heap: %lu bytes\n", ESP.getFreeHeap());
    print("Minimum Free Heap: %lu bytes\n", ESP.getMinFreeHeap());
    print("\n");
    
    print("TASK: SensorPoll (CRITICAL)\n");
    print("  Executions: %lu\n", stats_sensorPoll.executionCount);
    uint32_t avgTime = (stats_sensorPoll.executionCount > 0) ? 
                       (stats_sensorPoll.totalTimeUs / stats_sensorPoll.executionCount) : 0;
    print("  Avg Time: %lu us\n", avgTime);
    print("  Max Time: %lu us\n", stats_sensorPoll.maxTimeUs);
    print("  Deadline Misses: %lu\n", stats_sensorPoll.deadlineMisses);
    print("  Stack Free: %lu bytes\n", stats_sensorPoll.stackHighWater * sizeof(StackType_t));
    print("\n");
    
    print("==========================================\n\n");
}

// ============================================
// HIGH PRIORITY: Compression Task (Core 1)
// ============================================

void TaskManager::compressionTask(void* parameter) {
    print("[Compression] Task started on Core %d\n", xPortGetCoreID());
    
    // Register this task with hardware watchdog
    esp_task_wdt_add(NULL);
    
    const uint32_t deadlineUs = 2000000;  // 2s deadline
    const size_t batchSize = 3;           // Compress 3 samples together
    
    SensorSample sampleBatch[batchSize];
    size_t batchCount = 0;
    
    print("[Compression] Batch size: %zu samples\n", batchSize);
    print("[Compression] Deadline: %lu us\n", deadlineUs);
    
    while (1) {
        // Wait for sensor sample (blocks until available)
        SensorSample sample;
        if (xQueueReceive(sensorDataQueue, &sample, portMAX_DELAY) == pdTRUE) {
            uint32_t startTime = micros();
            
            // Add to batch
            sampleBatch[batchCount++] = sample;
            
            // When batch is full, compress and send
            if (batchCount >= batchSize) {
                CompressedPacket packet;
                
                // Lock data pipeline during compression
                if (xSemaphoreTake(dataPipelineMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    
                    // Convert samples to linear array for compression
                    size_t totalRegisterCount = batchSize * sampleBatch[0].registerCount;
                    uint16_t* linearData = new uint16_t[totalRegisterCount];
                    RegID* linearRegs = new RegID[totalRegisterCount];
                    
                    for (size_t i = 0; i < batchSize; i++) {
                        memcpy(linearData + (i * sampleBatch[0].registerCount), 
                               sampleBatch[i].values, 
                               sampleBatch[0].registerCount * sizeof(uint16_t));
                        memcpy(linearRegs + (i * sampleBatch[0].registerCount),
                               sampleBatch[i].registers,
                               sampleBatch[0].registerCount * sizeof(RegID));
                    }
                    
                    // Call actual compression API
                    std::vector<uint8_t> compressedVec = DataCompression::compressWithSmartSelection(
                        linearData, linearRegs, totalRegisterCount);
                    
                    delete[] linearData;
                    delete[] linearRegs;
                    
                    // Copy to fixed-size buffer (CRITICAL: No heap allocation in queue!)
                    if (compressedVec.size() <= sizeof(packet.data)) {
                        memcpy(packet.data, compressedVec.data(), compressedVec.size());
                        packet.dataSize = compressedVec.size();
                        packet.timestamp = sampleBatch[batchSize - 1].timestamp;
                        packet.sampleCount = batchSize;
                        packet.uncompressedSize = totalRegisterCount * sizeof(uint16_t);
                        packet.compressedSize = compressedVec.size();
                        
                        // Copy register layout from first sample (all samples have same layout)
                        packet.registerCount = sampleBatch[0].registerCount;
                        memcpy(packet.registers, sampleBatch[0].registers, 
                               sampleBatch[0].registerCount * sizeof(RegID));
                        
                        // Determine compression method from header
                        if (packet.dataSize > 0) {
                            switch (packet.data[0]) {
                                case 0xD0: strncpy(packet.compressionMethod, "dictionary", sizeof(packet.compressionMethod) - 1); break;
                                case 0x70:
                                case 0x71: strncpy(packet.compressionMethod, "temporal", sizeof(packet.compressionMethod) - 1); break;
                                case 0x50: strncpy(packet.compressionMethod, "semantic", sizeof(packet.compressionMethod) - 1); break;
                                default: strncpy(packet.compressionMethod, "bitpack", sizeof(packet.compressionMethod) - 1);
                            }
                        } else {
                            strncpy(packet.compressionMethod, "raw", sizeof(packet.compressionMethod) - 1);
                        }
                    } else {
                        print("[Compression] ERROR: Compressed data too large (%zu > %zu)\n",
                              compressedVec.size(), sizeof(packet.data));
                        stats_compression.deadlineMisses++;
                        batchCount = 0;
                        continue;
                    }
                    
                    xSemaphoreGive(dataPipelineMutex);
                    
                    // Send to upload queue (non-blocking to avoid deadline miss)
                    if (xQueueSend(compressedDataQueue, &packet, 0) != pdTRUE) {
                        print("[Compression] WARNING: Upload queue full! Packet dropped\n");
                        stats_compression.deadlineMisses++;
                    }
                    
                    print("[Compression] Batch compressed: %zu samples -> %zu bytes (ratio: %.2f%%)\n",
                          batchSize, packet.dataSize, 
                          (float)packet.compressedSize / packet.uncompressedSize * 100.0f);
                    
                } else {
                    print("[Compression] ERROR: Failed to acquire pipeline mutex\n");
                }
                
                // Reset batch
                batchCount = 0;
            }
            
            uint32_t executionTime = micros() - startTime;
            
            // Record statistics
            recordTaskExecution(stats_compression, executionTime);
            checkDeadline("Compression", executionTime, deadlineUs, stats_compression);
            
            stats_compression.stackHighWater = uxTaskGetStackHighWaterMark(NULL);
            
            // Feed hardware watchdog
            esp_task_wdt_reset();
        }
    }
}

// ============================================
// HIGH PRIORITY: Upload Task (Core 0)
// ============================================

void TaskManager::uploadTask(void* parameter) {
    print("[Upload] Task started on Core %d\n", xPortGetCoreID());
    
    // Register this task with hardware watchdog
    esp_task_wdt_add(NULL);
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(uploadFrequency);
    const uint32_t deadlineUs = 5000000;  // 5s deadline per upload
    
    print("[Upload] Upload frequency: %lu ms\n", uploadFrequency);
    print("[Upload] Deadline: %lu us\n", deadlineUs);
    
    while (1) {
        // Wait for exact upload interval
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        uint32_t startTime = micros();
        
        // First, add all pending compressed packets to DataUploader queue
        CompressedPacket packet;
        size_t queuedCount = 0;
        
        print("[Upload] Checking compressed data queue...\n");
        
        while (xQueueReceive(compressedDataQueue, &packet, 0) == pdTRUE) {
            // Feed watchdog during queue processing
            yield();
            
            // Convert CompressedPacket to SmartCompressedData
            SmartCompressedData smartData;
            smartData.binaryData.assign(packet.data, packet.data + packet.dataSize);  // Copy fixed buffer to vector
            smartData.timestamp = packet.timestamp;
            smartData.sampleCount = packet.sampleCount;  // Number of samples in this packet
            smartData.registerCount = packet.registerCount;  // Actual number of registers per sample
            smartData.originalSize = packet.uncompressedSize;
            smartData.academicRatio = (float)packet.compressedSize / (float)packet.uncompressedSize;
            smartData.traditionalRatio = (float)packet.uncompressedSize / (float)packet.compressedSize;
            strncpy(smartData.compressionMethod, packet.compressionMethod, sizeof(smartData.compressionMethod) - 1);
            
            // Copy register layout
            memcpy(smartData.registers, packet.registers, packet.registerCount * sizeof(RegID));
            
            if (DataUploader::addToQueue(smartData)) {
                queuedCount++;
            } else {
                print("[Upload] WARNING: Failed to queue packet - buffer full\n");
            }
        }
        
        print("[Upload] Queued %zu packets for upload\n", queuedCount);
        
        // Now upload all pending data (if any)
        bool uploadSuccess = false;
        
        if (queuedCount > 0) {
            print("[Upload] Attempting to acquire WiFi mutex (timeout: 15s)...\n");
            
            // Acquire WiFi client mutex (shared with other network tasks)
            // Increased timeout to 15 seconds for upload stability
            if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(15000)) == pdTRUE) {
                
                print("[Upload] WiFi mutex acquired. Starting upload...\n");
                
                // Call actual DataUploader API
                uploadSuccess = DataUploader::uploadPendingData();
                
                print("[Upload] Upload completed. Releasing WiFi mutex...\n");
                xSemaphoreGive(wifiClientMutex);
                
                if (uploadSuccess) {
                    print("[Upload] Successfully uploaded %zu packets\n", queuedCount);
                } else {
                    print("[Upload] Upload failed for %zu packets\n", queuedCount);
                }
            } else {
                print("[Upload] ERROR: Failed to acquire WiFi mutex within 15s\n");
                stats_upload.deadlineMisses++;
            }
        }
        
        uint32_t executionTime = micros() - startTime;
        
        // Record statistics
        recordTaskExecution(stats_upload, executionTime);
        checkDeadline("Upload", executionTime, deadlineUs, stats_upload);
        
        stats_upload.stackHighWater = uxTaskGetStackHighWaterMark(NULL);
        
        // Feed hardware watchdog (critical for long uploads)
        esp_task_wdt_reset();
    }
}

// ============================================
// MEDIUM-HIGH: Command Task (Core 0)
// ============================================

void TaskManager::commandTask(void* parameter) {
    print("[Commands] Task started on Core %d\n", xPortGetCoreID());
    
    // Register this task with hardware watchdog
    esp_task_wdt_add(NULL);
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10000);  // Check every 10s
    const uint32_t deadlineUs = 1000000;  // 1s deadline
    
    print("[Commands] Check frequency: 10s\n");
    print("[Commands] Deadline: %lu us\n", deadlineUs);
    
    while (1) {
        // Wait for command check interval
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        uint32_t startTime = micros();
        
        // Feed watchdog before network operation
        yield();
        
        // Acquire WiFi mutex for network commands (5s timeout to allow for HTTP timeout)
        if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
            
            // Execute command using actual CommandExecutor API
            CommandExecutor::checkAndExecuteCommands();
            
            xSemaphoreGive(wifiClientMutex);
            
        } else {
            print("[Commands] ERROR: Failed to acquire WiFi mutex\n");
        }
        
        uint32_t executionTime = micros() - startTime;
        
        // Record statistics
        recordTaskExecution(stats_command, executionTime);
        checkDeadline("Commands", executionTime, deadlineUs, stats_command);
        
        stats_command.stackHighWater = uxTaskGetStackHighWaterMark(NULL);
        
        // Feed hardware watchdog (critical - this task blocks on HTTP)
        esp_task_wdt_reset();
    }
}

// ============================================
// MEDIUM: Config Task (Core 0)
// ============================================

void TaskManager::configTask(void* parameter) {
    print("[Config] Task started on Core %d\n", xPortGetCoreID());
    
    // Register this task with hardware watchdog
    esp_task_wdt_add(NULL);
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(configFrequency);
    const uint32_t deadlineUs = 2000000;  // 2s deadline
    
    // State flags for configuration changes
    static bool registers_uptodate = true;
    static bool pollFreq_uptodate = true;
    static bool uploadFreq_uptodate = true;
    
    print("[Config] Check frequency: %lu ms\n", configFrequency);
    print("[Config] Deadline: %lu us\n", deadlineUs);
    
    while (1) {
        // Wait for config check interval
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        uint32_t startTime = micros();
        
        // Acquire WiFi mutex for HTTP request
        if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            
            // Fetch config from server using actual ConfigManager API
            ConfigManager::checkForChanges(&registers_uptodate, &pollFreq_uptodate, &uploadFreq_uptodate);
            
            xSemaphoreGive(wifiClientMutex);
            
            // Apply configuration changes if needed
            if (!registers_uptodate || !pollFreq_uptodate || !uploadFreq_uptodate) {
                print("[Config] Configuration changed! Flags need update.\n");
                
                // Note: Actual application of config changes happens in main system
                // These flags are used by the main coordinator
                // In FreeRTOS version, we could trigger events or update task parameters
            }
            
        } else {
            print("[Config] ERROR: Failed to acquire WiFi mutex\n");
        }
        
        uint32_t executionTime = micros() - startTime;
        
        // Record statistics
        recordTaskExecution(stats_config, executionTime);
        checkDeadline("Config", executionTime, deadlineUs, stats_config);
        
        stats_config.stackHighWater = uxTaskGetStackHighWaterMark(NULL);
        
        // Feed hardware watchdog
        esp_task_wdt_reset();
    }
}

// ============================================
// LOW: OTA Update Task (Core 0)
// ============================================

void TaskManager::otaTask(void* parameter) {
    print("[OTA] Task started on Core %d\n", xPortGetCoreID());
    
    // NOTE: OTA task NOT registered with watchdog - it runs every 60s
    // which exceeds the 30s watchdog timeout. OTA is low priority and infrequent.
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(otaFrequency);
    const uint32_t deadlineUs = 120000000;  // 120s deadline (when active)
    
    // Store OTA manager instance (passed via parameter)
    OTAManager* otaManager = static_cast<OTAManager*>(parameter);
    
    print("[OTA] Check frequency: %lu ms\n", otaFrequency);
    print("[OTA] Deadline: %lu us\n", deadlineUs);
    
    while (1) {
        // Wait for OTA check interval
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        uint32_t startTime = micros();
        
        // Acquire WiFi mutex for network access
        if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
            
            // Check for firmware updates using actual OTAManager API
            if (otaManager && otaManager->checkForUpdate()) {
                print("[OTA] Firmware update available! Starting download...\n");
                
                // KEEP the WiFi mutex - don't release it before suspending tasks!
                // This prevents deadlock when re-acquiring after suspend.
                
                // Suspend critical tasks during OTA
                print("[OTA] Suspending critical tasks for update...\n");
                suspendAllTasks();
                
                // Perform OTA update (we already have the mutex)
                bool otaSuccess = otaManager->downloadAndApplyFirmware();
                
                xSemaphoreGive(wifiClientMutex);
                
                if (otaSuccess) {
                    print("[OTA] Update successful! Verifying and rebooting...\n");
                    otaManager->verifyAndReboot();
                    // System will reboot - code won't reach here
                } else {
                    print("[OTA] Update failed! Resuming normal operation...\n");
                    resumeAllTasks();
                }
            } else {
                xSemaphoreGive(wifiClientMutex);
            }
            
        } else {
            print("[OTA] ERROR: Failed to acquire WiFi mutex\n");
        }
        
        uint32_t executionTime = micros() - startTime;
        
        // Record statistics (only when OTA actually runs)
        if (executionTime > 10000) {  // More than 10ms means OTA ran
            recordTaskExecution(stats_ota, executionTime);
            checkDeadline("OTA", executionTime, deadlineUs, stats_ota);
        }
        
        stats_ota.stackHighWater = uxTaskGetStackHighWaterMark(NULL);
        
        // NOTE: No watchdog reset - OTA task not registered with watchdog
    }
}

// ============================================
// LOWEST: Watchdog Task (Core 1)
// ============================================

void TaskManager::watchdogTask(void* parameter) {
    print("[Watchdog] Task started on Core %d\n", xPortGetCoreID());
    
    // Register this task with hardware watchdog
    esp_task_wdt_add(NULL);
    
    const TickType_t xCheckInterval = pdMS_TO_TICKS(30000);  // 30s interval
    const uint32_t maxTaskIdleTime = 60000;  // 60s max idle
    
    print("[Watchdog] Check interval: 30s\n");
    print("[Watchdog] Max task idle time: %lu ms\n", maxTaskIdleTime);
    
    while (1) {
        vTaskDelay(xCheckInterval);
        
        uint32_t currentTime = millis();
        uint32_t startTime = micros();
        
        // Check sensor poll task (CRITICAL)
        if (currentTime - stats_sensorPoll.lastRunTime > maxTaskIdleTime) {
            print("[Watchdog] CRITICAL: SensorPoll task stalled! Last run: %lu ms ago\n",
                  currentTime - stats_sensorPoll.lastRunTime);
            print("[Watchdog] SYSTEM RESET TRIGGERED!\n");
            vTaskDelay(pdMS_TO_TICKS(1000));  // Give time for log
            ESP.restart();
        }
        
        // Check upload task (HIGH)
        if (currentTime - stats_upload.lastRunTime > uploadFrequency * 3) {
            print("[Watchdog] WARNING: Upload task delayed! Last run: %lu ms ago\n",
                  currentTime - stats_upload.lastRunTime);
        }
        
        // Check compression task (HIGH)
        if (currentTime - stats_compression.lastRunTime > pollFrequency * 10) {
            print("[Watchdog] WARNING: Compression task delayed! Last run: %lu ms ago\n",
                  currentTime - stats_compression.lastRunTime);
        }
        
        // Check for excessive deadline misses
        if (stats_sensorPoll.deadlineMisses > 10) {
            print("[Watchdog] CRITICAL: Excessive sensor deadline misses (%lu)! Resetting...\n",
                  stats_sensorPoll.deadlineMisses);
            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP.restart();
        }
        
        // Print health report periodically (every 5 minutes)
        static uint32_t lastHealthReport = 0;
        if (currentTime - lastHealthReport > 300000) {
            printSystemHealth();
            lastHealthReport = currentTime;
        }
        
        uint32_t executionTime = micros() - startTime;
        recordTaskExecution(stats_watchdog, executionTime);
        stats_watchdog.stackHighWater = uxTaskGetStackHighWaterMark(NULL);
        
        // Feed hardware watchdog
        esp_task_wdt_reset();
    }
}

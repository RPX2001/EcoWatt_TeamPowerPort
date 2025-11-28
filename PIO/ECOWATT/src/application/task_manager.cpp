/**
 * @file task_manager.cpp
 * @brief FreeRTOS dual-core task management implementation
 * 
 * @author Team PowerPort
 * @date 2025-10-28
 */

#include "application/task_manager.h"
#include "peripheral/logger.h"
#include "application/system_config.h"  // Centralized configuration constants
#include "application/credentials.h"    // DEVICE_ID and FLASK_SERVER_URL
#include "peripheral/logger.h"
#include "peripheral/acquisition.h"
#include "application/compression.h"
#include "application/ringbuffer.h"
#include "application/data_uploader.h"
#include "application/command_executor.h"
#include "application/config_manager.h"
#include "application/statistics_manager.h"
#include "application/OTAManager.h"
#include "application/power_management.h"
#include "application/nvs.h"
#include <esp_task_wdt.h>
#include <time.h>
#include <HTTPClient.h>
#include <WiFi.h>

// Helper function to get current Unix timestamp in milliseconds
static unsigned long long getCurrentTimestampMs() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        // Convert tm struct to Unix timestamp in MILLISECONDS
        time_t now = mktime(&timeinfo);
        // Cast to 64-bit BEFORE multiplying to avoid overflow
        return (unsigned long long)now * 1000ULL;
    }
    // Fallback to millis if NTP not synced
    return (unsigned long long)millis();
}

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
TaskHandle_t TaskManager::powerReportTask_h = NULL;
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
SemaphoreHandle_t TaskManager::batchReadySemaphore = NULL;
SemaphoreHandle_t TaskManager::configReloadSemaphore = NULL;

// Upload frequency reload flag (separate from semaphore since upload task gives the semaphore)
static volatile bool uploadFrequencyChanged = false;

// Configuration
uint32_t TaskManager::pollFrequency = DEFAULT_POLL_FREQUENCY_US / 1000;        // Convert to ms
uint32_t TaskManager::uploadFrequency = DEFAULT_UPLOAD_FREQUENCY_US / 1000;    // Convert to ms
uint32_t TaskManager::configFrequency = DEFAULT_CONFIG_FREQUENCY_US / 1000;    // Convert to ms
uint32_t TaskManager::commandFrequency = DEFAULT_COMMAND_FREQUENCY_US / 1000;  // Convert to ms
uint32_t TaskManager::otaFrequency = DEFAULT_OTA_FREQUENCY_US / 1000;          // Convert to ms
uint32_t TaskManager::powerReportFrequency = 300000;  // 5 minutes default

// Statistics
TaskStats TaskManager::stats_sensorPoll = {0};
TaskStats TaskManager::stats_compression = {0};
TaskStats TaskManager::stats_upload = {0};
TaskStats TaskManager::stats_command = {0};
TaskStats TaskManager::stats_config = {0};
TaskStats TaskManager::stats_statistics = {0};
TaskStats TaskManager::stats_powerReport = {0};
TaskStats TaskManager::stats_ota = {0};
TaskStats TaskManager::stats_watchdog = {0};

// Deadline Monitors
DeadlineMonitor TaskManager::deadlineMonitor_sensorPoll;
DeadlineMonitor TaskManager::deadlineMonitor_upload;
DeadlineMonitor TaskManager::deadlineMonitor_compression;

// System state
bool TaskManager::systemInitialized = false;
bool TaskManager::systemSuspended = false;
uint32_t TaskManager::systemStartTime = 0;

// ============================================
// Initialization
// ============================================

bool TaskManager::init(uint32_t pollFreqMs, uint32_t uploadFreqMs, 
                       uint32_t configFreqMs, uint32_t commandFreqMs, uint32_t otaFreqMs) {
    LOG_SECTION("Initializing FreeRTOS dual-core system");
    
    // Store configuration
    pollFrequency = pollFreqMs;
    uploadFrequency = uploadFreqMs;
    configFrequency = configFreqMs;
    commandFrequency = commandFreqMs;
    otaFrequency = otaFreqMs;
    
    // Create queues
    sensorDataQueue = xQueueCreate(QUEUE_SENSOR_DATA_SIZE, sizeof(SensorSample));
    if (!sensorDataQueue) {
        LOG_ERROR(LOG_TAG_BOOT, "Failed to create sensor data queue");
        return false;
    }
    
    compressedDataQueue = xQueueCreate(QUEUE_COMPRESSED_DATA_SIZE, sizeof(CompressedPacket));
    if (!compressedDataQueue) {
        LOG_ERROR(LOG_TAG_BOOT, "Failed to create compressed data queue");
        return false;
    }
    
    commandQueue = xQueueCreate(QUEUE_COMMAND_SIZE, sizeof(Command));
    if (!commandQueue) {
        LOG_ERROR(LOG_TAG_BOOT, "Failed to create command queue");
        return false;
    }
    
    LOG_SUCCESS(LOG_TAG_BOOT, "Queues created successfully");
    
    // Create mutexes (with priority inheritance)
    nvsAccessMutex = xSemaphoreCreateMutex();
    if (!nvsAccessMutex) {
        LOG_ERROR(LOG_TAG_BOOT, "Failed to create NVS mutex");
        return false;
    }
    
    wifiClientMutex = xSemaphoreCreateMutex();
    if (!wifiClientMutex) {
        LOG_ERROR(LOG_TAG_BOOT, "Failed to create WiFi mutex");
        return false;
    }
    
    dataPipelineMutex = xSemaphoreCreateMutex();
    if (!dataPipelineMutex) {
        LOG_ERROR(LOG_TAG_BOOT, "Failed to create data pipeline mutex");
        return false;
    }
    
    // Create binary semaphore for batch-ready signaling (starts empty)
    batchReadySemaphore = xSemaphoreCreateBinary();
    if (!batchReadySemaphore) {
        LOG_ERROR(LOG_TAG_BOOT, "Failed to create batch ready semaphore");
        return false;
    }
    
    // Create counting semaphore for config reload signaling (max count = 10, starts at 0)
    // This allows upload task to signal all tasks (up to 10) to reload config
    configReloadSemaphore = xSemaphoreCreateCounting(10, 0);
    if (!configReloadSemaphore) {
        LOG_ERROR(LOG_TAG_BOOT, "Failed to create config reload semaphore");
        return false;
    }
    
    LOG_SUCCESS(LOG_TAG_BOOT, "Mutexes and semaphores created successfully");
    
    systemInitialized = true;
    systemStartTime = millis();
    
    LOG_SUCCESS(LOG_TAG_BOOT, "TaskManager initialization complete");
    return true;
}

// ============================================
// Task Management
// ============================================

void TaskManager::startAllTasks(void* otaManager) {
    if (!systemInitialized) {
        LOG_ERROR(LOG_TAG_BOOT, "System not initialized!");
        return;
    }
    
    LOG_INFO(LOG_TAG_BOOT, "Starting all FreeRTOS tasks...");
    
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
    LOG_SUCCESS(LOG_TAG_BOOT, "Created: SensorPoll (Core 1, Priority 24)");
    
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
    LOG_SUCCESS(LOG_TAG_BOOT, "Created: Compression (Core 1, Priority 18)");
    
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
    LOG_SUCCESS(LOG_TAG_BOOT, "Created: Watchdog (Core 1, Priority 1)");
    
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
    LOG_SUCCESS(LOG_TAG_BOOT, "Created: Upload (Core 0, Priority 20)");
    
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
    LOG_SUCCESS(LOG_TAG_BOOT, "Created: Commands (Core 0, Priority 16)");
    
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
    LOG_SUCCESS(LOG_TAG_BOOT, "Created: Config (Core 0, Priority 12)");

    // MEDIUM-LOW: Power Report Task
    xTaskCreatePinnedToCore(
        powerReportTask,
        "PowerReport",
        STACK_POWER_REPORT,
        NULL,
        PRIORITY_POWER_REPORT,
        &powerReportTask_h,
        CORE_NETWORK              // Core 0
    );
    LOG_SUCCESS(LOG_TAG_BOOT, "Created: PowerReport (Core 0, Priority 8)");
    
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
    LOG_SUCCESS(LOG_TAG_BOOT, "Created: OTA (Core 0, Priority 5)");
    
    LOG_SUCCESS(LOG_TAG_BOOT, "All tasks started successfully!");
    LOG_SUCCESS(LOG_TAG_BOOT, "System is now running in dual-core mode");
}

void TaskManager::suspendAllTasks() {
    LOG_INFO(LOG_TAG_BOOT, "Suspending all tasks (except OTA)...");
    
    if (sensorPollTask_h) vTaskSuspend(sensorPollTask_h);
    if (compressionTask_h) vTaskSuspend(compressionTask_h);
    if (uploadTask_h) vTaskSuspend(uploadTask_h);
    if (commandTask_h) vTaskSuspend(commandTask_h);
    if (configTask_h) vTaskSuspend(configTask_h);
    if (statisticsTask_h) vTaskSuspend(statisticsTask_h);
    // NOTE: Do NOT suspend otaTask_h - it's the one calling this function!
    if (watchdogTask_h) vTaskSuspend(watchdogTask_h);
    
    systemSuspended = true;
    LOG_INFO(LOG_TAG_BOOT, "All tasks suspended (OTA still running)");
}

void TaskManager::resumeAllTasks() {
    LOG_INFO(LOG_TAG_BOOT, "Resuming all tasks...");
    
    if (sensorPollTask_h) vTaskResume(sensorPollTask_h);
    if (compressionTask_h) vTaskResume(compressionTask_h);
    if (uploadTask_h) vTaskResume(uploadTask_h);
    if (commandTask_h) vTaskResume(commandTask_h);
    if (configTask_h) vTaskResume(configTask_h);
    if (statisticsTask_h) vTaskResume(statisticsTask_h);
    if (otaTask_h) vTaskResume(otaTask_h);
    if (watchdogTask_h) vTaskResume(watchdogTask_h);
    
    systemSuspended = false;
    LOG_INFO(LOG_TAG_BOOT, "All tasks resumed");
}

// ============================================
// CRITICAL TASK: Sensor Polling (Core 1)
// ============================================

void TaskManager::sensorPollTask(void* parameter) {
    LOG_INFO(LOG_TAG_DATA, "SensorPoll task started on Core %d", xPortGetCoreID());
    
    // Register this task with hardware watchdog
    esp_task_wdt_add(NULL);
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t xFrequency = pdMS_TO_TICKS(pollFrequency);  // NOT const - can update
    const uint32_t deadlineUs = SENSOR_POLL_DEADLINE_US;  // 2s deadline (Modbus takes ~1.8s)
    
    // Get initial register configuration from NVS
    xSemaphoreTake(nvsAccessMutex, portMAX_DELAY);
    size_t registerCount = nvs::getReadRegCount();
    const RegID* registers = nvs::getReadRegs();
    xSemaphoreGive(nvsAccessMutex);
    
    LOG_INFO(LOG_TAG_DATA, "Monitoring %zu registers", registerCount);
    LOG_INFO(LOG_TAG_DATA, "Poll frequency: %lu ms", pollFrequency);
    LOG_INFO(LOG_TAG_DATA, "Deadline: %lu us", deadlineUs);
    
    while (1) {
        // ALWAYS wait for the full interval before starting next cycle
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Check if configuration reload is needed (signaled by upload task after successful upload)
        if (xSemaphoreTake(configReloadSemaphore, 0) == pdTRUE) {
            LOG_INFO(LOG_TAG_DATA, "Config reload signal received - updating configuration");
            
            // Reload poll frequency from static variable (updated by ConfigManager)
            TickType_t newFrequency = pdMS_TO_TICKS(pollFrequency);
            if (newFrequency != xFrequency) {
                xFrequency = newFrequency;
                xLastWakeTime = xTaskGetTickCount();  // Reset timing baseline
                LOG_INFO(LOG_TAG_DATA, "Poll frequency updated to %lu ms", pollFrequency);
            }
            
            // Reload register configuration from NVS (register list is stored in NVS)
            xSemaphoreTake(nvsAccessMutex, portMAX_DELAY);
            size_t newRegisterCount = nvs::getReadRegCount();
            const RegID* newRegisters = nvs::getReadRegs();
            
            // Check if configuration changed
            if (newRegisterCount != registerCount || 
                memcmp(registers, newRegisters, registerCount * sizeof(RegID)) != 0) {
                
                registerCount = newRegisterCount;
                registers = newRegisters;
                
                LOG_INFO(LOG_TAG_DATA, "Register configuration updated - now monitoring %zu registers", registerCount);
            }
            xSemaphoreGive(nvsAccessMutex);
            // Semaphore consumed - do not give back (counting semaphore will be given 6 times by upload task)
        }
        
        uint32_t startTime = micros();
        
        // CRITICAL: Read sensors from Modbus
        SensorSample sample;
        sample.registerCount = registerCount;
        memcpy(sample.registers, registers, registerCount * sizeof(RegID));
        
        DecodedValues result = readRequest(registers, registerCount);
        
        if (result.count == registerCount) {
            // Success - copy values
            memcpy(sample.values, result.values, registerCount * sizeof(uint16_t));
            sample.timestamp = getCurrentTimestampMs();  // Unix timestamp in milliseconds
            
            // Send to compression queue (non-blocking to avoid deadline miss)
            if (xQueueSend(sensorDataQueue, &sample, 0) != pdTRUE) {
                LOG_WARN(LOG_TAG_DATA, "Queue full! Sample dropped");
                // Record as deadline miss (network/queue issue)
                deadlineMonitor_sensorPoll.recordMiss(true);
            }
        } else {
            LOG_ERROR(LOG_TAG_DATA, "Modbus read failed (%zu/%zu regs)", 
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
        LOG_ERROR(LOG_TAG_WATCHDOG, "[%s] DEADLINE MISS! Execution: %lu us, Deadline: %lu us",
              taskName, executionTimeUs, deadlineUs);
        stats.deadlineMisses++;
        
        // Record in appropriate deadline monitor
        bool isNetworkRelated = false;
        if (strcmp(taskName, "SensorPoll") == 0) {
            deadlineMonitor_sensorPoll.recordMiss(isNetworkRelated);
        } else if (strcmp(taskName, "Upload") == 0) {
            isNetworkRelated = (WiFi.status() != WL_CONNECTED);
            deadlineMonitor_upload.recordMiss(isNetworkRelated);
        } else if (strcmp(taskName, "Compression") == 0) {
            deadlineMonitor_compression.recordMiss(isNetworkRelated);
        }
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
    
    LOG_SECTION("SYSTEM HEALTH REPORT");
    LOG_INFO(LOG_TAG_WATCHDOG, "Uptime: %lu seconds", uptime);
    LOG_INFO(LOG_TAG_WATCHDOG, "Free Heap: %lu bytes", ESP.getFreeHeap());
    LOG_INFO(LOG_TAG_WATCHDOG, "Minimum Free Heap: %lu bytes", ESP.getMinFreeHeap());
    // Newline removed - LOG_INFO handles this
    
    LOG_INFO(LOG_TAG_WATCHDOG, "TASK: SensorPoll (CRITICAL)");
    LOG_INFO(LOG_TAG_WATCHDOG, "  Executions: %lu", stats_sensorPoll.executionCount);
    uint32_t avgTime = (stats_sensorPoll.executionCount > 0) ? 
                       (stats_sensorPoll.totalTimeUs / stats_sensorPoll.executionCount) : 0;
    LOG_INFO(LOG_TAG_WATCHDOG, "  Avg Time: %lu us", avgTime);
    LOG_INFO(LOG_TAG_WATCHDOG, "  Max Time: %lu us", stats_sensorPoll.maxTimeUs);
    LOG_INFO(LOG_TAG_WATCHDOG, "  Deadline Misses: %lu", stats_sensorPoll.deadlineMisses);
    LOG_INFO(LOG_TAG_WATCHDOG, "  Stack Free: %lu bytes", stats_sensorPoll.stackHighWater * sizeof(StackType_t));
    // Newline removed - LOG_INFO handles this
    
    LOG_INFO(LOG_TAG_WATCHDOG, "==========================================");
}

// ============================================
// HIGH PRIORITY: Compression Task (Core 1)
// ============================================

void TaskManager::compressionTask(void* parameter) {
    LOG_INFO(LOG_TAG_COMPRESS, "Compression task started on Core %d", xPortGetCoreID());
    
    // Register this task with hardware watchdog
    esp_task_wdt_add(NULL);
    
    const uint32_t deadlineUs = COMPRESSION_DEADLINE_US;  // 2s deadline
    
    // Calculate batch size dynamically based on upload/poll timing
    // batchSize = uploadFrequency / pollFrequency
    // Example: 15000ms / 5000ms = 3 samples per upload cycle
    const size_t batchSize = (uploadFrequency / pollFrequency);
    
    LOG_INFO(LOG_TAG_COMPRESS, "Dynamic batch size: %zu samples (upload: %lu ms / poll: %lu ms)", 
          batchSize, uploadFrequency, pollFrequency);
    
    SensorSample sampleBatch[batchSize > 0 ? batchSize : 1];  // Ensure at least 1
    size_t batchCount = 0;
    
    LOG_INFO(LOG_TAG_COMPRESS, "Batch size: %zu samples", batchSize);
    LOG_INFO(LOG_TAG_COMPRESS, "Deadline: %lu us", deadlineUs);
    
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
                if (xSemaphoreTake(dataPipelineMutex, pdMS_TO_TICKS(DATA_PIPELINE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
                    
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
                        LOG_ERROR(LOG_TAG_COMPRESS, "Compressed data too large (%zu > %zu)",
                              compressedVec.size(), sizeof(packet.data));
                        stats_compression.deadlineMisses++;
                        batchCount = 0;
                        continue;
                    }
                    
                    xSemaphoreGive(dataPipelineMutex);
                    
                    // Send to upload queue (non-blocking to avoid deadline miss)
                    if (xQueueSend(compressedDataQueue, &packet, 0) != pdTRUE) {
                        LOG_WARN(LOG_TAG_COMPRESS, "Upload queue full! Packet dropped");
                        stats_compression.deadlineMisses++;
                    } else {
                        LOG_SUCCESS(LOG_TAG_COMPRESS, "Packet enqueued to upload queue (size: %zu bytes, queue depth: %d)",
                              packet.dataSize, uxQueueMessagesWaiting(compressedDataQueue));
                        
                        // Signal upload task that batch is ready
                        xSemaphoreGive(batchReadySemaphore);
                    }
                    
                    LOG_INFO(LOG_TAG_COMPRESS, "Batch compressed: %zu samples -> %zu bytes (ratio: %.2f%%)",
                          batchSize, packet.dataSize, 
                          (float)packet.compressedSize / packet.uncompressedSize * 100.0f);
                    
                } else {
                    LOG_ERROR(LOG_TAG_COMPRESS, "Failed to acquire pipeline mutex");
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
    LOG_INFO(LOG_TAG_UPLOAD, "Upload task started on Core %d", xPortGetCoreID());
    
    // Register this task with hardware watchdog
    esp_task_wdt_add(NULL);
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t xFrequency = pdMS_TO_TICKS(uploadFrequency);  // NOT const - can update
    const uint32_t deadlineUs = UPLOAD_DEADLINE_US;  // 5s deadline per upload
    
    LOG_INFO(LOG_TAG_UPLOAD, "Upload frequency: %lu ms", uploadFrequency);
    LOG_INFO(LOG_TAG_UPLOAD, "Deadline: %lu us", deadlineUs);
    
    while (1) {
        // ALWAYS wait for the full interval before starting next cycle
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Check if upload frequency was changed by ConfigManager (uses dedicated flag, not semaphore)
        // The semaphore is consumed by other tasks, so upload task uses its own flag
        if (uploadFrequencyChanged) {
            uploadFrequencyChanged = false;  // Clear the flag
            TickType_t newFrequency = pdMS_TO_TICKS(uploadFrequency);
            LOG_DEBUG(LOG_TAG_UPLOAD, "Config reload: current=%lu ticks, new=%lu ticks (uploadFrequency=%lu ms)", 
                      (unsigned long)xFrequency, (unsigned long)newFrequency, uploadFrequency);
            if (newFrequency != xFrequency) {
                xFrequency = newFrequency;
                xLastWakeTime = xTaskGetTickCount();  // Reset timing baseline
                LOG_INFO(LOG_TAG_UPLOAD, "Upload frequency updated to %lu ms", uploadFrequency);
            }
        }
        
        uint32_t startTime = micros();
        
        // Clear any pending batch ready signals (non-blocking)
        // This prevents signal accumulation from previous batches
        while (xSemaphoreTake(batchReadySemaphore, 0) == pdTRUE) {
            // Drain all signals
        }
        
        LOG_DEBUG(LOG_TAG_UPLOAD, "Checking compressed data queue...");
        
        // First, add all pending compressed packets to DataUploader queue
        CompressedPacket packet;
        size_t queuedCount = 0;
        
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
                LOG_WARN(LOG_TAG_UPLOAD, "Failed to queue packet - buffer full");
            }
        }
        
        LOG_INFO(LOG_TAG_UPLOAD, "Queued %zu packets for upload", queuedCount);
        
        // Now upload all pending data (if any)
        bool uploadSuccess = false;
        
        if (queuedCount > 0) {
            LOG_DEBUG(LOG_TAG_UPLOAD, "Attempting to acquire WiFi mutex (timeout: %d ms)...", 
                  WIFI_MUTEX_TIMEOUT_UPLOAD_MS);
            
            // Acquire WiFi client mutex (shared with other network tasks)
            // Uses centralized timeout from system_config.h
            if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(WIFI_MUTEX_TIMEOUT_UPLOAD_MS)) == pdTRUE) {
                
                LOG_DEBUG(LOG_TAG_UPLOAD, "WiFi mutex acquired. Starting upload...");
                
                // Call actual DataUploader API
                uploadSuccess = DataUploader::uploadPendingData();
                
                LOG_DEBUG(LOG_TAG_UPLOAD, "Upload completed. Releasing WiFi mutex...");
                xSemaphoreGive(wifiClientMutex);
                
                if (uploadSuccess) {
                    LOG_SUCCESS(LOG_TAG_UPLOAD, "Successfully uploaded %zu packets", queuedCount);
                    
                    // Signal all tasks to reload configuration (after successful upload)
                    // Give semaphore 6 times (once for each task that needs to reload config):
                    // 1. Sensor Poll Task (registers + poll freq)
                    // 2. Upload Task (upload freq)
                    // 3. Command Task (command freq)
                    // 4. Config Task (config freq)
                    // 5. Power Report Task (power report freq)
                    // 6. OTA Task (OTA freq)
                    for (int i = 0; i < 6; i++) {
                        xSemaphoreGive(configReloadSemaphore);
                    }
                    LOG_DEBUG(LOG_TAG_UPLOAD, "Config reload signal sent to all 6 tasks");
                } else {
                    LOG_ERROR(LOG_TAG_UPLOAD, "Upload failed for %zu packets", queuedCount);
                }
            } else {
                LOG_ERROR(LOG_TAG_UPLOAD, "Failed to acquire WiFi mutex within 15s");
                stats_upload.deadlineMisses++;
            }
        }
        
        uint32_t executionTime = micros() - startTime;
        
        // Record statistics
        recordTaskExecution(stats_upload, executionTime);
        
        // Check deadline but don't retry immediately if missed
        if (executionTime > deadlineUs) {
            stats_upload.deadlineMisses++;
            LOG_WARN(LOG_TAG_UPLOAD, "Deadline miss (%lu us > %lu us) - will retry at next interval",
                     executionTime, deadlineUs);
        }
        
        stats_upload.stackHighWater = uxTaskGetStackHighWaterMark(NULL);
        
        // Feed hardware watchdog (critical for long uploads)
        esp_task_wdt_reset();
    }
}

// ============================================
// MEDIUM-HIGH: Command Task (Core 0)
// ============================================

void TaskManager::commandTask(void* parameter) {
    LOG_INFO(LOG_TAG_COMMAND, "Commands task started on Core %d", xPortGetCoreID());
    
    // Register this task with hardware watchdog
    esp_task_wdt_add(NULL);
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t xFrequency = pdMS_TO_TICKS(commandFrequency);  // Use configurable frequency
    const uint32_t deadlineUs = COMMAND_DEADLINE_US;
    
    LOG_INFO(LOG_TAG_COMMAND, "Check frequency: %lu ms", commandFrequency);
    LOG_INFO(LOG_TAG_COMMAND, "Deadline: %lu us", deadlineUs);
    
    while (1) {
        // ALWAYS wait for the full interval before starting next cycle
        // This prevents rapid retries even if previous cycle missed deadline
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Check if configuration reload is needed (signaled by upload task after successful upload)
        if (xSemaphoreTake(configReloadSemaphore, 0) == pdTRUE) {
            // Reload command frequency from static variable (updated by ConfigManager)
            TickType_t newFrequency = pdMS_TO_TICKS(commandFrequency);
            if (newFrequency != xFrequency) {
                xFrequency = newFrequency;
                xLastWakeTime = xTaskGetTickCount();  // Reset timing baseline
                LOG_INFO(LOG_TAG_COMMAND, "Command frequency updated to %lu ms", commandFrequency);
            }
            // Semaphore consumed - do not give back
        }
        
        uint32_t startTime = micros();
        
        // Feed watchdog before network operation
        yield();
        
        // Acquire WiFi mutex for network commands (uses centralized timeout from system_config.h)
        if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(WIFI_MUTEX_TIMEOUT_COMMAND_MS)) == pdTRUE) {
            
            // Execute command using actual CommandExecutor API
            CommandExecutor::checkAndExecuteCommands();
            
            xSemaphoreGive(wifiClientMutex);
            
        } else {
            // Failed to acquire mutex - skip this cycle and try again next interval
            LOG_DEBUG(LOG_TAG_COMMAND, "Skipped (mutex busy)");
        }
        
        uint32_t executionTime = micros() - startTime;
        
        // Record statistics
        recordTaskExecution(stats_command, executionTime);
        
        // Check deadline but don't retry immediately if missed
        if (executionTime > deadlineUs) {
            stats_command.deadlineMisses++;
            LOG_WARN(LOG_TAG_COMMAND, "Deadline miss (%lu us > %lu us) - will retry at next interval",
                     executionTime, deadlineUs);
        }
        
        stats_command.stackHighWater = uxTaskGetStackHighWaterMark(NULL);
        
        // Feed hardware watchdog (critical - this task blocks on HTTP)
        esp_task_wdt_reset();
    }
}

// ============================================
// MEDIUM: Config Task (Core 0)
// ============================================

void TaskManager::configTask(void* parameter) {
    LOG_INFO(LOG_TAG_CONFIG, "Config task started on Core %d", xPortGetCoreID());
    
    // Register this task with hardware watchdog
    esp_task_wdt_add(NULL);
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t xFrequency = pdMS_TO_TICKS(configFrequency);  // NOT const - can update
    const uint32_t deadlineUs = CONFIG_DEADLINE_US;  // 2s deadline
    
    // State flags for configuration changes
    static bool registers_uptodate = true;
    static bool pollFreq_uptodate = true;
    static bool uploadFreq_uptodate = true;
    
    LOG_INFO(LOG_TAG_CONFIG, "Check frequency: %lu ms", configFrequency);
    LOG_INFO(LOG_TAG_CONFIG, "Deadline: %lu us", deadlineUs);
    
    while (1) {
        // ALWAYS wait for the full interval before starting next cycle
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Check if configuration reload is needed (signaled by upload task after successful upload)
        if (xSemaphoreTake(configReloadSemaphore, 0) == pdTRUE) {
            // Reload config check frequency from static variable (updated by ConfigManager)
            TickType_t newFrequency = pdMS_TO_TICKS(configFrequency);
            if (newFrequency != xFrequency) {
                xFrequency = newFrequency;
                xLastWakeTime = xTaskGetTickCount();  // Reset timing baseline
                LOG_INFO(LOG_TAG_CONFIG, "Config check frequency updated to %lu ms", configFrequency);
            }
            // Semaphore consumed - do not give back
        }
        
        uint32_t startTime = micros();
        
        // Acquire WiFi mutex for HTTP request (timeout from system_config.h)
        if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(WIFI_MUTEX_TIMEOUT_CONFIG_MS)) == pdTRUE) {
            
            // Fetch config from server using actual ConfigManager API
            ConfigManager::checkForChanges(&registers_uptodate, &pollFreq_uptodate, &uploadFreq_uptodate);
            
            xSemaphoreGive(wifiClientMutex);
            
        } else {
            // Failed to acquire mutex - skip this cycle
            LOG_DEBUG(LOG_TAG_CONFIG, "Skipped (mutex busy)");
        }
        
        uint32_t executionTime = micros() - startTime;
        
        // Record statistics
        recordTaskExecution(stats_config, executionTime);
        
        // Check deadline but don't retry immediately if missed
        if (executionTime > deadlineUs) {
            stats_config.deadlineMisses++;
            LOG_WARN(LOG_TAG_CONFIG, "Deadline miss (%lu us > %lu us) - will retry at next interval",
                     executionTime, deadlineUs);
        }
        
        stats_config.stackHighWater = uxTaskGetStackHighWaterMark(NULL);
        
        // Feed hardware watchdog
        esp_task_wdt_reset();
    }
}

// ============================================
// MEDIUM-LOW: Power Report Task (Core 0)
// ============================================

void TaskManager::powerReportTask(void* parameter) {
    LOG_INFO(LOG_TAG_POWER, "PowerReport task started on Core %d", xPortGetCoreID());
    
    // Register with watchdog
    esp_task_wdt_add(NULL);
    LOG_INFO(LOG_TAG_POWER, "Registered with watchdog");
    
    // Load frequency from NVS (converted to ms)
    powerReportFrequency = nvs::getEnergyPollFreq() / 1000;  // Convert μs to ms
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t xFrequency = pdMS_TO_TICKS(powerReportFrequency);
    const uint32_t deadlineUs = POWER_REPORT_DEADLINE_US;  // 5s deadline
    
    LOG_INFO(LOG_TAG_POWER, "Report frequency: %lu ms", powerReportFrequency);
    LOG_INFO(LOG_TAG_POWER, "Deadline: %lu us", deadlineUs);
    
    while (1) {
        // Wait for power report interval
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Check if configuration reload is needed (signaled by upload task after successful upload)
        if (xSemaphoreTake(configReloadSemaphore, 0) == pdTRUE) {
            // Reload power report frequency from static variable (updated by ConfigManager)
            TickType_t newFrequency = pdMS_TO_TICKS(powerReportFrequency);
            if (newFrequency != xFrequency) {
                xFrequency = newFrequency;
                xLastWakeTime = xTaskGetTickCount();  // Reset timing baseline
                LOG_INFO(LOG_TAG_POWER, "Power report frequency updated to %lu ms", powerReportFrequency);
            }
            // Semaphore consumed - do not give back
        }
        
        uint32_t startTime = micros();
        
        // Always collect and report energy data (even if power management is disabled)
        // This allows monitoring energy usage regardless of power-saving state
        bool isPowerMgmtEnabled = PowerManagement::isEnabled();
        
        // Get power statistics
        PowerStats stats = PowerManagement::getStats();
        PowerTechniqueFlags techniques = PowerManagement::getTechniques();
        
        // Build JSON payload (always report, include actual enabled status)
        char jsonBuffer[600];
        snprintf(jsonBuffer, sizeof(jsonBuffer),
            "{"
            "\"device_id\":\"%s\","
            "\"timestamp\":%llu,"
            "\"power_management\":{"
            "\"enabled\":%s,"
            "\"techniques\":\"0x%02X\","
            "\"avg_current_ma\":%.2f,"
            "\"energy_saved_mah\":%.2f,"
            "\"peripheral_savings_mah\":%.2f,"
            "\"uptime_ms\":%lu,"
            "\"high_perf_ms\":%lu,"
            "\"normal_ms\":%lu,"
            "\"low_power_ms\":%lu,"
            "\"sleep_ms\":%lu"
            "}"
            "}",
            DEVICE_ID,
            getCurrentTimestampMs(),
            isPowerMgmtEnabled ? "true" : "false",
            techniques,
            stats.avg_current_ma,
            stats.energy_saved_mah,
            stats.peripheral_savings_mah,
            stats.total_time_ms,
            stats.high_perf_time_ms,
            stats.normal_time_ms,
            stats.low_power_time_ms,
            stats.sleep_time_ms
        );
        
        // Acquire WiFi mutex for network access
        if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(WIFI_MUTEX_TIMEOUT_CONFIG_MS)) == pdTRUE) {
            
            // Send power report to server
            String uploadURL = String(FLASK_SERVER_URL) + "/power/energy/" + DEVICE_ID;
            
            WiFiClient client;
            client.setTimeout(10000);
            
            HTTPClient http;
            http.begin(client, uploadURL);
            http.addHeader("Content-Type", "application/json");
            http.setTimeout(10000);
            
            int httpResponseCode = http.POST(jsonBuffer);
            
            if (httpResponseCode > 0) {
                if (httpResponseCode == 200 || httpResponseCode == 201) {
                    LOG_SUCCESS(LOG_TAG_POWER, "Successfully sent power report");
                } else {
                    LOG_WARN(LOG_TAG_POWER, "Server returned code: %d", httpResponseCode);
                }
            } else {
                LOG_ERROR(LOG_TAG_POWER, "POST failed: %s", http.errorToString(httpResponseCode).c_str());
            }
            
            http.end();
            xSemaphoreGive(wifiClientMutex);
        } else {
            LOG_ERROR(LOG_TAG_POWER, "Failed to acquire WiFi mutex");
        }
        
        uint32_t executionTime = micros() - startTime;
        
        // Record execution statistics
        recordTaskExecution(stats_powerReport, executionTime);
        checkDeadline("PowerReport", executionTime, deadlineUs, stats_powerReport);
        
        // Reset watchdog
        esp_task_wdt_reset();
    }
}

// ============================================
// LOW: OTA Update Task (Core 0)
// ============================================

void TaskManager::otaTask(void* parameter) {
    LOG_INFO(LOG_TAG_FOTA, "OTA task started on Core %d", xPortGetCoreID());
    
    // NOTE: OTA task NOT registered with watchdog - it runs every 60s
    // which exceeds the 30s watchdog timeout. OTA is low priority and infrequent.
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t xFrequency = pdMS_TO_TICKS(otaFrequency);  // NOT const - can update
    const uint32_t deadlineUs = OTA_DEADLINE_US;  // 120s deadline (when active)
    
    // Store OTA manager instance (passed via parameter)
    OTAManager* otaManager = static_cast<OTAManager*>(parameter);
    
    LOG_INFO(LOG_TAG_FOTA, "Check frequency: %lu ms", otaFrequency);
    LOG_INFO(LOG_TAG_FOTA, "Deadline: %lu us", deadlineUs);
    
    while (1) {
        // Wait for OTA check interval
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Check if configuration reload is needed (signaled by upload task after successful upload)
        if (xSemaphoreTake(configReloadSemaphore, 0) == pdTRUE) {
            // Reload OTA frequency from static variable (updated by ConfigManager)
            TickType_t newFrequency = pdMS_TO_TICKS(otaFrequency);
            if (newFrequency != xFrequency) {
                xFrequency = newFrequency;
                xLastWakeTime = xTaskGetTickCount();  // Reset timing baseline
                LOG_INFO(LOG_TAG_FOTA, "OTA check frequency updated to %lu ms", otaFrequency);
            }
            // Semaphore consumed - do not give back
        }
        
        uint32_t startTime = micros();
        
        // Acquire WiFi mutex for network access (uses centralized timeout from system_config.h)
        if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(WIFI_MUTEX_TIMEOUT_CONFIG_MS)) == pdTRUE) {
            
            // Check for firmware updates using actual OTAManager API
            if (otaManager && otaManager->checkForUpdate()) {
                LOG_INFO(LOG_TAG_FOTA, "Firmware update available! Starting download...");
                
                // KEEP the WiFi mutex - don't release it before suspending tasks!
                // This prevents deadlock when re-acquiring after suspend.
                
                // Suspend critical tasks during OTA
                LOG_WARN(LOG_TAG_FOTA, "Suspending critical tasks for update...");
                suspendAllTasks();
                
                // Perform OTA update (we already have the mutex)
                bool otaSuccess = otaManager->downloadAndApplyFirmware();
                
                xSemaphoreGive(wifiClientMutex);
                
                if (otaSuccess) {
                    LOG_SUCCESS(LOG_TAG_FOTA, "Update successful! Verifying and rebooting...");
                    otaManager->verifyAndReboot();
                    // System will reboot - code won't reach here
                } else {
                    LOG_ERROR(LOG_TAG_FOTA, "Update failed! Resuming normal operation...");
                    resumeAllTasks();
                }
            } else {
                xSemaphoreGive(wifiClientMutex);
            }
            
        } else {
            LOG_ERROR(LOG_TAG_FOTA, "Failed to acquire WiFi mutex");
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
    LOG_INFO(LOG_TAG_WATCHDOG, "Watchdog task started on Core %d", xPortGetCoreID());
    
    // Register this task with hardware watchdog
    esp_task_wdt_add(NULL);
    
    const TickType_t xCheckInterval = pdMS_TO_TICKS(WATCHDOG_CHECK_INTERVAL_MS);
    const uint32_t maxTaskIdleTime = MAX_TASK_IDLE_TIME_MS;
    
    LOG_INFO(LOG_TAG_WATCHDOG, "Check interval: %lu ms", WATCHDOG_CHECK_INTERVAL_MS);
    LOG_INFO(LOG_TAG_WATCHDOG, "Max task idle time: %lu ms", maxTaskIdleTime);
    
    // Track WiFi state for network recovery detection
    static bool wasWiFiConnected = (WiFi.status() == WL_CONNECTED);
    
    while (1) {
        vTaskDelay(xCheckInterval);
        
        uint32_t currentTime = millis();
        uint32_t startTime = micros();
        
        // Check for WiFi reconnection and clear network-related deadline misses
        bool isWiFiConnected = (WiFi.status() == WL_CONNECTED);
        if (isWiFiConnected && !wasWiFiConnected) {
            // WiFi just reconnected - reset network-related deadline counters
            LOG_INFO(LOG_TAG_WATCHDOG, "WiFi reconnected - clearing network-related deadline misses");
            deadlineMonitor_sensorPoll.onNetworkRestored();
            deadlineMonitor_upload.onNetworkRestored();
            deadlineMonitor_compression.onNetworkRestored();
        }
        wasWiFiConnected = isWiFiConnected;
        
        // Check sensor poll task (CRITICAL)
        if (currentTime - stats_sensorPoll.lastRunTime > maxTaskIdleTime) {
            LOG_ERROR(LOG_TAG_WATCHDOG, "CRITICAL: SensorPoll task stalled! Last run: %lu ms ago",
                  currentTime - stats_sensorPoll.lastRunTime);
            LOG_ERROR(LOG_TAG_WATCHDOG, "SYSTEM RESET TRIGGERED!");
            vTaskDelay(pdMS_TO_TICKS(1000));  // Give time for log
            ESP.restart();
        }
        
        // Check upload task (HIGH)
        if (currentTime - stats_upload.lastRunTime > uploadFrequency * 3) {
            LOG_WARN(LOG_TAG_WATCHDOG, "Upload task delayed! Last run: %lu ms ago",
                  currentTime - stats_upload.lastRunTime);
        }
        
        // Check compression task (HIGH)
        if (currentTime - stats_compression.lastRunTime > pollFrequency * 10) {
            LOG_WARN(LOG_TAG_WATCHDOG, "Compression task delayed! Last run: %lu ms ago",
                  currentTime - stats_compression.lastRunTime);
        }
        
        // Check for excessive deadline misses (using intelligent monitoring)
        if (deadlineMonitor_sensorPoll.shouldRestart()) {
            uint8_t recentMisses = deadlineMonitor_sensorPoll.getRecentMisses();
            uint32_t lifetimeMisses = deadlineMonitor_sensorPoll.getLifetimeMisses();
            uint32_t networkMisses = deadlineMonitor_sensorPoll.getNetworkMisses();
            
            LOG_ERROR(LOG_TAG_WATCHDOG, "CRITICAL: Excessive sensor deadline misses!");
            LOG_ERROR(LOG_TAG_WATCHDOG, "Recent: %d, Lifetime: %lu, Network-related: %lu",
                     recentMisses, lifetimeMisses, networkMisses);
            
            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP.restart();
        }
        
        // Print health report periodically (using centralized interval)
        static uint32_t lastHealthReport = 0;
        if (currentTime - lastHealthReport > HEALTH_REPORT_INTERVAL_MS) {
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

// ============================================
// Frequency Update Functions
// ============================================

void TaskManager::updatePollFrequency(uint32_t newFreqMs) {
    pollFrequency = newFreqMs;
    LOG_INFO(LOG_TAG_BOOT, "Poll frequency updated to %lu ms", newFreqMs);
}

void TaskManager::updateUploadFrequency(uint32_t newFreqMs) {
    uint32_t oldFreq = uploadFrequency;
    uploadFrequency = newFreqMs;
    uploadFrequencyChanged = true;  // Signal the upload task to reload
    LOG_INFO(LOG_TAG_BOOT, "Upload frequency static var updated: %lu ms -> %lu ms", oldFreq, newFreqMs);
}

void TaskManager::updateConfigFrequency(uint32_t newFreqMs) {
    configFrequency = newFreqMs;
    LOG_INFO(LOG_TAG_BOOT, "Config check frequency updated to %lu ms", newFreqMs);
}

void TaskManager::updateCommandFrequency(uint32_t newFreqMs) {
    commandFrequency = newFreqMs;
    LOG_INFO(LOG_TAG_BOOT, "Command poll frequency updated to %lu ms", newFreqMs);
}

void TaskManager::updateOtaFrequency(uint32_t newFreqMs) {
    otaFrequency = newFreqMs;
    LOG_INFO(LOG_TAG_BOOT, "OTA check frequency updated to %lu ms", newFreqMs);
}

void TaskManager::updatePowerReportFrequency(uint32_t newFreqMs) {
    powerReportFrequency = newFreqMs;
    LOG_INFO(LOG_TAG_BOOT, "Power report frequency updated to %lu ms", newFreqMs);
}
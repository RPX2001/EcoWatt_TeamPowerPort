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
SemaphoreHandle_t TaskManager::rawSampleMutex = NULL;

// Raw sample buffer for compress-on-upload architecture
SensorSample TaskManager::rawSampleBuffer[RAW_SAMPLE_BUFFER_SIZE];
size_t TaskManager::rawSampleHead = 0;
size_t TaskManager::rawSampleCount = 0;

// Upload frequency reload flag (separate from semaphore since upload task gives the semaphore)
static volatile bool uploadFrequencyChanged = false;

// Flags to indicate tasks should reload config (set by upload task AFTER buffer drain)
// This ensures ALL config changes apply only after current data is uploaded
static volatile bool sensorConfigReloadPending = false;
static volatile bool commandConfigReloadPending = false;
static volatile bool configTaskReloadPending = false;
static volatile bool powerReportConfigReloadPending = false;
static volatile bool otaConfigReloadPending = false;

// Flag set by ConfigManager when cloud config change is detected (read by upload task)
// This prevents unnecessary config reload after every upload when no change occurred
static volatile bool cloudConfigChangePending = false;

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
bool TaskManager::tasksNeedTimingReset = false;
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
    
    // Create mutex for raw sample buffer access (compress-on-upload architecture)
    rawSampleMutex = xSemaphoreCreateMutex();
    if (!rawSampleMutex) {
        LOG_ERROR(LOG_TAG_BOOT, "Failed to create raw sample mutex");
        return false;
    }
    
    // Initialize raw sample buffer
    rawSampleHead = 0;
    rawSampleCount = 0;
    
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
    
    // NOTE: Compression Task REMOVED - compression now happens inside Upload Task
    // (compress-on-upload architecture to prevent register layout misalignment when config changes)
    // This saves ~6KB of stack memory
    LOG_INFO(LOG_TAG_BOOT, "Compression Task DISABLED (compress-on-upload architecture)");
    
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
    // compressionTask removed - compression now in upload task
    if (uploadTask_h) vTaskSuspend(uploadTask_h);
    if (commandTask_h) vTaskSuspend(commandTask_h);
    if (configTask_h) vTaskSuspend(configTask_h);
    if (statisticsTask_h) vTaskSuspend(statisticsTask_h);
    if (powerReportTask_h) vTaskSuspend(powerReportTask_h);  // Suspend power reporting during OTA
    // NOTE: Do NOT suspend otaTask_h - it's the one calling this function!
    if (watchdogTask_h) vTaskSuspend(watchdogTask_h);
    
    systemSuspended = true;
    LOG_INFO(LOG_TAG_BOOT, "All tasks suspended (OTA still running)");
}

void TaskManager::resumeAllTasks() {
    LOG_INFO(LOG_TAG_BOOT, "Resuming all tasks...");
    
    // Signal all tasks to reset their timing baselines
    tasksNeedTimingReset = true;
    
    if (sensorPollTask_h) vTaskResume(sensorPollTask_h);
    // compressionTask removed - compression now in upload task
    if (uploadTask_h) vTaskResume(uploadTask_h);
    if (commandTask_h) vTaskResume(commandTask_h);
    if (configTask_h) vTaskResume(configTask_h);
    if (statisticsTask_h) vTaskResume(statisticsTask_h);
    if (powerReportTask_h) vTaskResume(powerReportTask_h);  // Resume power reporting after OTA
    if (otaTask_h) vTaskResume(otaTask_h);
    if (watchdogTask_h) vTaskResume(watchdogTask_h);
    
    systemSuspended = false;
    
    // Give tasks time to check the flag and reset their timing (500ms should be plenty)
    vTaskDelay(pdMS_TO_TICKS(500));
    tasksNeedTimingReset = false;  // Clear flag after all tasks have had a chance to see it
    
    LOG_INFO(LOG_TAG_BOOT, "All tasks resumed - timing baselines will reset");
}

// ============================================
// CRITICAL TASK: Sensor Polling (Core 1)
// ============================================

void TaskManager::sensorPollTask(void* parameter) {
    LOG_INFO(LOG_TAG_DATA, "SensorPoll task started on Core %d", xPortGetCoreID());
    
    // Register this task with hardware watchdog
    esp_task_wdt_add(NULL);
    
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
        // Wait for poll interval OR until notified of config change
        uint32_t notifyValue = 0;
        BaseType_t notified = xTaskNotifyWait(0, ULONG_MAX, &notifyValue, xFrequency);
        
        // Check if we were woken by config change notification
        if (notified == pdTRUE && notifyValue == 1) {
            // Config changed - reload frequency and restart wait
            xFrequency = pdMS_TO_TICKS(pollFrequency);
            LOG_INFO(LOG_TAG_DATA, "Config change - poll interval now %lu ms (restarting timer)", pollFrequency);
            continue;  // Restart wait with new frequency
        }
        
        // Check if tasks were resumed after suspension (e.g., after OTA)
        if (tasksNeedTimingReset) {
            LOG_INFO(LOG_TAG_DATA, "Timing baseline reset after task resume");
        }
        
        // Check if configuration reload is needed (flag set by upload task AFTER buffer drain)
        // This ensures ALL samples in a buffer use the same config before any changes apply
        if (sensorConfigReloadPending) {
            sensorConfigReloadPending = false;  // Clear the flag
            LOG_INFO(LOG_TAG_DATA, "Config reload after upload - applying pending changes");
            
            // Reload poll frequency from static variable (updated by ConfigManager)
            TickType_t newFrequency = pdMS_TO_TICKS(pollFrequency);
            if (newFrequency != xFrequency) {
                xFrequency = newFrequency;
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
            
            // CRITICAL: Check if config changed DURING this Modbus read
            // If so, discard this sample (it was read with old config) and apply new config immediately
            if (sensorConfigReloadPending) {
                sensorConfigReloadPending = false;
                LOG_WARN(LOG_TAG_DATA, "Config changed during Modbus read - discarding sample, applying new config");
                
                // Apply new config immediately
                TickType_t newFrequency = pdMS_TO_TICKS(pollFrequency);
                if (newFrequency != xFrequency) {
                    xFrequency = newFrequency;
                    LOG_INFO(LOG_TAG_DATA, "Poll frequency updated to %lu ms", pollFrequency);
                }
                
                xSemaphoreTake(nvsAccessMutex, portMAX_DELAY);
                size_t newRegisterCount = nvs::getReadRegCount();
                const RegID* newRegisters = nvs::getReadRegs();
                if (newRegisterCount != registerCount || 
                    memcmp(registers, newRegisters, registerCount * sizeof(RegID)) != 0) {
                    registerCount = newRegisterCount;
                    registers = newRegisters;
                    LOG_INFO(LOG_TAG_DATA, "Register configuration updated - now monitoring %zu registers", registerCount);
                }
                xSemaphoreGive(nvsAccessMutex);
                
                // Don't store this sample - continue to next iteration with new config
                uint32_t executionTime = micros() - startTime;
                recordTaskExecution(stats_sensorPoll, executionTime);
                esp_task_wdt_reset();
                continue;
            }
            
            // Store in raw sample buffer (compress-on-upload architecture)
            // This ensures all samples in an upload batch have consistent register layout
            if (xSemaphoreTake(rawSampleMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (rawSampleCount < RAW_SAMPLE_BUFFER_SIZE) {
                    // Buffer has space - add sample
                    rawSampleBuffer[rawSampleHead] = sample;
                    rawSampleHead = (rawSampleHead + 1) % RAW_SAMPLE_BUFFER_SIZE;
                    rawSampleCount++;
                    LOG_DEBUG(LOG_TAG_DATA, "Sample stored in buffer (%zu/%d)", 
                              rawSampleCount, RAW_SAMPLE_BUFFER_SIZE);
                } else {
                    // Buffer full - overwrite oldest (ring buffer behavior)
                    LOG_WARN(LOG_TAG_DATA, "Raw sample buffer full! Overwriting oldest sample");
                    rawSampleBuffer[rawSampleHead] = sample;
                    rawSampleHead = (rawSampleHead + 1) % RAW_SAMPLE_BUFFER_SIZE;
                    // Count stays at max
                }
                xSemaphoreGive(rawSampleMutex);
            } else {
                LOG_WARN(LOG_TAG_DATA, "Failed to acquire raw sample mutex");
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
    
    TickType_t xFrequency = pdMS_TO_TICKS(uploadFrequency);  // NOT const - can update
    const uint32_t deadlineUs = UPLOAD_DEADLINE_US;  // 5s deadline per upload
    
    LOG_INFO(LOG_TAG_UPLOAD, "Upload frequency: %lu ms", uploadFrequency);
    LOG_INFO(LOG_TAG_UPLOAD, "Deadline: %lu us", deadlineUs);
    
    while (1) {
        // Wait for upload interval OR until notified of config change
        uint32_t notifyValue = 0;
        BaseType_t notified = xTaskNotifyWait(0, ULONG_MAX, &notifyValue, xFrequency);
        
        // Check if we were woken by config change notification
        if (notified == pdTRUE && notifyValue == 1) {
            // Config changed - reload frequency and restart wait
            xFrequency = pdMS_TO_TICKS(uploadFrequency);
            LOG_INFO(LOG_TAG_UPLOAD, "Config change - upload interval now %lu ms (restarting timer)", uploadFrequency);
            continue;  // Restart wait with new frequency
        }
        
        // Check if tasks were resumed after suspension (e.g., after OTA)
        if (tasksNeedTimingReset) {
            LOG_INFO(LOG_TAG_UPLOAD, "Timing baseline reset after task resume");
        }
        
        // Check if upload frequency was changed by ConfigManager (uses dedicated flag, not semaphore)
        // The semaphore is consumed by other tasks, so upload task uses its own flag
        if (uploadFrequencyChanged) {
            uploadFrequencyChanged = false;  // Clear the flag
            TickType_t newFrequency = pdMS_TO_TICKS(uploadFrequency);
            LOG_DEBUG(LOG_TAG_UPLOAD, "Config reload: current=%lu ticks, new=%lu ticks (uploadFrequency=%lu ms)", 
                      (unsigned long)xFrequency, (unsigned long)newFrequency, uploadFrequency);
            if (newFrequency != xFrequency) {
                xFrequency = newFrequency;
                LOG_INFO(LOG_TAG_UPLOAD, "Upload frequency updated to %lu ms", uploadFrequency);
            }
        }
        
        uint32_t startTime = micros();
        
        // =========================================================
        // COMPRESS-ON-UPLOAD ARCHITECTURE
        // Compress ALL raw samples just before upload to ensure
        // consistent register layout (no mixing old/new configs)
        // =========================================================
        
        size_t samplesToProcess = 0;
        SensorSample* localSamples = nullptr;
        
        // Step 1: Drain raw sample buffer (under mutex protection)
        if (xSemaphoreTake(rawSampleMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            samplesToProcess = rawSampleCount;
            
            if (samplesToProcess > 0) {
                // Allocate local copy of samples
                localSamples = new SensorSample[samplesToProcess];
                if (localSamples) {
                    // Copy samples from ring buffer (in order: oldest to newest)
                    size_t readPos = (rawSampleHead + RAW_SAMPLE_BUFFER_SIZE - rawSampleCount) % RAW_SAMPLE_BUFFER_SIZE;
                    for (size_t i = 0; i < samplesToProcess; i++) {
                        localSamples[i] = rawSampleBuffer[readPos];
                        readPos = (readPos + 1) % RAW_SAMPLE_BUFFER_SIZE;
                    }
                    
                    // Clear the buffer
                    rawSampleCount = 0;
                    rawSampleHead = 0;
                    LOG_INFO(LOG_TAG_UPLOAD, "Drained %zu samples from raw buffer", samplesToProcess);
                } else {
                    LOG_ERROR(LOG_TAG_UPLOAD, "Failed to allocate memory for %zu samples", samplesToProcess);
                    samplesToProcess = 0;
                }
            }
            xSemaphoreGive(rawSampleMutex);
        } else {
            LOG_ERROR(LOG_TAG_UPLOAD, "Failed to acquire raw sample mutex");
        }
        
        // Step 2: Compress and queue all samples (if any)
        size_t queuedCount = 0;
        
        if (samplesToProcess > 0 && localSamples != nullptr) {
            // Feed watchdog during compression
            yield();
            
            // Get register info from first sample (all samples have same layout in this batch)
            size_t registerCount = localSamples[0].registerCount;
            
            // Convert samples to linear array for compression
            size_t totalRegisterCount = samplesToProcess * registerCount;
            uint16_t* linearData = new uint16_t[totalRegisterCount];
            RegID* linearRegs = new RegID[totalRegisterCount];
            
            if (linearData && linearRegs) {
                for (size_t i = 0; i < samplesToProcess; i++) {
                    memcpy(linearData + (i * registerCount), 
                           localSamples[i].values, 
                           registerCount * sizeof(uint16_t));
                    memcpy(linearRegs + (i * registerCount),
                           localSamples[i].registers,
                           registerCount * sizeof(RegID));
                }
                
                // Compress all samples together
                LOG_INFO(LOG_TAG_UPLOAD, "Compressing %zu samples (%zu registers each)...", 
                         samplesToProcess, registerCount);
                
                std::vector<uint8_t> compressedVec = DataCompression::compressWithSmartSelection(
                    linearData, linearRegs, totalRegisterCount);
                
                // Create SmartCompressedData for upload
                SmartCompressedData smartData;
                smartData.binaryData = compressedVec;
                smartData.timestamp = localSamples[samplesToProcess - 1].timestamp;  // Use last sample timestamp
                smartData.sampleCount = samplesToProcess;
                smartData.registerCount = registerCount;
                smartData.originalSize = totalRegisterCount * sizeof(uint16_t);
                smartData.academicRatio = (compressedVec.size() > 0) ? 
                    (float)compressedVec.size() / (float)smartData.originalSize : 1.0f;
                smartData.traditionalRatio = (compressedVec.size() > 0) ? 
                    (float)smartData.originalSize / (float)compressedVec.size() : 0.0f;
                
                // Copy register layout from first sample
                memcpy(smartData.registers, localSamples[0].registers, registerCount * sizeof(RegID));
                
                // Determine compression method from header
                if (compressedVec.size() > 0) {
                    switch (compressedVec[0]) {
                        case 0xD0: strncpy(smartData.compressionMethod, "dictionary", sizeof(smartData.compressionMethod) - 1); break;
                        case 0x70:
                        case 0x71: strncpy(smartData.compressionMethod, "temporal", sizeof(smartData.compressionMethod) - 1); break;
                        case 0x50: strncpy(smartData.compressionMethod, "semantic", sizeof(smartData.compressionMethod) - 1); break;
                        default: strncpy(smartData.compressionMethod, "bitpack", sizeof(smartData.compressionMethod) - 1);
                    }
                } else {
                    strncpy(smartData.compressionMethod, "raw", sizeof(smartData.compressionMethod) - 1);
                }
                
                LOG_INFO(LOG_TAG_UPLOAD, "Compressed %zu samples -> %zu bytes (%.1f%% savings)", 
                         samplesToProcess, compressedVec.size(),
                         (1.0f - smartData.academicRatio) * 100.0f);
                
                // Add to upload queue
                if (DataUploader::addToQueue(smartData)) {
                    queuedCount = 1;  // One compressed packet containing all samples
                } else {
                    LOG_ERROR(LOG_TAG_UPLOAD, "Failed to queue compressed packet");
                }
                
                delete[] linearData;
                delete[] linearRegs;
            } else {
                LOG_ERROR(LOG_TAG_UPLOAD, "Failed to allocate compression buffers");
                if (linearData) delete[] linearData;
                if (linearRegs) delete[] linearRegs;
            }
            
            delete[] localSamples;
            localSamples = nullptr;
        }
        
        LOG_INFO(LOG_TAG_UPLOAD, "Prepared %zu packet(s) for upload", queuedCount);
        
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
                    
                    // Only signal config reload if there's actually a pending cloud config change
                    // This prevents unnecessary reload after every upload when no config changed
                    if (cloudConfigChangePending) {
                        cloudConfigChangePending = false;  // Clear the flag
                        
                        // Signal ALL tasks to reload config via flags (ensures changes apply AFTER buffer drain)
                        // This prevents mixing old/new configs in the same upload batch
                        sensorConfigReloadPending = true;
                        commandConfigReloadPending = true;
                        configTaskReloadPending = true;
                        powerReportConfigReloadPending = true;
                        otaConfigReloadPending = true;
                        uploadFrequencyChanged = true;
                        
                        LOG_INFO(LOG_TAG_UPLOAD, "Config reload flags set for all tasks (cloud change detected)");
                    }
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
    
    TickType_t xFrequency = pdMS_TO_TICKS(commandFrequency);  // Use configurable frequency
    const uint32_t deadlineUs = COMMAND_DEADLINE_US;
    
    LOG_INFO(LOG_TAG_COMMAND, "Check frequency: %lu ms", commandFrequency);
    LOG_INFO(LOG_TAG_COMMAND, "Deadline: %lu us", deadlineUs);
    
    while (1) {
        // Wait for command poll interval OR until notified of config change
        uint32_t notifyValue = 0;
        BaseType_t notified = xTaskNotifyWait(0, ULONG_MAX, &notifyValue, xFrequency);
        
        // Check if we were woken by config change notification
        if (notified == pdTRUE && notifyValue == 1) {
            // Config changed - reload frequency and restart wait
            xFrequency = pdMS_TO_TICKS(commandFrequency);
            LOG_INFO(LOG_TAG_COMMAND, "Config change - command interval now %lu ms (restarting timer)", commandFrequency);
            continue;  // Restart wait with new frequency
        }
        
        // Check if tasks were resumed after suspension (e.g., after OTA)
        if (tasksNeedTimingReset) {
            LOG_INFO(LOG_TAG_COMMAND, "Timing baseline reset after task resume");
        }
        
        // Check if configuration reload is needed (flag set by upload task AFTER buffer drain)
        if (commandConfigReloadPending) {
            commandConfigReloadPending = false;  // Clear the flag
            // Reload command frequency from NVS (source of truth)
            commandFrequency = nvs::getCommandFreq() / 1000;  // Convert μs to ms
            TickType_t newFrequency = pdMS_TO_TICKS(commandFrequency);
            if (newFrequency != xFrequency) {
                xFrequency = newFrequency;
                LOG_INFO(LOG_TAG_COMMAND, "Command frequency updated to %lu ms (takes effect next cycle)", commandFrequency);
            }
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
    
    TickType_t xFrequency = pdMS_TO_TICKS(configFrequency);  // NOT const - can update
    const uint32_t deadlineUs = CONFIG_DEADLINE_US;  // 2s deadline
    
    // State flags for configuration changes
    static bool registers_uptodate = true;
    static bool pollFreq_uptodate = true;
    static bool uploadFreq_uptodate = true;
    
    LOG_INFO(LOG_TAG_CONFIG, "Check frequency: %lu ms", configFrequency);
    LOG_INFO(LOG_TAG_CONFIG, "Deadline: %lu us", deadlineUs);
    
    while (1) {
        // Wait for config poll interval OR until notified of config change
        uint32_t notifyValue = 0;
        BaseType_t notified = xTaskNotifyWait(0, ULONG_MAX, &notifyValue, xFrequency);
        
        // Check if we were woken by config change notification
        if (notified == pdTRUE && notifyValue == 1) {
            // Config changed - reload frequency and restart wait
            xFrequency = pdMS_TO_TICKS(configFrequency);
            LOG_INFO(LOG_TAG_CONFIG, "Config change - config poll interval now %lu ms (restarting timer)", configFrequency);
            continue;  // Restart wait with new frequency
        }
        
        // Check if tasks were resumed after suspension (e.g., after OTA)
        if (tasksNeedTimingReset) {
            LOG_INFO(LOG_TAG_CONFIG, "Timing baseline reset after task resume");
        }
        
        // Check if configuration reload is needed (flag set by upload task AFTER buffer drain)
        if (configTaskReloadPending) {
            configTaskReloadPending = false;  // Clear the flag
            // Reload config check frequency from NVS (source of truth)
            configFrequency = nvs::getConfigFreq() / 1000;  // Convert μs to ms
            TickType_t newFrequency = pdMS_TO_TICKS(configFrequency);
            if (newFrequency != xFrequency) {
                xFrequency = newFrequency;
                LOG_INFO(LOG_TAG_CONFIG, "Config check frequency updated to %lu ms (takes effect next cycle)", configFrequency);
            }
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
    
    TickType_t xFrequency = pdMS_TO_TICKS(powerReportFrequency);
    const uint32_t deadlineUs = POWER_REPORT_DEADLINE_US;  // 5s deadline
    
    LOG_INFO(LOG_TAG_POWER, "Report frequency: %lu ms", powerReportFrequency);
    LOG_INFO(LOG_TAG_POWER, "Deadline: %lu us", deadlineUs);
    
    while (1) {
        // Wait for power report interval OR until notified of config change
        uint32_t notifyValue = 0;
        BaseType_t notified = xTaskNotifyWait(0, ULONG_MAX, &notifyValue, xFrequency);
        
        // Check if we were woken by config change notification
        if (notified == pdTRUE && notifyValue == 1) {
            // Config changed - reload frequency and restart wait
            xFrequency = pdMS_TO_TICKS(powerReportFrequency);
            LOG_INFO(LOG_TAG_POWER, "Config change - power report interval now %lu ms (restarting timer)", powerReportFrequency);
            continue;  // Restart wait with new frequency
        }
        
        // Check if tasks were resumed after suspension (e.g., after OTA)
        if (tasksNeedTimingReset) {
            LOG_INFO(LOG_TAG_POWER, "Timing baseline reset after task resume");
        }
        
        // Check if configuration reload is needed (flag set by upload task AFTER buffer drain)
        if (powerReportConfigReloadPending) {
            powerReportConfigReloadPending = false;  // Clear the flag
            // Reload power report frequency from NVS (source of truth)
            powerReportFrequency = nvs::getEnergyPollFreq() / 1000;  // Convert μs to ms
            TickType_t newFrequency = pdMS_TO_TICKS(powerReportFrequency);
            if (newFrequency != xFrequency) {
                xFrequency = newFrequency;
                LOG_INFO(LOG_TAG_POWER, "Power report frequency updated to %lu ms (takes effect next cycle)", powerReportFrequency);
            }
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
    
    TickType_t xFrequency = pdMS_TO_TICKS(otaFrequency);  // NOT const - can update
    const uint32_t deadlineUs = OTA_DEADLINE_US;  // 120s deadline (when active)
    
    // Store OTA manager instance (passed via parameter)
    OTAManager* otaManager = static_cast<OTAManager*>(parameter);
    
    LOG_INFO(LOG_TAG_FOTA, "Check frequency: %lu ms", otaFrequency);
    LOG_INFO(LOG_TAG_FOTA, "Deadline: %lu us", deadlineUs);
    
    while (1) {
        // Wait for OTA check interval OR until notified of config change
        // xTaskNotifyWait returns pdTRUE if notified, pdFALSE if timeout (normal OTA check)
        uint32_t notifyValue = 0;
        BaseType_t notified = xTaskNotifyWait(
            0,                      // Don't clear bits on entry
            ULONG_MAX,              // Clear all bits on exit
            &notifyValue,           // Store notification value
            xFrequency              // Wait for OTA interval (timeout)
        );
        
        // Check if we were woken by config change notification
        if (notified == pdTRUE && notifyValue == 1) {
            // Config changed - use the static variable (already updated by updateOtaFrequency)
            // Don't read from NVS here as it may have race conditions
            LOG_INFO(LOG_TAG_FOTA, "Notification received! Static otaFrequency = %lu ms", otaFrequency);
            xFrequency = pdMS_TO_TICKS(otaFrequency);
            LOG_INFO(LOG_TAG_FOTA, "Config change detected - OTA interval now %lu ms (restarting timer)", otaFrequency);
            continue;  // Restart the wait with new frequency
        }
        
        // Normal timeout - time to check for OTA updates
        
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
                
                // ALWAYS release mutex and resume tasks before checking result
                xSemaphoreGive(wifiClientMutex);
                
                if (otaSuccess) {
                    LOG_SUCCESS(LOG_TAG_FOTA, "Update successful! Verifying and rebooting...");
                    otaManager->verifyAndReboot();
                    // System will reboot - code won't reach here
                } else {
                    LOG_ERROR(LOG_TAG_FOTA, "Update failed or cancelled! Resuming normal operation...");
                    resumeAllTasks();
                    LOG_INFO(LOG_TAG_FOTA, "All tasks resumed - system operational");
                    LOG_INFO(LOG_TAG_FOTA, "Next OTA check in %lu ms", otaFrequency);
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
        
        // NOTE: Compression task monitoring removed - compression now happens inside upload task
        // (compress-on-upload architecture to prevent register layout misalignment)
        
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
    
    // Wake up the sensor poll task immediately to apply new frequency
    if (sensorPollTask_h != NULL) {
        xTaskNotify(sensorPollTask_h, 1, eSetValueWithOverwrite);
        LOG_INFO(LOG_TAG_BOOT, "Sensor poll task notified to apply new frequency immediately");
    }
}

void TaskManager::updateUploadFrequency(uint32_t newFreqMs) {
    uint32_t oldFreq = uploadFrequency;
    uploadFrequency = newFreqMs;
    uploadFrequencyChanged = true;  // Signal the upload task to reload
    LOG_INFO(LOG_TAG_BOOT, "Upload frequency static var updated: %lu ms -> %lu ms", oldFreq, newFreqMs);
    
    // Wake up the upload task immediately to apply new frequency
    if (uploadTask_h != NULL) {
        xTaskNotify(uploadTask_h, 1, eSetValueWithOverwrite);
        LOG_INFO(LOG_TAG_BOOT, "Upload task notified to apply new frequency immediately");
    }
}

void TaskManager::updateConfigFrequency(uint32_t newFreqMs) {
    configFrequency = newFreqMs;
    LOG_INFO(LOG_TAG_BOOT, "Config check frequency updated to %lu ms", newFreqMs);
    
    // Wake up the config task immediately to apply new frequency
    if (configTask_h != NULL) {
        xTaskNotify(configTask_h, 1, eSetValueWithOverwrite);
        LOG_INFO(LOG_TAG_BOOT, "Config task notified to apply new frequency immediately");
    }
}

void TaskManager::updateCommandFrequency(uint32_t newFreqMs) {
    commandFrequency = newFreqMs;
    LOG_INFO(LOG_TAG_BOOT, "Command poll frequency updated to %lu ms", newFreqMs);
    
    // Wake up the command task immediately to apply new frequency
    if (commandTask_h != NULL) {
        xTaskNotify(commandTask_h, 1, eSetValueWithOverwrite);
        LOG_INFO(LOG_TAG_BOOT, "Command task notified to apply new frequency immediately");
    }
}

void TaskManager::updateOtaFrequency(uint32_t newFreqMs) {
    uint32_t oldFreq = otaFrequency;
    otaFrequency = newFreqMs;
    LOG_INFO(LOG_TAG_BOOT, "OTA check frequency updated: %lu ms -> %lu ms", oldFreq, newFreqMs);
    
    // Wake up the OTA task immediately so it can apply the new frequency
    // This prevents waiting for the old (potentially very long) interval to expire
    if (otaTask_h != NULL) {
        BaseType_t result = xTaskNotify(otaTask_h, 1, eSetValueWithOverwrite);  // Value 1 = config change notification
        LOG_INFO(LOG_TAG_BOOT, "OTA task notified (result=%d), otaFrequency static var is now %lu ms", result, otaFrequency);
    } else {
        LOG_WARN(LOG_TAG_BOOT, "OTA task handle is NULL - cannot notify!");
    }
}

void TaskManager::updatePowerReportFrequency(uint32_t newFreqMs) {
    powerReportFrequency = newFreqMs;
    LOG_INFO(LOG_TAG_BOOT, "Power report frequency updated to %lu ms", newFreqMs);
    
    // Wake up the power report task immediately to apply new frequency
    if (powerReportTask_h != NULL) {
        xTaskNotify(powerReportTask_h, 1, eSetValueWithOverwrite);
        LOG_INFO(LOG_TAG_BOOT, "Power report task notified to apply new frequency immediately");
    }
}

void TaskManager::setCloudConfigChangePending(bool pending) {
    cloudConfigChangePending = pending;
    if (pending) {
        LOG_INFO(LOG_TAG_BOOT, "Cloud config change detected - will apply after next upload");
    }
}

// ============================================
// Raw Sample Buffer Access Functions
// ============================================

SensorSample* TaskManager::getRawSampleBuffer() {
    return rawSampleBuffer;
}

size_t TaskManager::getRawSampleCount() {
    return rawSampleCount;
}

void TaskManager::clearRawSampleBuffer() {
    rawSampleHead = 0;
    rawSampleCount = 0;
}

SemaphoreHandle_t TaskManager::getRawSampleMutex() {
    return rawSampleMutex;
}
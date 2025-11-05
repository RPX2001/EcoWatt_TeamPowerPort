/**
 * @file task_manager.h
 * @brief FreeRTOS dual-core task management system
 * 
 * Implements real-time multi-core task scheduling with guaranteed deadlines
 * 
 * Core Assignment:
 *   Core 0 (PRO_CPU): WiFi stack, HTTP uploads, network operations
 *   Core 1 (APP_CPU): Sensor polling, compression, critical timing
 * 
 * @author Team PowerPort
 * @date 2025-10-28
 */

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "peripheral/acquisition.h"

// ============================================
// Task Configuration
// ============================================

// Core Assignments
#define CORE_NETWORK    0  // Core 0: WiFi, HTTP, MQTT
#define CORE_SENSORS    1  // Core 1: Polling, Compression, Processing

// Task Priorities (0-24, higher = more important)
#define PRIORITY_SENSOR_POLL    24  // CRITICAL - highest priority
#define PRIORITY_UPLOAD         20  // HIGH - network operations
#define PRIORITY_COMPRESSION    18  // HIGH - CPU intensive
#define PRIORITY_COMMANDS       16  // MEDIUM-HIGH
#define PRIORITY_CONFIG         12  // MEDIUM
#define PRIORITY_STATISTICS     10  // MEDIUM-LOW
#define PRIORITY_POWER_REPORT   8   // MEDIUM-LOW - periodic power stats
#define PRIORITY_OTA            5   // LOW - runs when idle
#define PRIORITY_WATCHDOG       1   // LOWEST - safety net

// Stack Sizes (bytes)
#define STACK_SENSOR_POLL       8192  // Increased for fault recovery (Milestone 5)
#define STACK_UPLOAD            12288 // Increased for JSON payload + HTTP client + encryption
#define STACK_COMPRESSION       6144
#define STACK_COMMANDS          4096
#define STACK_CONFIG            6144  // Increased to prevent stack overflow
#define STACK_STATISTICS        3072
#define STACK_POWER_REPORT      4096  // Power stats reporting
#define STACK_OTA               10240 // Large for firmware buffer
#define STACK_WATCHDOG          2048

// Queue Sizes
#define QUEUE_SENSOR_DATA_SIZE      10  // 10 sensor samples
#define QUEUE_COMPRESSED_DATA_SIZE  5   // 5 compressed packets
#define QUEUE_COMMAND_SIZE          5   // 5 pending commands

// ============================================
// Data Structures
// ============================================

/**
 * @brief Single sensor sample with timestamp
 */
struct SensorSample {
    uint16_t values[10];        // Register values
    uint32_t timestamp;         // Unix timestamp in milliseconds
    uint8_t registerCount;      // Number of valid registers
    RegID registers[10];        // Register IDs
};

/**
 * @brief Compressed data packet ready for upload
 * CRITICAL: Must be safe for FreeRTOS queue (no heap allocation in queue!)
 */
struct CompressedPacket {
    uint8_t data[512];          // Fixed-size buffer instead of vector!
    size_t dataSize;            // Actual size of compressed data
    uint32_t timestamp;         // When compressed
    size_t sampleCount;         // Number of samples in packet
    size_t uncompressedSize;    // Original data size
    size_t compressedSize;      // Compressed data size
    char compressionMethod[32]; // Compression method used
    RegID registers[16];        // Register IDs that were sampled (NEW)
    size_t registerCount;       // Number of registers per sample (NEW)
};

/**
 * @brief Command to execute
 */
struct Command {
    char commandName[64];       // Command name
    char parameters[256];       // Command parameters
    uint8_t parameterCount;     // Number of parameters
    bool requiresNVSUpdate;     // If command modifies NVS
    uint32_t timestamp;         // When received
};

/**
 * @brief Task statistics for monitoring
 */
struct TaskStats {
    uint32_t executionCount;    // Times executed
    uint32_t totalTimeUs;       // Total execution time
    uint32_t maxTimeUs;         // Worst-case execution time
    uint32_t deadlineMisses;    // Deadline violations
    uint32_t lastRunTime;       // Last execution timestamp
    UBaseType_t stackHighWater; // Minimum free stack
};

// ============================================
// Task Manager Class
// ============================================

class TaskManager {
public:
    /**
     * @brief Initialize FreeRTOS task system
     * @param pollFreqMs Sensor polling frequency in milliseconds
     * @param uploadFreqMs Data upload frequency in milliseconds
     * @param configFreqMs Config check frequency in milliseconds
     * @param commandFreqMs Command poll frequency in milliseconds
     * @param otaFreqMs OTA check frequency in milliseconds
     * @return true if initialization successful
     */
    static bool init(uint32_t pollFreqMs, uint32_t uploadFreqMs, 
                     uint32_t configFreqMs, uint32_t commandFreqMs, uint32_t otaFreqMs);
    
    /**
     * @brief Start all FreeRTOS tasks
     * @param otaManager Pointer to OTA manager instance (optional)
     */
    static void startAllTasks(void* otaManager = nullptr);
    
    /**
     * @brief Suspend all tasks (for OTA or emergency)
     */
    static void suspendAllTasks();
    
    /**
     * @brief Resume all tasks
     */
    static void resumeAllTasks();
    
    /**
     * @brief Update sensor polling frequency
     */
    static void updatePollFrequency(uint32_t newFreqMs);
    
    /**
     * @brief Update upload frequency
     */
    static void updateUploadFrequency(uint32_t newFreqMs);
    
    /**
     * @brief Update config check frequency
     */
    static void updateConfigFrequency(uint32_t newFreqMs);
    
    /**
     * @brief Update command poll frequency
     */
    static void updateCommandFrequency(uint32_t newFreqMs);
    
    /**
     * @brief Update OTA check frequency
     */
    static void updateOtaFrequency(uint32_t newFreqMs);
    
    /**
     * @brief Update power report frequency
     */
    static void updatePowerReportFrequency(uint32_t newFreqMs);
    
    /**
     * @brief Get current configuration frequencies (in milliseconds)
     */
    static uint32_t getConfigFrequency() { return configFrequency; }
    static uint32_t getCommandFrequency() { return commandFrequency; }
    static uint32_t getOtaFrequency() { return otaFrequency; }
    
    /**
     * @brief Get task statistics for monitoring
     */
    static TaskStats getTaskStats(const char* taskName);
    
    /**
     * @brief Print system health report
     */
    static void printSystemHealth();
    
    /**
     * @brief Check if system is healthy (no deadline misses)
     */
    static bool isSystemHealthy();

private:
    // Task entry points
    static void sensorPollTask(void* parameter);
    static void compressionTask(void* parameter);
    static void uploadTask(void* parameter);
    static void commandTask(void* parameter);
    static void configTask(void* parameter);
    static void statisticsTask(void* parameter);
    static void powerReportTask(void* parameter);
    static void otaTask(void* parameter);
    static void watchdogTask(void* parameter);
    
    // Utility functions
    static void recordTaskExecution(TaskStats& stats, uint32_t executionTimeUs);
    static void checkDeadline(const char* taskName, uint32_t executionTimeUs, 
                             uint32_t deadlineUs, TaskStats& stats);
    
    // Task handles
    static TaskHandle_t sensorPollTask_h;
    static TaskHandle_t compressionTask_h;
    static TaskHandle_t uploadTask_h;
    static TaskHandle_t commandTask_h;
    static TaskHandle_t configTask_h;
    static TaskHandle_t statisticsTask_h;
    static TaskHandle_t powerReportTask_h;
    static TaskHandle_t otaTask_h;
    static TaskHandle_t watchdogTask_h;
    
    // Queues
    static QueueHandle_t sensorDataQueue;
    static QueueHandle_t compressedDataQueue;
    static QueueHandle_t commandQueue;
    
    // Synchronization primitives
    static SemaphoreHandle_t nvsAccessMutex;
    static SemaphoreHandle_t wifiClientMutex;
    static SemaphoreHandle_t dataPipelineMutex;
    static SemaphoreHandle_t batchReadySemaphore;  // Signals when compression batch is ready for upload
    
    // Configuration
    static uint32_t pollFrequency;
    static uint32_t uploadFrequency;
    static uint32_t configFrequency;
    static uint32_t commandFrequency;
    static uint32_t otaFrequency;
    static uint32_t powerReportFrequency;
    
    // Statistics
    static TaskStats stats_sensorPoll;
    static TaskStats stats_compression;
    static TaskStats stats_upload;
    static TaskStats stats_command;
    static TaskStats stats_config;
    static TaskStats stats_statistics;
    static TaskStats stats_powerReport;
    static TaskStats stats_ota;
    static TaskStats stats_watchdog;
    
    // System state
    static bool systemInitialized;
    static bool systemSuspended;
    static uint32_t systemStartTime;
};

#endif // TASK_MANAGER_H

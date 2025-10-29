/**
 * @file system_config.h
 * @brief Centralized system configuration constants
 * 
 * This file contains ALL configurable parameters for the ESP32 system.
 * ALL timing, frequency, and system constants should be defined here.
 * 
 * Configuration Hierarchy:
 * 1. Default values (defined here) - Used on first boot
 * 2. NVS stored values - Override defaults after configuration changes
 * 3. Runtime values - Active in-memory configuration
 * 
 * @author Team PowerPort
 * @date 2025-10-29
 */

#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <stdint.h>

// ============================================
// TIMING CONFIGURATION (microseconds)
// ============================================

// Sensor Polling Frequency
// How often to read data from Modbus inverter
// Range: 1 second (1000000 μs) to 1 hour (3600000000 μs)
#define DEFAULT_POLL_FREQUENCY_US       5000000ULL      // 5 seconds
#define MIN_POLL_FREQUENCY_US           1000000ULL      // 1 second
#define MAX_POLL_FREQUENCY_US           3600000000ULL   // 1 hour

// Data Upload Frequency
// How often to upload compressed data to cloud
// Range: 10 seconds to 1 hour
// NOTE: Should be >= pollFrequency for proper batching
#define DEFAULT_UPLOAD_FREQUENCY_US     15000000ULL     // 15 seconds
#define MIN_UPLOAD_FREQUENCY_US         10000000ULL     // 10 seconds
#define MAX_UPLOAD_FREQUENCY_US         3600000000ULL   // 1 hour

// Configuration Check Frequency
// How often to poll server for configuration updates
// Range: 1 second to 5 minutes
#define DEFAULT_CONFIG_FREQUENCY_US     5000000ULL      // 5 seconds
#define MIN_CONFIG_FREQUENCY_US         1000000ULL      // 1 second
#define MAX_CONFIG_FREQUENCY_US         300000000ULL    // 5 minutes

// Command Poll Frequency
// How often to check server for pending commands
// Range: 5 seconds to 5 minutes
#define DEFAULT_COMMAND_FREQUENCY_US    10000000ULL     // 10 seconds
#define MIN_COMMAND_FREQUENCY_US        5000000ULL      // 5 seconds
#define MAX_COMMAND_FREQUENCY_US        300000000ULL    // 5 minutes

// OTA Check Frequency
// How often to check for firmware updates
// Range: 30 seconds to 24 hours
#define DEFAULT_OTA_FREQUENCY_US        6000000000ULL     // 1 minute
#define MIN_OTA_FREQUENCY_US            30000000ULL     // 30 seconds
#define MAX_OTA_FREQUENCY_US            86400000000ULL  // 24 hours

// ============================================
// TASK DEADLINES (microseconds)
// ============================================

// Maximum execution time allowed for each task
// Exceeding deadline triggers warning and increments miss counter
#define SENSOR_POLL_DEADLINE_US         2000000         // 2 seconds (Modbus ~1.8s)
#define COMPRESSION_DEADLINE_US         2000000         // 2 seconds
#define UPLOAD_DEADLINE_US              5000000         // 5 seconds
#define COMMAND_DEADLINE_US             3000000         // 3 seconds (increased to accommodate mutex wait)
#define CONFIG_DEADLINE_US              3000000         // 3 seconds (increased to accommodate mutex wait)
#define OTA_DEADLINE_US                 10000000        // 10 seconds

// ============================================
// WATCHDOG CONFIGURATION
// ============================================

// Hardware Watchdog Timeout (seconds)
// If any registered task doesn't reset watchdog within this time, ESP32 reboots
#define HARDWARE_WATCHDOG_TIMEOUT_S     600             // 10 minutes

// Software Watchdog Thresholds
// Used by watchdog task to monitor other tasks
#define MAX_TASK_IDLE_TIME_MS           120000          // 120 seconds (2 minutes)
#define MAX_DEADLINE_MISSES             20              // Trigger recovery after 20 misses
#define WATCHDOG_CHECK_INTERVAL_MS      30000           // Check every 30 seconds

// ============================================
// MUTEX TIMEOUT CONFIGURATION (milliseconds)
// ============================================

// Data Pipeline Mutex - Protects compression operations
#define DATA_PIPELINE_MUTEX_TIMEOUT_MS  100             // 100ms (fast operations)

// WiFi Client Mutex - Protects HTTP client (shared resource)
// Timeouts MUST be less than task deadlines to avoid deadline misses
#define WIFI_MUTEX_TIMEOUT_UPLOAD_MS    4000            // 4s for upload (< 5s deadline)
#define WIFI_MUTEX_TIMEOUT_COMMAND_MS   2000            // 2s for commands (< 3s deadline)
#define WIFI_MUTEX_TIMEOUT_CONFIG_MS    2000            // 2s for config (< 3s deadline)
#define WIFI_MUTEX_TIMEOUT_OTA_MS       5000            // 5s for OTA check

// NVS Access Mutex - Protects flash writes
// Use portMAX_DELAY - flash writes must complete

// ============================================
// QUEUE CONFIGURATION
// ============================================

// Queue Sizes (number of items)
// Dimensioned to hold multiple upload cycles worth of data
#define QUEUE_SENSOR_DATA_SIZE          10              // 10 sensor samples (~50s @ 5s poll)
#define QUEUE_COMPRESSED_DATA_SIZE      5               // 5 compressed packets (~75s @ 15s upload)
#define QUEUE_COMMAND_SIZE              5               // 5 pending commands

// ============================================
// BUFFER SIZES (bytes)
// ============================================

// Compression/Decompression Buffers
#define COMPRESSION_INPUT_BUFFER        2048            // Raw sensor data buffer
#define COMPRESSION_OUTPUT_BUFFER       512             // Compressed data buffer
#define DECOMPRESSION_OUTPUT_BUFFER     2048            // Decompressed data buffer

// JSON Serialization Buffers
#define JSON_UPLOAD_BUFFER              8192            // Upload payload JSON
#define JSON_COMMAND_BUFFER             1024            // Command response JSON
#define JSON_CONFIG_BUFFER              2048            // Config response JSON

// Base64 Encoding Buffers
#define BASE64_ENCODED_BUFFER           4096            // Base64 output (4/3 * input)
#define BASE64_DECODED_BUFFER           3072            // Base64 decode output

// ============================================
// NETWORK CONFIGURATION
// ============================================

// HTTP Timeouts (milliseconds)
#define HTTP_CONNECT_TIMEOUT_MS         10000           // 10 seconds
#define HTTP_READ_TIMEOUT_MS            10000           // 10 seconds
#define HTTP_UPLOAD_TIMEOUT_MS          15000           // 15 seconds

// HTTP Retry Configuration
#define HTTP_MAX_RETRIES                3               // Retry failed requests 3 times
#define HTTP_RETRY_DELAY_MS             1000            // Wait 1s between retries

// ============================================
// REGISTER CONFIGURATION
// ============================================

// Register Selection Limits
#define MIN_REGISTER_COUNT              3               // Minimum 3 registers
#define MAX_REGISTER_COUNT              10              // Maximum 10 registers
#define DEFAULT_REGISTER_COUNT          10              // Default: all 10 registers

// ============================================
// MODBUS CONFIGURATION
// ============================================

// Modbus Protocol Settings (for Inverter Communication)
#define MODBUS_SLAVE_ADDRESS            0x11            // 17 decimal (inverter address)
#define MODBUS_FUNC_READ                0x03            // Read Holding Registers
#define MODBUS_FUNC_WRITE               0x06            // Write Single Register
#define MODBUS_TIMEOUT_MS               2000            // Modbus read timeout
#define MODBUS_MAX_RETRIES              3               // Maximum retry attempts

// ============================================
// COMPRESSION CONFIGURATION
// ============================================

// Batch Size Calculation
// batchSize = uploadFrequency / pollFrequency
// Example: 15000ms / 5000ms = 3 samples per batch
// NOTE: Calculated dynamically at runtime, not hardcoded!

// Compression Thresholds
#define COMPRESSION_MIN_BATCH_SIZE      1               // Minimum batch size
#define COMPRESSION_MAX_BATCH_SIZE      20              // Maximum batch size (safety limit)

// ============================================
// LOGGING CONFIGURATION
// ============================================

// Log Levels
#define LOG_LEVEL_NONE                  0
#define LOG_LEVEL_ERROR                 1
#define LOG_LEVEL_WARN                  2
#define LOG_LEVEL_INFO                  3
#define LOG_LEVEL_DEBUG                 4
#define LOG_LEVEL_VERBOSE               5

#define DEFAULT_LOG_LEVEL               LOG_LEVEL_INFO

// ============================================
// HEALTH MONITORING
// ============================================

// Health Report Intervals
#define HEALTH_REPORT_INTERVAL_MS       300000          // Print every 5 minutes
#define TELEMETRY_UPLOAD_INTERVAL_MS    3600000         // Upload telemetry every hour

// ============================================
// VALIDATION MACROS
// ============================================

// Validate frequency is within allowed range
#define IS_VALID_POLL_FREQ(f)    ((f) >= MIN_POLL_FREQUENCY_US && (f) <= MAX_POLL_FREQUENCY_US)
#define IS_VALID_UPLOAD_FREQ(f)  ((f) >= MIN_UPLOAD_FREQUENCY_US && (f) <= MAX_UPLOAD_FREQUENCY_US)
#define IS_VALID_CONFIG_FREQ(f)  ((f) >= MIN_CONFIG_FREQUENCY_US && (f) <= MAX_CONFIG_FREQUENCY_US)
#define IS_VALID_COMMAND_FREQ(f) ((f) >= MIN_COMMAND_FREQUENCY_US && (f) <= MAX_COMMAND_FREQUENCY_US)
#define IS_VALID_OTA_FREQ(f)     ((f) >= MIN_OTA_FREQUENCY_US && (f) <= MAX_OTA_FREQUENCY_US)

// Validate register count
#define IS_VALID_REGISTER_COUNT(c) ((c) >= MIN_REGISTER_COUNT && (c) <= MAX_REGISTER_COUNT)

#endif // SYSTEM_CONFIG_H

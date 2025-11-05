#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

// ============================================
// Log Levels
// ============================================
enum LogLevel {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_NONE = 4  // Disable all logging
};

// ============================================
// Module Tags
// ============================================
#define LOG_TAG_BOOT        "BOOT"
#define LOG_TAG_WIFI        "WIFI"
#define LOG_TAG_TASK        "TASK"
#define LOG_TAG_SENSOR      "SENSOR"
#define LOG_TAG_COMPRESS    "COMPRESS"
#define LOG_TAG_DATA        "DATA"
#define LOG_TAG_UPLOAD      "UPLOAD"
#define LOG_TAG_COMMAND     "COMMAND"
#define LOG_TAG_CONFIG      "CONFIG"
#define LOG_TAG_FOTA        "FOTA"
#define LOG_TAG_SECURITY    "SECURITY"
#define LOG_TAG_POWER       "POWER"
#define LOG_TAG_NVS         "NVS"
#define LOG_TAG_DIAG        "DIAG"
#define LOG_TAG_STATS       "STATS"
#define LOG_TAG_FAULT       "FAULT"
#define LOG_TAG_MODBUS      "MODBUS"
#define LOG_TAG_BUFFER      "BUFFER"
#define LOG_TAG_WATCHDOG    "WATCHDOG"

// ============================================
// Global Log Level Control
// ============================================
extern LogLevel g_logLevel;

// Function to set log level at runtime
inline void setLogLevel(LogLevel level) {
    g_logLevel = level;
}

inline LogLevel getLogLevel() {
    return g_logLevel;
}

// ============================================
// Timestamp Helper
// ============================================
inline void _printTimestamp() {
    struct tm timeinfo;
    // Try to get real time from NTP (only if WiFi is connected)
    if (WiFi.status() == WL_CONNECTED && getLocalTime(&timeinfo, 0)) {
        // Use real time in Sri Lankan timezone (UTC+5:30)
        // getLocalTime already accounts for timezone if configured
        Serial.printf("[%02d:%02d:%02d]", 
                     timeinfo.tm_hour, 
                     timeinfo.tm_min, 
                     timeinfo.tm_sec);
    } else {
        // Fallback to millis if WiFi not connected or NTP not synced
        unsigned long ms = millis();
        unsigned long seconds = ms / 1000;
        unsigned long minutes = seconds / 60;
        unsigned long hours = minutes / 60;
        seconds %= 60;
        minutes %= 60;
        hours %= 24;
        
        Serial.printf("[%02lu:%02lu:%02lu]", hours, minutes, seconds);
    }
}

// ============================================
// Core Logging Macros
// ============================================

// Internal logging macro - don't use directly
#define _LOG(level, tag, symbol, format, ...) \
    do { \
        if (g_logLevel <= level) { \
            _printTimestamp(); \
            Serial.printf(" [%-10s] %s " format "\n", tag, symbol, ##__VA_ARGS__); \
        } \
    } while(0)

// Public logging macros
#define LOG_DEBUG(tag, format, ...)   _LOG(LOG_LEVEL_DEBUG, tag, "    ", format, ##__VA_ARGS__)
#define LOG_INFO(tag, format, ...)    _LOG(LOG_LEVEL_INFO,  tag, "    ", format, ##__VA_ARGS__)
#define LOG_WARN(tag, format, ...)    _LOG(LOG_LEVEL_WARN,  tag, "[!] ", format, ##__VA_ARGS__)
#define LOG_ERROR(tag, format, ...)   _LOG(LOG_LEVEL_ERROR, tag, "✗   ", format, ##__VA_ARGS__)

// Special success logging (always shows with ✓ when level allows)
#define LOG_SUCCESS(tag, format, ...) \
    do { \
        if (g_logLevel <= LOG_LEVEL_INFO) { \
            _printTimestamp(); \
            Serial.printf(" [%-10s] ✓   " format "\n", tag, ##__VA_ARGS__); \
        } \
    } while(0)

// ============================================
// Section Headers (always visible if INFO or below)
// ============================================
// Section header macro for major operations
#define LOG_SECTION(title) \
    do { \
        if (g_logLevel <= LOG_LEVEL_INFO) { \
            _printTimestamp(); \
            Serial.printf(" ═══════════════════════════════════════════\n"); \
            _printTimestamp(); \
            Serial.printf(" %s\n", title); \
            _printTimestamp(); \
            Serial.printf(" ═══════════════════════════════════════════\n"); \
        } \
    } while(0)

#define LOG_SUBSECTION(title) \
    do { \
        if (g_logLevel <= LOG_LEVEL_INFO) { \
            Serial.println("┌────────────────────────────────────────────────────────────┐"); \
            Serial.printf("│  %s\n", title); \
            Serial.println("└────────────────────────────────────────────────────────────┘"); \
        } \
    } while(0)

#define LOG_DIVIDER() \
    do { \
        if (g_logLevel <= LOG_LEVEL_INFO) { \
            Serial.println("────────────────────────────────────────────────────────────"); \
        } \
    } while(0)

// ============================================
// Initialization
// ============================================
// Initialize logger - uses g_logLevel from logger.cpp (change log level there)
inline void initLogger() {
    Serial.begin(115200);
    delay(100);  // Allow serial to stabilize
    Serial.println();
    Serial.println("╔════════════════════════════════════════════════════════════╗");
    Serial.println("║              EcoWatt Logger Initialized                    ║");
    Serial.println("╚════════════════════════════════════════════════════════════╝");
    Serial.printf("Log Level: %d (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=NONE)\n\n", g_logLevel);
}

#endif // LOGGER_H

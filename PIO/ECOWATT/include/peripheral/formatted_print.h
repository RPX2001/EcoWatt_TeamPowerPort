#ifndef FORMATTED_PRINT_H
#define FORMATTED_PRINT_H

#include "driver/debug.h"
#include <Arduino.h>

// Color codes for ESP32 serial monitor (ANSI escape codes)
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_BOLD    "\033[1m"

// Box drawing characters
#define BOX_H "═"
#define BOX_V "║"
#define BOX_TL "╔"
#define BOX_TR "╗"
#define BOX_BL "╚"
#define BOX_BR "╝"

// Formatted printing macros
#define print(...) debug.log(__VA_ARGS__)
#define print_init() debug.init()

// Section headers
#define PRINT_SECTION(title) \
    do { \
        Serial.println(); \
        Serial.println("╔════════════════════════════════════════════════════════════╗"); \
        Serial.printf("║  %-56s  ║\n", title); \
        Serial.println("╚════════════════════════════════════════════════════════════╝"); \
    } while(0)

#define PRINT_SUBSECTION(title) \
    do { \
        Serial.println("┌────────────────────────────────────────────────────────────┐"); \
        Serial.printf("│  %s\n", title); \
        Serial.println("└────────────────────────────────────────────────────────────┘"); \
    } while(0)

// Status indicators (no emojis)
#define PRINT_SUCCESS(msg, ...) \
    do { \
        Serial.printf("  [OK] " msg "\n", ##__VA_ARGS__); \
    } while(0)

#define PRINT_ERROR(msg, ...) \
    do { \
        Serial.printf("  [ERROR] " msg "\n", ##__VA_ARGS__); \
    } while(0)

#define PRINT_WARNING(msg, ...) \
    do { \
        Serial.printf("  [WARN] " msg "\n", ##__VA_ARGS__); \
    } while(0)

#define PRINT_INFO(msg, ...) \
    do { \
        Serial.printf("  [INFO] " msg "\n", ##__VA_ARGS__); \
    } while(0)

#define PRINT_PROGRESS(msg, ...) \
    do { \
        Serial.printf("  [....] " msg "\n", ##__VA_ARGS__); \
    } while(0)

#define PRINT_COMMAND(msg, ...) \
    do { \
        Serial.printf("  [CMD] " msg "\n", ##__VA_ARGS__); \
    } while(0)

#define PRINT_DATA(key, value, ...) \
    do { \
        Serial.printf("     • %-20s: " value "\n", key, ##__VA_ARGS__); \
    } while(0)

// Divider lines
#define PRINT_DIVIDER() \
    Serial.println("────────────────────────────────────────────────────────────")

#define PRINT_SEPARATOR() \
    Serial.println()

// Box messages
#define PRINT_BOX_START(title) \
    do { \
        Serial.println(); \
        Serial.println("╔════════════════════════════════════════════════════════════╗"); \
        Serial.printf("║  %-56s  ║\n", title); \
        Serial.println("╠════════════════════════════════════════════════════════════╣"); \
    } while(0)

#define PRINT_BOX_LINE(msg, ...) \
    do { \
        char buf[60]; \
        snprintf(buf, sizeof(buf), msg, ##__VA_ARGS__); \
        Serial.printf("║  %-56s  ║\n", buf); \
    } while(0)

#define PRINT_BOX_END() \
    do { \
        Serial.println("╚════════════════════════════════════════════════════════════╝"); \
        Serial.println(); \
    } while(0)

// Timestamp formatting
#define PRINT_TIMESTAMP() \
    do { \
        unsigned long ms = millis(); \
        unsigned long secs = ms / 1000; \
        unsigned long mins = secs / 60; \
        unsigned long hours = mins / 60; \
        Serial.printf("[%02lu:%02lu:%02lu.%03lu] ", \
            hours % 24, mins % 60, secs % 60, ms % 1000); \
    } while(0)

#define PRINT_TIME(msg, ...) \
    do { \
        PRINT_TIMESTAMP(); \
        Serial.printf(msg "\n", ##__VA_ARGS__); \
    } while(0)

#endif // FORMATTED_PRINT_H

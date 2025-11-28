/**
 * @file statistics_manager.h
 * @brief Performance statistics tracking and reporting for compression operations
 * 
 * This module provides centralized management of compression performance metrics,
 * including method selection tracking, compression ratios, timing, and quality distribution.
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#ifndef STATISTICS_MANAGER_H
#define STATISTICS_MANAGER_H

#include <Arduino.h>
#include "compression_benchmark.h"

/**
 * @class StatisticsManager
 * @brief Manages performance statistics for compression operations
 * 
 * This class provides a singleton-style interface for tracking and reporting
 * compression performance metrics across the entire system lifetime.
 */
class StatisticsManager {
public:
    /**
     * @brief Initialize the statistics manager
     * 
     * Resets all counters and prepares the system for tracking
     */
    static void init();

    /**
     * @brief Update compression statistics after each compression operation
     * 
     * This function updates running averages, best/worst ratios, quality distribution,
     * and timing metrics.
     * 
     * @param method Name of the compression method used (e.g., "DICTIONARY", "DELTA")
     * @param academicRatio Compression ratio (compressed/original, lower is better)
     * @param timeUs Time taken for compression in microseconds
     * @return true if update successful, false if invalid parameters
     */
    static bool updateCompressionStats(const char* method, float academicRatio, unsigned long timeUs);

    /**
     * @brief Update method-specific usage counter
     * 
     * @param method Name of the method to increment ("DICTIONARY", "TEMPORAL", "SEMANTIC", "BITPACK")
     */
    static void incrementMethodUsage(const char* method);

    /**
     * @brief Record a successful lossless compression
     */
    static void recordLosslessSuccess();

    /**
     * @brief Record a compression failure
     */
    static void recordCompressionFailure();

    /**
     * @brief Print comprehensive performance report to serial
     * 
     * Displays all statistics including averages, best/worst ratios,
     * quality distribution, method usage, and timing metrics.
     */
    static void printPerformanceReport();

    /**
     * @brief Print a compact performance summary (one line)
     */
    static void printCompactSummary();

    /**
     * @brief Get read-only access to current statistics
     * 
     * @return Reference to the statistics structure
     */
    static const SmartPerformanceStats& getStats();

    /**
     * @brief Reset all statistics to initial values
     */
    static void reset();

    /**
     * @brief Get average compression time in microseconds
     * 
     * @return Average time, or 0 if no compressions performed
     */
    static unsigned long getAverageCompressionTime();

    /**
     * @brief Get the current optimal method name
     * 
     * @return Pointer to method name string
     */
    static const char* getOptimalMethod();

    /**
     * @brief Get compression success rate as percentage
     * 
     * @return Success rate (0.0 to 100.0), or 100.0 if no failures
     */
    static float getSuccessRate();

    /**
     * @brief Check if statistics have been initialized
     * 
     * @return true if init() has been called
     */
    static bool isInitialized();

private:
    static SmartPerformanceStats stats;
    static bool initialized;

    // Prevent instantiation
    StatisticsManager() = delete;
    ~StatisticsManager() = delete;
    StatisticsManager(const StatisticsManager&) = delete;
    StatisticsManager& operator=(const StatisticsManager&) = delete;
};

#endif // STATISTICS_MANAGER_H

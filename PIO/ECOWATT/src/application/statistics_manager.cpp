/**
 * @file statistics_manager.cpp
 * @brief Implementation of performance statistics tracking
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#include "application/statistics_manager.h"
#include "peripheral/logger.h"
#include <string.h>

// Initialize static members
SmartPerformanceStats StatisticsManager::stats;
bool StatisticsManager::initialized = false;

void StatisticsManager::init() {
    // Reset all statistics
    memset(&stats, 0, sizeof(SmartPerformanceStats));
    
    // Initialize defaults
    stats.bestAcademicRatio = 1.0f;
    stats.worstAcademicRatio = 0.0f;
    stats.fastestCompressionTime = ULONG_MAX;
    stats.slowestCompressionTime = 0;
    strcpy(stats.currentOptimalMethod, "DICTIONARY");
    
    initialized = true;
    LOG_INFO(LOG_TAG_STATS, "Initialized");;
}

bool StatisticsManager::updateCompressionStats(const char* method, float academicRatio, 
                                               unsigned long timeUs) {
    if (!initialized) {
        LOG_INFO(LOG_TAG_STATS, "WARNING: Not initialized, initializing now");;
        init();
    }
    
    // Bounds checking to prevent invalid data
    if (academicRatio < 0.0f || academicRatio > 10.0f) {
        LOG_INFO(LOG_TAG_STATS, "WARNING: Invalid academic ratio: %.3f, skipping update\n", academicRatio);
        return false;
    }
    
    if (timeUs == 0 || timeUs > 10000000UL) { // 10 seconds max
        LOG_INFO(LOG_TAG_STATS, "WARNING: Invalid compression time: %lu μs, skipping update\n", timeUs);
        return false;
    }
    
    // Update counters
    stats.totalSmartCompressions++;
    stats.totalCompressionTime += timeUs;
    
    // Running average formula: new_avg = (old_avg * (n-1) + new_value) / n
    // This is mathematically correct for incremental mean calculation
    if (stats.totalSmartCompressions > 0) {
        stats.averageAcademicRatio = 
            (stats.averageAcademicRatio * (stats.totalSmartCompressions - 1) + academicRatio) / 
            stats.totalSmartCompressions;
    } else {
        stats.averageAcademicRatio = academicRatio;
    }
    
    // Update best ratio if this is better (lower is better)
    if (academicRatio < stats.bestAcademicRatio) {
        stats.bestAcademicRatio = academicRatio;
        strncpy(stats.currentOptimalMethod, method, sizeof(stats.currentOptimalMethod) - 1);
        stats.currentOptimalMethod[sizeof(stats.currentOptimalMethod) - 1] = '\0';
    }
    
    // Track worst ratio (for debugging/monitoring)
    if (stats.totalSmartCompressions == 1) {
        stats.worstAcademicRatio = academicRatio;
    } else if (academicRatio > stats.worstAcademicRatio) {
        stats.worstAcademicRatio = academicRatio;
    }
    
    // Update quality distribution
    if (academicRatio <= 0.5f) {
        stats.excellentCompressionCount++;
    } else if (academicRatio <= 0.67f) {
        stats.goodCompressionCount++;
    } else if (academicRatio <= 0.91f) {
        stats.fairCompressionCount++;
    } else {
        stats.poorCompressionCount++;
    }
    
    // Update fastest compression time
    if (stats.fastestCompressionTime == 0 || timeUs < stats.fastestCompressionTime) {
        stats.fastestCompressionTime = timeUs;
    }
    
    // Track slowest compression time
    if (timeUs > stats.slowestCompressionTime) {
        stats.slowestCompressionTime = timeUs;
    }
    
    return true;
}

void StatisticsManager::incrementMethodUsage(const char* method) {
    if (!initialized) return;
    
    if (strcmp(method, "DICTIONARY") == 0 || strstr(method, "DICT") != nullptr) {
        stats.dictionaryUsed++;
    } else if (strcmp(method, "TEMPORAL") == 0 || strstr(method, "DELTA") != nullptr) {
        stats.temporalUsed++;
    } else if (strcmp(method, "SEMANTIC") == 0) {
        stats.semanticUsed++;
    } else if (strcmp(method, "BITPACK") == 0 || strstr(method, "RLE") != nullptr) {
        stats.bitpackUsed++;
    }
}

void StatisticsManager::recordLosslessSuccess() {
    if (initialized) {
        stats.losslessSuccesses++;
    }
}

void StatisticsManager::recordCompressionFailure() {
    if (initialized) {
        stats.compressionFailures++;
    }
}

void StatisticsManager::printPerformanceReport() {
    if (!initialized) {
        LOG_INFO(LOG_TAG_STATS, "Not initialized");;
        return;
    }
    
    LOG_INFO(LOG_TAG_STATS, "");;
    LOG_INFO(LOG_TAG_STATS, "========================================");;
    LOG_INFO(LOG_TAG_STATS, "  COMPRESSION PERFORMANCE STATISTICS");;
    LOG_INFO(LOG_TAG_STATS, "========================================");;
    
    // Overall Statistics
    LOG_INFO(LOG_TAG_STATS, "\n📊 OVERALL METRICS:");;
    LOG_INFO(LOG_TAG_STATS, "  Total Compressions:  %lu\n", stats.totalSmartCompressions);
    LOG_INFO(LOG_TAG_STATS, "  Total Time:          %lu μs (%.2f ms)\n", 
          stats.totalCompressionTime, stats.totalCompressionTime / 1000.0f);
    
    if (stats.totalSmartCompressions > 0) {
        unsigned long avgTime = stats.totalCompressionTime / stats.totalSmartCompressions;
        LOG_INFO(LOG_TAG_STATS, "  Average Time:        %lu μs (%.2f ms)\n", avgTime, avgTime / 1000.0f);
        LOG_INFO(LOG_TAG_STATS, "  Average Ratio:       %.4f (%.1f%% savings)\n", 
              stats.averageAcademicRatio, (1.0f - stats.averageAcademicRatio) * 100);
    }
    
    // Best/Worst Performance
    LOG_INFO(LOG_TAG_STATS, "\n🏆 PERFORMANCE RANGE:");;
    LOG_INFO(LOG_TAG_STATS, "  Best Ratio:          %.4f (Method: %s)\n", 
          stats.bestAcademicRatio, stats.currentOptimalMethod);
    LOG_INFO(LOG_TAG_STATS, "  Worst Ratio:         %.4f\n", stats.worstAcademicRatio);
    LOG_INFO(LOG_TAG_STATS, "  Fastest Time:        %lu μs\n", 
          stats.fastestCompressionTime == ULONG_MAX ? 0 : stats.fastestCompressionTime);
    LOG_INFO(LOG_TAG_STATS, "  Slowest Time:        %lu μs\n", stats.slowestCompressionTime);
    
    // Quality Distribution
    LOG_INFO(LOG_TAG_STATS, "\n📈 QUALITY DISTRIBUTION:");;
    LOG_INFO(LOG_TAG_STATS, "  Excellent (≤50%%):    %lu compressions\n", stats.excellentCompressionCount);
    LOG_INFO(LOG_TAG_STATS, "  Good (≤67%%):         %lu compressions\n", stats.goodCompressionCount);
    LOG_INFO(LOG_TAG_STATS, "  Fair (≤91%%):         %lu compressions\n", stats.fairCompressionCount);
    LOG_INFO(LOG_TAG_STATS, "  Poor (>91%%):         %lu compressions\n", stats.poorCompressionCount);
    
    // Method Usage
    LOG_INFO(LOG_TAG_STATS, "\n🔧 METHOD USAGE:");;
    LOG_INFO(LOG_TAG_STATS, "  Dictionary:          %lu times\n", stats.dictionaryUsed);
    LOG_INFO(LOG_TAG_STATS, "  Temporal/Delta:      %lu times\n", stats.temporalUsed);
    LOG_INFO(LOG_TAG_STATS, "  Semantic:            %lu times\n", stats.semanticUsed);
    LOG_INFO(LOG_TAG_STATS, "  Bitpack/RLE:         %lu times\n", stats.bitpackUsed);
    
    // Success Rate
    LOG_INFO(LOG_TAG_STATS, "\n✅ RELIABILITY:");;
    LOG_INFO(LOG_TAG_STATS, "  Lossless Successes:  %lu\n", stats.losslessSuccesses);
    LOG_INFO(LOG_TAG_STATS, "  Failures:            %lu\n", stats.compressionFailures);
    
    unsigned long total = stats.losslessSuccesses + stats.compressionFailures;
    if (total > 0) {
        float successRate = (stats.losslessSuccesses * 100.0f) / total;
        LOG_INFO(LOG_TAG_STATS, "  Success Rate:        %.2f%%\n", successRate);
    }
    
    LOG_INFO(LOG_TAG_STATS, "========================================");;;
}

void StatisticsManager::printCompactSummary() {
    if (!initialized || stats.totalSmartCompressions == 0) {
        LOG_INFO(LOG_TAG_STATS, "[Stats] No compressions yet");;
        return;
    }
    
    unsigned long avgTime = stats.totalCompressionTime / stats.totalSmartCompressions;
    float savings = (1.0f - stats.averageAcademicRatio) * 100;
    
    LOG_INFO(LOG_TAG_STATS, "[Stats] Compressions: %lu | Avg: %.1f%% savings in %lu μs | Best: %s (%.1f%% savings)\n",
          stats.totalSmartCompressions, savings, avgTime, 
          stats.currentOptimalMethod, (1.0f - stats.bestAcademicRatio) * 100);
}

const SmartPerformanceStats& StatisticsManager::getStats() {
    if (!initialized) {
        init();
    }
    return stats;
}

void StatisticsManager::reset() {
    LOG_INFO(LOG_TAG_STATS, "Resetting all statistics");;
    init(); // Re-initialize to reset values
}

unsigned long StatisticsManager::getAverageCompressionTime() {
    if (!initialized || stats.totalSmartCompressions == 0) {
        return 0;
    }
    return stats.totalCompressionTime / stats.totalSmartCompressions;
}

const char* StatisticsManager::getOptimalMethod() {
    return stats.currentOptimalMethod;
}

float StatisticsManager::getSuccessRate() {
    if (!initialized) return 100.0f;
    
    unsigned long total = stats.losslessSuccesses + stats.compressionFailures;
    if (total == 0) return 100.0f;
    
    return (stats.losslessSuccesses * 100.0f) / total;
}

bool StatisticsManager::isInitialized() {
    return initialized;
}

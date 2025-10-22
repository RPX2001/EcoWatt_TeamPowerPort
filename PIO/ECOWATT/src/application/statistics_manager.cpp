/**
 * @file statistics_manager.cpp
 * @brief Implementation of performance statistics tracking
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#include "application/statistics_manager.h"
#include "peripheral/print.h"
#include "peripheral/formatted_print.h"
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
    print("[StatisticsManager] Initialized\n");
}

bool StatisticsManager::updateCompressionStats(const char* method, float academicRatio, 
                                               unsigned long timeUs) {
    if (!initialized) {
        print("[StatisticsManager] WARNING: Not initialized, initializing now\n");
        init();
    }
    
    // Bounds checking to prevent invalid data
    if (academicRatio < 0.0f || academicRatio > 10.0f) {
        print("[StatisticsManager] WARNING: Invalid academic ratio: %.3f, skipping update\n", academicRatio);
        return false;
    }
    
    if (timeUs == 0 || timeUs > 10000000UL) { // 10 seconds max
        print("[StatisticsManager] WARNING: Invalid compression time: %lu μs, skipping update\n", timeUs);
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
        print("[StatisticsManager] Not initialized\n");
        return;
    }
    
    print("\n");
    print("========================================\n");
    print("  COMPRESSION PERFORMANCE STATISTICS\n");
    print("========================================\n");
    
    // Overall Statistics
    print("\n📊 OVERALL METRICS:\n");
    print("  Total Compressions:  %lu\n", stats.totalSmartCompressions);
    print("  Total Time:          %lu μs (%.2f ms)\n", 
          stats.totalCompressionTime, stats.totalCompressionTime / 1000.0f);
    
    if (stats.totalSmartCompressions > 0) {
        unsigned long avgTime = stats.totalCompressionTime / stats.totalSmartCompressions;
        print("  Average Time:        %lu μs (%.2f ms)\n", avgTime, avgTime / 1000.0f);
        print("  Average Ratio:       %.4f (%.1f%% savings)\n", 
              stats.averageAcademicRatio, (1.0f - stats.averageAcademicRatio) * 100);
    }
    
    // Best/Worst Performance
    print("\n🏆 PERFORMANCE RANGE:\n");
    print("  Best Ratio:          %.4f (Method: %s)\n", 
          stats.bestAcademicRatio, stats.currentOptimalMethod);
    print("  Worst Ratio:         %.4f\n", stats.worstAcademicRatio);
    print("  Fastest Time:        %lu μs\n", 
          stats.fastestCompressionTime == ULONG_MAX ? 0 : stats.fastestCompressionTime);
    print("  Slowest Time:        %lu μs\n", stats.slowestCompressionTime);
    
    // Quality Distribution
    print("\n📈 QUALITY DISTRIBUTION:\n");
    print("  Excellent (≤50%%):    %lu compressions\n", stats.excellentCompressionCount);
    print("  Good (≤67%%):         %lu compressions\n", stats.goodCompressionCount);
    print("  Fair (≤91%%):         %lu compressions\n", stats.fairCompressionCount);
    print("  Poor (>91%%):         %lu compressions\n", stats.poorCompressionCount);
    
    // Method Usage
    print("\n🔧 METHOD USAGE:\n");
    print("  Dictionary:          %lu times\n", stats.dictionaryUsed);
    print("  Temporal/Delta:      %lu times\n", stats.temporalUsed);
    print("  Semantic:            %lu times\n", stats.semanticUsed);
    print("  Bitpack/RLE:         %lu times\n", stats.bitpackUsed);
    
    // Success Rate
    print("\n✅ RELIABILITY:\n");
    print("  Lossless Successes:  %lu\n", stats.losslessSuccesses);
    print("  Failures:            %lu\n", stats.compressionFailures);
    
    unsigned long total = stats.losslessSuccesses + stats.compressionFailures;
    if (total > 0) {
        float successRate = (stats.losslessSuccesses * 100.0f) / total;
        print("  Success Rate:        %.2f%%\n", successRate);
    }
    
    print("========================================\n\n");
}

void StatisticsManager::printCompactSummary() {
    if (!initialized || stats.totalSmartCompressions == 0) {
        print("[Stats] No compressions yet\n");
        return;
    }
    
    unsigned long avgTime = stats.totalCompressionTime / stats.totalSmartCompressions;
    float savings = (1.0f - stats.averageAcademicRatio) * 100;
    
    print("[Stats] Compressions: %lu | Avg: %.1f%% savings in %lu μs | Best: %s (%.1f%% savings)\n",
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
    print("[StatisticsManager] Resetting all statistics\n");
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

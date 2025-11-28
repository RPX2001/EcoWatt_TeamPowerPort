/**
 * @file aggregation.h
 * @brief Data Aggregation Module for EcoWatt Device
 * 
 * Provides optional aggregation (min/avg/max) per register to reduce
 * payload size when compression alone isn't sufficient.
 */

#ifndef AGGREGATION_H
#define AGGREGATION_H

#include <Arduino.h>
#include "peripheral/acquisition.h"

// Configuration options for aggregation
#define AGGREGATION_WINDOW 5          // Number of samples to aggregate (default: 5)
#define AGGREGATION_THRESHOLD 512     // Enable if payload > X bytes (default: 512)

// Aggregation modes
enum AggregationMode {
    AGG_DISABLED = 0,    // No aggregation
    AGG_MIN_MAX = 1,     // Only min and max
    AGG_FULL = 2         // Min, avg, and max
};

// Aggregated sample structure
struct AggregatedSample {
    uint16_t min[REGISTER_COUNT];         // Minimum value per register
    uint16_t avg[REGISTER_COUNT];         // Average value per register
    uint16_t max[REGISTER_COUNT];         // Maximum value per register
    uint32_t timestampStart;              // First sample timestamp
    uint32_t timestampEnd;                // Last sample timestamp
    uint8_t sampleCount;                  // Number of samples aggregated
    RegID registers[REGISTER_COUNT];      // Register selection
    uint8_t registerCount;                // Number of registers
    AggregationMode mode;                 // Aggregation mode used
    
    AggregatedSample() : timestampStart(0), timestampEnd(0), 
                        sampleCount(0), registerCount(0), 
                        mode(AGG_DISABLED) {
        memset(min, 0, sizeof(min));
        memset(avg, 0, sizeof(avg));
        memset(max, 0, sizeof(max));
        memset(registers, 0, sizeof(registers));
    }
};

// Aggregation engine class
class Aggregation {
private:
    static AggregationMode currentMode;
    static uint16_t aggregationWindow;
    static uint16_t payloadThreshold;

public:
    // Initialize aggregation system
    static void init();
    
    // Set aggregation configuration
    static void setMode(AggregationMode mode);
    static void setWindow(uint16_t window);
    static void setThreshold(uint16_t threshold);
    
    // Get current configuration
    static AggregationMode getMode() { return currentMode; }
    static uint16_t getWindow() { return aggregationWindow; }
    static uint16_t getThreshold() { return payloadThreshold; }
    
    // Aggregate multiple samples into one
    static AggregatedSample aggregateSamples(
        uint16_t samples[][REGISTER_COUNT],
        size_t sampleCount,
        const RegID* registerSelection,
        size_t registerCount,
        uint32_t* timestamps
    );
    
    // Check if aggregation should be used
    static bool shouldUseAggregation(size_t payloadSize);
    
    // Calculate aggregated sample size
    static size_t calculateAggregatedSize(
        AggregationMode mode,
        size_t registerCount
    );
    
    // Convert aggregated sample to binary format
    static std::vector<uint8_t> serializeAggregated(
        const AggregatedSample& sample
    );
    
    // Deserialize aggregated sample from binary
    static AggregatedSample deserializeAggregated(
        const std::vector<uint8_t>& data
    );
    
    // Calculate reduction ratio
    static float getReductionRatio(
        size_t originalSampleCount,
        size_t registerCount
    );
};

#endif // AGGREGATION_H

/**
 * @file aggregation.cpp
 * @brief Implementation of Data Aggregation Module
 */

#include "application/aggregation.h"
#include "peripheral/logger.h"
#include <vector>

// Initialize static members
AggregationMode Aggregation::currentMode = AGG_DISABLED;
uint16_t Aggregation::aggregationWindow = AGGREGATION_WINDOW;
uint16_t Aggregation::payloadThreshold = AGGREGATION_THRESHOLD;

void Aggregation::init() {
    LOG_INFO(LOG_TAG_DATA, "Initializing...");;
    LOG_INFO(LOG_TAG_DATA, "Mode=%d, Window=%u, Threshold=%u bytes\n",
          currentMode, aggregationWindow, payloadThreshold);
}

void Aggregation::setMode(AggregationMode mode) {
    currentMode = mode;
    LOG_INFO(LOG_TAG_DATA, "Mode set to %d\n", mode);
}

void Aggregation::setWindow(uint16_t window) {
    if (window > 0 && window <= 20) {
        aggregationWindow = window;
        LOG_INFO(LOG_TAG_DATA, "Window set to %u samples\n", window);
    } else {
        LOG_INFO(LOG_TAG_DATA, "Invalid window %u (must be 1-20)\n", window);
    }
}

void Aggregation::setThreshold(uint16_t threshold) {
    payloadThreshold = threshold;
    LOG_INFO(LOG_TAG_DATA, "Threshold set to %u bytes\n", threshold);
}

AggregatedSample Aggregation::aggregateSamples(
    uint16_t samples[][REGISTER_COUNT],
    size_t sampleCount,
    const RegID* registerSelection,
    size_t registerCount,
    uint32_t* timestamps
) {
    AggregatedSample result;
    
    if (sampleCount == 0 || registerCount == 0) {
        LOG_INFO(LOG_TAG_DATA, "Invalid parameters (samples=%zu, regs=%zu)\n",
              sampleCount, registerCount);
        return result;
    }
    
    // Initialize result
    result.sampleCount = sampleCount;
    result.registerCount = registerCount;
    result.mode = currentMode;
    result.timestampStart = timestamps[0];
    result.timestampEnd = timestamps[sampleCount - 1];
    
    // Copy register selection
    memcpy(result.registers, registerSelection, 
           registerCount * sizeof(RegID));
    
    // Initialize min/max arrays
    for (size_t r = 0; r < registerCount; r++) {
        result.min[r] = 0xFFFF;  // Max uint16_t
        result.max[r] = 0;
        result.avg[r] = 0;
    }
    
    // Calculate aggregations
    for (size_t r = 0; r < registerCount; r++) {
        uint32_t sum = 0;
        
        for (size_t s = 0; s < sampleCount; s++) {
            uint16_t value = samples[s][r];
            
            // Update min
            if (value < result.min[r]) {
                result.min[r] = value;
            }
            
            // Update max
            if (value > result.max[r]) {
                result.max[r] = value;
            }
            
            // Sum for average
            sum += value;
        }
        
        // Calculate average
        result.avg[r] = (uint16_t)(sum / sampleCount);
    }
    
    LOG_INFO(LOG_TAG_DATA, "Aggregated %zu samples, %zu registers\n",
          sampleCount, registerCount);
    LOG_INFO(LOG_TAG_DATA, "Time span: %u ms to %u ms\n",
          result.timestampStart, result.timestampEnd);
    
    return result;
}

bool Aggregation::shouldUseAggregation(size_t payloadSize) {
    if (currentMode == AGG_DISABLED) {
        return false;
    }
    
    bool shouldUse = (payloadSize > payloadThreshold);
    
    if (shouldUse) {
        LOG_INFO(LOG_TAG_DATA, "Payload %zu bytes > threshold %u, using aggregation\n",
              payloadSize, payloadThreshold);
    }
    
    return shouldUse;
}

size_t Aggregation::calculateAggregatedSize(
    AggregationMode mode,
    size_t registerCount
) {
    size_t baseSize = sizeof(AggregatedSample);
    
    if (mode == AGG_MIN_MAX) {
        // Only min and max arrays
        return 8 + (registerCount * 2 * sizeof(uint16_t)); // timestamps + 2 arrays
    } else if (mode == AGG_FULL) {
        // Min, avg, and max arrays
        return 8 + (registerCount * 3 * sizeof(uint16_t)); // timestamps + 3 arrays
    }
    
    return 0;
}

std::vector<uint8_t> Aggregation::serializeAggregated(
    const AggregatedSample& sample
) {
    std::vector<uint8_t> data;
    
    // Header: AGG marker (1 byte) + mode (1 byte) + sample count (1 byte) + reg count (1 byte)
    data.push_back(0xAA); // Aggregation marker
    data.push_back((uint8_t)sample.mode);
    data.push_back(sample.sampleCount);
    data.push_back(sample.registerCount);
    
    // Timestamps (8 bytes)
    data.push_back((sample.timestampStart >> 24) & 0xFF);
    data.push_back((sample.timestampStart >> 16) & 0xFF);
    data.push_back((sample.timestampStart >> 8) & 0xFF);
    data.push_back(sample.timestampStart & 0xFF);
    
    data.push_back((sample.timestampEnd >> 24) & 0xFF);
    data.push_back((sample.timestampEnd >> 16) & 0xFF);
    data.push_back((sample.timestampEnd >> 8) & 0xFF);
    data.push_back(sample.timestampEnd & 0xFF);
    
    // Register IDs
    for (size_t i = 0; i < sample.registerCount; i++) {
        data.push_back((uint8_t)sample.registers[i]);
    }
    
    // Min values
    for (size_t i = 0; i < sample.registerCount; i++) {
        data.push_back((sample.min[i] >> 8) & 0xFF);
        data.push_back(sample.min[i] & 0xFF);
    }
    
    // Max values
    for (size_t i = 0; i < sample.registerCount; i++) {
        data.push_back((sample.max[i] >> 8) & 0xFF);
        data.push_back(sample.max[i] & 0xFF);
    }
    
    // Average values (if full mode)
    if (sample.mode == AGG_FULL) {
        for (size_t i = 0; i < sample.registerCount; i++) {
            data.push_back((sample.avg[i] >> 8) & 0xFF);
            data.push_back(sample.avg[i] & 0xFF);
        }
    }
    
    LOG_INFO(LOG_TAG_DATA, "Serialized to %zu bytes\n", data.size());
    return data;
}

AggregatedSample Aggregation::deserializeAggregated(
    const std::vector<uint8_t>& data
) {
    AggregatedSample sample;
    
    if (data.size() < 12) {
        LOG_INFO(LOG_TAG_DATA, "Invalid serialized data (too small)");;
        return sample;
    }
    
    size_t offset = 0;
    
    // Check marker
    if (data[offset++] != 0xAA) {
        LOG_INFO(LOG_TAG_DATA, "Invalid aggregation marker");;
        return sample;
    }
    
    // Read header
    sample.mode = (AggregationMode)data[offset++];
    sample.sampleCount = data[offset++];
    sample.registerCount = data[offset++];
    
    // Read timestamps
    sample.timestampStart = ((uint32_t)data[offset] << 24) | 
                           ((uint32_t)data[offset+1] << 16) |
                           ((uint32_t)data[offset+2] << 8) | 
                           data[offset+3];
    offset += 4;
    
    sample.timestampEnd = ((uint32_t)data[offset] << 24) | 
                         ((uint32_t)data[offset+1] << 16) |
                         ((uint32_t)data[offset+2] << 8) | 
                         data[offset+3];
    offset += 4;
    
    // Read register IDs
    for (size_t i = 0; i < sample.registerCount; i++) {
        sample.registers[i] = (RegID)data[offset++];
    }
    
    // Read min values
    for (size_t i = 0; i < sample.registerCount; i++) {
        sample.min[i] = ((uint16_t)data[offset] << 8) | data[offset+1];
        offset += 2;
    }
    
    // Read max values
    for (size_t i = 0; i < sample.registerCount; i++) {
        sample.max[i] = ((uint16_t)data[offset] << 8) | data[offset+1];
        offset += 2;
    }
    
    // Read average values (if full mode)
    if (sample.mode == AGG_FULL) {
        for (size_t i = 0; i < sample.registerCount; i++) {
            sample.avg[i] = ((uint16_t)data[offset] << 8) | data[offset+1];
            offset += 2;
        }
    }
    
    LOG_INFO(LOG_TAG_DATA, "Deserialized %zu bytes\n", offset);
    return sample;
}

float Aggregation::getReductionRatio(
    size_t originalSampleCount,
    size_t registerCount
) {
    size_t originalSize = originalSampleCount * registerCount * sizeof(uint16_t);
    size_t aggregatedSize = calculateAggregatedSize(currentMode, registerCount);
    
    if (originalSize == 0) return 0.0f;
    
    float ratio = (float)aggregatedSize / (float)originalSize;
    
    LOG_INFO(LOG_TAG_DATA, "Reduction ratio: %.2f (%zu bytes -> %zu bytes)\n",
          ratio, originalSize, aggregatedSize);
    
    return ratio;
}

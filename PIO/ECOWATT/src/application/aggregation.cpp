/**
 * @file aggregation.cpp
 * @brief Implementation of data aggregation functions
 */

#include "application/aggregation.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace DataAggregation {

// Helper function to calculate mean
static uint16_t calcMean(const uint16_t* values, size_t count) {
    if (count == 0) return 0;
    uint32_t sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += values[i];
    }
    return (uint16_t)(sum / count);
}

// Helper function to calculate median
static uint16_t calcMedian(uint16_t* sortedValues, size_t count) {
    if (count == 0) return 0;
    if (count % 2 == 0) {
        return (sortedValues[count/2 - 1] + sortedValues[count/2]) / 2;
    } else {
        return sortedValues[count/2];
    }
}

// Helper function to calculate standard deviation
static uint16_t calcStdDev(const uint16_t* values, size_t count, uint16_t mean) {
    if (count <= 1) return 0;
    
    uint32_t variance = 0;
    for (size_t i = 0; i < count; i++) {
        int32_t diff = (int32_t)values[i] - (int32_t)mean;
        variance += diff * diff;
    }
    variance /= count;
    
    return (uint16_t)sqrt((double)variance);
}

AggregatedStats calculateStats(const uint16_t* values, size_t count) {
    AggregatedStats stats = {0};
    
    if (count == 0) return stats;
    
    // Copy for median calculation (needs sorting)
    uint16_t* sorted = new uint16_t[count];
    memcpy(sorted, values, count * sizeof(uint16_t));
    std::sort(sorted, sorted + count);
    
    // Calculate basic stats
    stats.count = count;
    stats.first = values[0];
    stats.last = values[count - 1];
    stats.min = sorted[0];
    stats.max = sorted[count - 1];
    stats.range = stats.max - stats.min;
    stats.median = calcMedian(sorted, count);
    
    // Calculate sum and mean
    stats.sum = 0;
    for (size_t i = 0; i < count; i++) {
        stats.sum += values[i];
    }
    stats.mean = (uint16_t)(stats.sum / count);
    
    // Calculate standard deviation
    stats.stddev = calcStdDev(values, count, stats.mean);
    
    delete[] sorted;
    return stats;
}

uint16_t aggregate(const uint16_t* values, size_t count, AggregationMethod method) {
    if (count == 0) return 0;
    if (count == 1) return values[0];
    
    switch (method) {
        case AGG_MEAN: {
            return calcMean(values, count);
        }
        
        case AGG_MEDIAN: {
            uint16_t* sorted = new uint16_t[count];
            memcpy(sorted, values, count * sizeof(uint16_t));
            std::sort(sorted, sorted + count);
            uint16_t result = calcMedian(sorted, count);
            delete[] sorted;
            return result;
        }
        
        case AGG_MIN: {
            uint16_t minVal = values[0];
            for (size_t i = 1; i < count; i++) {
                if (values[i] < minVal) minVal = values[i];
            }
            return minVal;
        }
        
        case AGG_MAX: {
            uint16_t maxVal = values[0];
            for (size_t i = 1; i < count; i++) {
                if (values[i] > maxVal) maxVal = values[i];
            }
            return maxVal;
        }
        
        case AGG_FIRST:
            return values[0];
        
        case AGG_LAST:
            return values[count - 1];
        
        case AGG_SMART: {
            // Smart selection based on data characteristics
            AggregatedStats stats = calculateStats(values, count);
            
            // If data is stable (low variance), use mean
            float coefficientOfVariation = (float)stats.stddev / (float)stats.mean;
            if (coefficientOfVariation < 0.1f) {
                return stats.mean;
            }
            
            // If there are outliers (large range), use median
            if (stats.range > stats.mean / 2) {
                return stats.median;
            }
            
            // Otherwise use mean
            return stats.mean;
        }
        
        default:
            return calcMean(values, count);
    }
}

size_t downsample(const uint16_t* input, size_t inputCount,
                 uint16_t* output, size_t windowSize,
                 AggregationMethod method) {
    if (inputCount == 0 || windowSize == 0) return 0;
    
    size_t outputCount = 0;
    size_t i = 0;
    
    while (i < inputCount) {
        size_t actualWindowSize = std::min(windowSize, inputCount - i);
        output[outputCount++] = aggregate(&input[i], actualWindowSize, method);
        i += windowSize;
    }
    
    return outputCount;
}

size_t adaptiveDownsample(const uint16_t* input, size_t inputCount,
                         uint16_t* output, size_t targetCount,
                         AggregationMethod method) {
    if (inputCount == 0 || targetCount == 0) return 0;
    if (inputCount <= targetCount) {
        memcpy(output, input, inputCount * sizeof(uint16_t));
        return inputCount;
    }
    
    // Calculate optimal window size
    size_t windowSize = (inputCount + targetCount - 1) / targetCount; // Ceiling division
    
    return downsample(input, inputCount, output, windowSize, method);
}

bool isStable(const uint16_t* values, size_t count, uint8_t thresholdPercent) {
    if (count <= 1) return true;
    
    AggregatedStats stats = calculateStats(values, count);
    
    if (stats.mean == 0) return true; // Avoid division by zero
    
    // Calculate coefficient of variation as percentage
    float cv = ((float)stats.stddev / (float)stats.mean) * 100.0f;
    
    return cv <= thresholdPercent;
}

size_t detectOutliers(const uint16_t* values, size_t count, bool* isOutlier) {
    if (count < 4) {
        // Not enough data for outlier detection
        for (size_t i = 0; i < count; i++) {
            isOutlier[i] = false;
        }
        return 0;
    }
    
    // Sort values for quartile calculation
    uint16_t* sorted = new uint16_t[count];
    memcpy(sorted, values, count * sizeof(uint16_t));
    std::sort(sorted, sorted + count);
    
    // Calculate Q1, Q3, and IQR
    size_t q1_idx = count / 4;
    size_t q3_idx = (3 * count) / 4;
    uint16_t q1 = sorted[q1_idx];
    uint16_t q3 = sorted[q3_idx];
    uint16_t iqr = q3 - q1;
    
    // Calculate outlier bounds
    int32_t lower_bound = (int32_t)q1 - (int32_t)(1.5f * iqr);
    int32_t upper_bound = (int32_t)q3 + (int32_t)(1.5f * iqr);
    
    if (lower_bound < 0) lower_bound = 0;
    if (upper_bound > 65535) upper_bound = 65535;
    
    // Mark outliers
    size_t outlierCount = 0;
    for (size_t i = 0; i < count; i++) {
        if ((int32_t)values[i] < lower_bound || (int32_t)values[i] > upper_bound) {
            isOutlier[i] = true;
            outlierCount++;
        } else {
            isOutlier[i] = false;
        }
    }
    
    delete[] sorted;
    return outlierCount;
}

size_t removeOutliers(const uint16_t* values, size_t count, uint16_t* output) {
    if (count == 0) return 0;
    
    bool* isOutlier = new bool[count];
    detectOutliers(values, count, isOutlier);
    
    size_t outputCount = 0;
    for (size_t i = 0; i < count; i++) {
        if (!isOutlier[i]) {
            output[outputCount++] = values[i];
        }
    }
    
    delete[] isOutlier;
    return outputCount;
}

} // namespace DataAggregation

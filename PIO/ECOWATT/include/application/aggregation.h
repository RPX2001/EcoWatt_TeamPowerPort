/**
 * @file aggregation.h
 * @brief Data aggregation functions for downsampling sensor data
 * 
 * Provides statistical aggregation methods to reduce data volume while
 * preserving important statistical properties. Useful for combining
 * multiple sensor readings into summary statistics.
 * 
 * Aggregation Methods:
 * - Mean (Average): Best for stable measurements
 * - Median: Robust to outliers
 * - Min/Max: Track extremes
 * - Range: Variability indicator
 * - Standard Deviation: Measure of spread
 * - First/Last: Preserve temporal endpoints
 */

#ifndef AGGREGATION_H
#define AGGREGATION_H

#include <stdint.h>
#include <stddef.h>

namespace DataAggregation {

    /**
     * @brief Aggregated statistics for a set of values
     */
    struct AggregatedStats {
        uint16_t mean;      // Average value
        uint16_t median;    // Middle value (robust to outliers)
        uint16_t min;       // Minimum value
        uint16_t max;       // Maximum value
        uint16_t range;     // Max - Min
        uint16_t first;     // First value in sequence
        uint16_t last;      // Last value in sequence
        uint16_t stddev;    // Standard deviation (spread)
        uint32_t sum;       // Sum of all values
        size_t count;       // Number of values aggregated
    };

    /**
     * @brief Aggregation method selection
     */
    enum AggregationMethod {
        AGG_MEAN,           // Use mean (average)
        AGG_MEDIAN,         // Use median (middle value)
        AGG_MIN,            // Use minimum
        AGG_MAX,            // Use maximum
        AGG_FIRST,          // Use first value
        AGG_LAST,           // Use last value
        AGG_SMART           // Auto-select based on data characteristics
    };

    /**
     * @brief Calculate full statistics for an array of values
     * 
     * @param values Input array of sensor values
     * @param count Number of values in array
     * @return AggregatedStats structure with all statistics
     */
    AggregatedStats calculateStats(const uint16_t* values, size_t count);

    /**
     * @brief Aggregate values using specified method
     * 
     * @param values Input array of sensor values
     * @param count Number of values in array
     * @param method Aggregation method to use
     * @return Single aggregated value
     */
    uint16_t aggregate(const uint16_t* values, size_t count, AggregationMethod method);

    /**
     * @brief Downsample data by aggregating windows
     * 
     * Example: 450 samples at 2s intervals → 30 samples at 60s intervals
     * 
     * @param input Input array of values
     * @param inputCount Number of input values
     * @param output Output array for downsampled values
     * @param windowSize Number of input values per output value
     * @param method Aggregation method to use
     * @return Number of output values produced
     */
    size_t downsample(const uint16_t* input, size_t inputCount,
                     uint16_t* output, size_t windowSize,
                     AggregationMethod method);

    /**
     * @brief Adaptive downsampling - automatically determines window size
     * 
     * @param input Input array of values
     * @param inputCount Number of input values
     * @param output Output array for downsampled values
     * @param targetCount Desired number of output values
     * @param method Aggregation method to use
     * @return Number of output values produced
     */
    size_t adaptiveDownsample(const uint16_t* input, size_t inputCount,
                             uint16_t* output, size_t targetCount,
                             AggregationMethod method);

    /**
     * @brief Check if data is relatively stable (low variance)
     * 
     * @param values Input array
     * @param count Number of values
     * @param thresholdPercent Stability threshold (0-100)
     * @return true if data is stable
     */
    bool isStable(const uint16_t* values, size_t count, uint8_t thresholdPercent = 10);

    /**
     * @brief Detect outliers using IQR method
     * 
     * @param values Input array
     * @param count Number of values
     * @param isOutlier Output array (true if value is outlier)
     * @return Number of outliers detected
     */
    size_t detectOutliers(const uint16_t* values, size_t count, bool* isOutlier);

    /**
     * @brief Remove outliers and return cleaned data
     * 
     * @param values Input array
     * @param count Number of input values
     * @param output Output array for cleaned data
     * @return Number of values in output (outliers removed)
     */
    size_t removeOutliers(const uint16_t* values, size_t count, uint16_t* output);

} // namespace DataAggregation

#endif // AGGREGATION_H

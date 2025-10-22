/**
 * @file data_pipeline.h
 * @brief Data acquisition, compression, and queueing pipeline
 * 
 * This module handles the complete data flow from sensor reading
 * through compression to queueing for upload.
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#ifndef DATA_PIPELINE_H
#define DATA_PIPELINE_H

#include <Arduino.h>
#include "peripheral/acquisition.h"
#include "application/compression.h"

/**
 * @class DataPipeline
 * @brief Manages the data acquisition and compression pipeline
 * 
 * This class provides a singleton-style interface for reading sensors,
 * batching samples, compressing data, and queueing for upload.
 */
class DataPipeline {
public:
    /**
     * @brief Initialize the data pipeline
     * 
     * @param selection Initial register selection
     * @param registerCount Number of registers to read
     */
    static void init(const RegID* selection, size_t registerCount);

    /**
     * @brief Poll sensors and process data
     * 
     * Reads sensors, adds to batch, compresses when full, and queues for upload
     */
    static void pollAndProcess();

    /**
     * @brief Update register selection
     * 
     * @param newSelection New register array
     * @param newCount Number of registers in new selection
     */
    static void updateRegisterSelection(const RegID* newSelection, size_t newCount);

    /**
     * @brief Get current sample batch information
     * 
     * @param samplesInBatch Output: number of samples in current batch
     * @param batchSize Output: maximum batch size
     */
    static void getBatchInfo(size_t& samplesInBatch, size_t& batchSize);

    /**
     * @brief Force compression of current batch
     * 
     * Useful for testing or manual triggering
     * 
     * @return true if compression successful
     */
    static bool forceCompressBatch();

private:
    static const RegID* activeRegisters;
    static size_t activeRegisterCount;
    static uint16_t* sensorBuffer;
    static SampleBatch currentBatch;

    // Helper functions
    static bool readSensors(uint16_t* buffer);
    static bool compressAndQueue();

    // Prevent instantiation
    DataPipeline() = delete;
    ~DataPipeline() = delete;
    DataPipeline(const DataPipeline&) = delete;
    DataPipeline& operator=(const DataPipeline&) = delete;
};

#endif // DATA_PIPELINE_H

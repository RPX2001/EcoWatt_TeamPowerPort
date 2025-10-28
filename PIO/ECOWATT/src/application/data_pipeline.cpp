/**
 * @file data_pipeline.cpp
 * @brief Implementation of data acquisition and compression pipeline
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#include "application/data_pipeline.h"
#include "application/data_uploader.h"
#include "application/statistics_manager.h"
#include "peripheral/print.h"
#include "peripheral/acquisition.h"
#include "application/peripheral_power.h"
#include "application/compression.h"

// Forward declarations of functions from main.cpp
extern bool readMultipleRegisters(const RegID* selection, size_t count, uint16_t* data);
extern std::vector<uint8_t> compressBatchWithSmartSelection(const SampleBatch& batch, 
                                                   const RegID* selection, size_t registerCount,
                                                   unsigned long& compressionTime, 
                                                   char* methodUsed, size_t methodSize,
                                                   float& academicRatio, 
                                                   float& traditionalRatio);

// Initialize static members
const RegID* DataPipeline::activeRegisters = nullptr;
size_t DataPipeline::activeRegisterCount = 0;
uint16_t* DataPipeline::sensorBuffer = nullptr;
SampleBatch DataPipeline::currentBatch;

void DataPipeline::init(const RegID* selection, size_t registerCount) {
    activeRegisters = selection;
    activeRegisterCount = registerCount;
    
    // Allocate sensor buffer
    if (sensorBuffer) {
        delete[] sensorBuffer;
    }
    sensorBuffer = new uint16_t[registerCount];
    
    // Initialize batch with default size (will be updated dynamically)
    currentBatch.reset();
    
    print("[DataPipeline] Initialized with %zu registers\n", registerCount);
}

void DataPipeline::updateBatchSize(uint64_t pollFreqUs, uint64_t uploadFreqUs) {
    // Calculate batch size: how many polls happen in one upload cycle
    // batch_size = upload_frequency / poll_frequency
    size_t calculatedBatchSize = (size_t)(uploadFreqUs / pollFreqUs);
    
    // Apply safety limits
    if (calculatedBatchSize < 1) {
        calculatedBatchSize = 1;  // Minimum 1 sample
    }
    if (calculatedBatchSize > 50) {
        calculatedBatchSize = 50;  // Maximum 50 samples (memory constraint)
    }
    
    currentBatch.setBatchSize(calculatedBatchSize);
    
    print("[DataPipeline] Batch size updated: %zu samples\n", calculatedBatchSize);
    print("[DataPipeline]   Poll: %.2fs, Upload: %.2fs\n", 
          pollFreqUs / 1000000.0, uploadFreqUs / 1000000.0);
}

void DataPipeline::pollAndProcess() {
    // Enable UART for Modbus communication (power gating)
    PERIPHERAL_UART_ON();
    
    if (readSensors(sensorBuffer)) {
        // Print polled values
        print("[DataPipeline] Polled: ");
        for (size_t i = 0; i < activeRegisterCount; i++) {
            print("%s=%u ", REGISTER_MAP[activeRegisters[i]].name, sensorBuffer[i]);
        }
        print("\n");
        
        // Add to batch
        currentBatch.addSample(sensorBuffer, millis(), activeRegisterCount);

        // When batch is full, compress and queue
        if (currentBatch.isFull()) {
            compressAndQueue();
        }
    } else {
        print("[DataPipeline] Failed to read registers\n");
    }
    
    // Disable UART to save power
    PERIPHERAL_UART_OFF();
}

bool DataPipeline::readSensors(uint16_t* buffer) {
    return readMultipleRegisters(activeRegisters, activeRegisterCount, buffer);
}

bool DataPipeline::compressAndQueue() {
    unsigned long compressionTime;
    char methodUsed[32];
    float academicRatio, traditionalRatio;
    
    // Compress the entire batch using the function from main.cpp
    std::vector<uint8_t> compressedBinary = compressBatchWithSmartSelection(
        currentBatch, activeRegisters, activeRegisterCount, 
        compressionTime, methodUsed, sizeof(methodUsed),
        academicRatio, traditionalRatio);
    
    if (!compressedBinary.empty()) {
        // Create compressed data entry
        SmartCompressedData entry(compressedBinary, activeRegisters, activeRegisterCount, methodUsed);
        entry.compressionTime = compressionTime;
        entry.academicRatio = academicRatio;
        entry.traditionalRatio = traditionalRatio;
        entry.losslessVerified = true;
        
        // Queue for upload
        if (DataUploader::addToQueue(entry)) {
            print("[DataPipeline] Batch compressed and queued successfully!\n");
            
            // Update statistics
            StatisticsManager::updateCompressionStats(methodUsed, academicRatio, compressionTime);
            StatisticsManager::incrementMethodUsage(methodUsed);
            StatisticsManager::recordLosslessSuccess();
        } else {
            print("[DataPipeline] Failed to queue compressed data (buffer full)\n");
            StatisticsManager::recordCompressionFailure();
        }
        
        // Reset batch for next collection
        currentBatch.reset();
        return true;
    } else {
        print("[DataPipeline] Compression failed for batch!\n");
        StatisticsManager::recordCompressionFailure();
        currentBatch.reset();
        return false;
    }
}

void DataPipeline::updateRegisterSelection(const RegID* newSelection, size_t newCount) {
    activeRegisters = newSelection;
    activeRegisterCount = newCount;
    
    // Reallocate sensor buffer
    if (sensorBuffer) {
        delete[] sensorBuffer;
    }
    sensorBuffer = new uint16_t[newCount];
    
    // Reset batch
    currentBatch.reset();
    
    print("[DataPipeline] Register selection updated: %zu registers\n", newCount);
    for (size_t i = 0; i < newCount && i < REGISTER_COUNT; i++) {
        print("  [%zu] %s (ID: %d)\n", i, REGISTER_MAP[newSelection[i]].name, newSelection[i]);
    }
}

void DataPipeline::getBatchInfo(size_t& samplesInBatch, size_t& batchSize) {
    samplesInBatch = currentBatch.sampleCount;
    batchSize = currentBatch.getBatchSize();  // Use dynamic batch size
}

bool DataPipeline::forceCompressBatch() {
    if (currentBatch.sampleCount > 0) {
        print("[DataPipeline] Force compressing batch with %zu samples\n", 
              currentBatch.sampleCount);
        return compressAndQueue();
    }
    return false;
}

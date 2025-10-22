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
    
    // Initialize batch
    currentBatch.reset();
    
    print("[DataPipeline] Initialized with %zu registers\n", registerCount);
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
    
    // Compress the entire batch
    std::vector<uint8_t> compressedBinary = DataCompression::compressBatchWithSmartSelection(
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
    samplesInBatch = currentBatch.getSampleCount();
    batchSize = currentBatch.getMaxSamples();
}

bool DataPipeline::forceCompressBatch() {
    if (currentBatch.getSampleCount() > 0) {
        print("[DataPipeline] Force compressing batch with %zu samples\n", 
              currentBatch.getSampleCount());
        return compressAndQueue();
    }
    return false;
}

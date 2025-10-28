/**
 * @file data_uploader.h
 * @brief Cloud data upload and ring buffer management
 * 
 * This module manages the ring buffer for compressed data and handles
 * uploading data to the cloud server with proper formatting and metadata.
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#ifndef DATA_UPLOADER_H
#define DATA_UPLOADER_H

#include <Arduino.h>
#include "ringbuffer.h"
#include "compression.h"

/**
 * @class DataUploader
 * @brief Manages cloud data upload and ring buffer operations
 * 
 * This class provides a singleton-style interface for managing compressed
 * data storage in a ring buffer and uploading batches to the cloud server.
 */
class DataUploader {
public:
    /**
     * @brief Initialize the data uploader with server configuration
     * 
     * @param serverURL Base URL of the Flask server
     * @param deviceID Unique device identifier
     */
    static void init(const char* serverURL, const char* deviceID);

    /**
     * @brief Add compressed data to the upload queue
     * 
     * @param data Reference to SmartCompressedData entry
     * @return true if successfully queued, false if buffer is full
     */
    static bool addToQueue(const SmartCompressedData& data);

    /**
     * @brief Upload all pending data to the cloud server
     * 
     * This function drains the ring buffer and uploads all compressed
     * data packets to the server in a single HTTP POST request.
     * Implements retry logic with exponential backoff on failure.
     * 
     * @return true if upload successful, false otherwise
     */
    static bool uploadPendingData();
    
    /**
     * @brief Set maximum retry attempts for failed uploads
     * 
     * @param maxRetries Maximum number of retry attempts (default: 3)
     */
    static void setMaxRetries(uint8_t maxRetries);
    
    /**
     * @brief Get current retry configuration
     * 
     * @return Maximum number of retry attempts
     */
    static uint8_t getMaxRetries();

    /**
     * @brief Get the current number of items in the queue
     * 
     * @return Number of compressed data packets waiting to be uploaded
     */
    static size_t getQueueSize();

    /**
     * @brief Check if the upload queue is full
     * 
     * @return true if queue is at capacity
     */
    static bool isQueueFull();

    /**
     * @brief Check if the upload queue is empty
     * 
     * @return true if queue is empty
     */
    static bool isQueueEmpty();

    /**
     * @brief Clear all data from the queue without uploading
     * 
     * Used for testing or when resetting the system
     */
    static void clearQueue();

    /**
     * @brief Get upload statistics
     * 
     * @param totalUploads Output: total number of successful uploads
     * @param totalFailed Output: total number of failed uploads
     * @param totalBytesUploaded Output: total bytes uploaded
     */
    static void getUploadStats(unsigned long& totalUploads, unsigned long& totalFailed, 
                               size_t& totalBytesUploaded);

    /**
     * @brief Reset upload statistics
     */
    static void resetStats();

    /**
     * @brief Print upload statistics to serial
     */
    static void printStats();

    /**
     * @brief Set upload URL (for testing or reconfiguration)
     * 
     * @param url New upload endpoint URL
     */
    static void setUploadURL(const char* url);

    /**
     * @brief Get the configured device ID
     * 
     * @return Pointer to device ID string
     */
    static const char* getDeviceID();

private:
    // Ring buffer for compressed data (capacity: 20 entries)
    static RingBuffer<SmartCompressedData, 20> ringBuffer;

    // Server configuration
    static char uploadURL[256];
    static char deviceID[64];

    // Upload statistics
    static unsigned long uploadCount;
    static unsigned long uploadFailures;
    static size_t totalBytesUploaded;
    
    // Retry configuration
    static uint8_t maxRetryAttempts;
    static uint8_t currentRetryCount;
    static unsigned long lastFailedUploadTime;

    // Helper functions
    static bool buildUploadPayload(const std::vector<SmartCompressedData>& data, 
                                   char* jsonBuffer, size_t bufferSize);
    static void convertBinaryToBase64(const std::vector<uint8_t>& binaryData, 
                                      char* result, size_t resultSize);
    static bool attemptUpload(const std::vector<SmartCompressedData>& data);
    static unsigned long calculateBackoffDelay(uint8_t attempt);

    // Prevent instantiation
    DataUploader() = delete;
    ~DataUploader() = delete;
    DataUploader(const DataUploader&) = delete;
    DataUploader& operator=(const DataUploader&) = delete;
};

#endif // DATA_UPLOADER_H

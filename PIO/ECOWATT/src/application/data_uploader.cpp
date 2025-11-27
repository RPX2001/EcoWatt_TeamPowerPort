/**
 * @file data_uploader.cpp
 * @brief Implementation of cloud data upload and ring buffer management
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#include "application/data_uploader.h"
#include "peripheral/logger.h"
#include "peripheral/acquisition.h"
#include "application/security.h"
#include "application/config_manager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

// Helper function to get current Unix timestamp in seconds
static unsigned long getCurrentTimestamp() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        // Convert tm struct to Unix timestamp in SECONDS
        time_t now = mktime(&timeinfo);
        return (unsigned long)now;
    }
    // Fallback to millis/1000 if NTP not synced (better than raw millis)
    return millis() / 1000;
}

// Initialize static members
RingBuffer<SmartCompressedData, 20> DataUploader::ringBuffer;
char DataUploader::uploadURL[256] = "";
char DataUploader::deviceID[64] = "ESP32_Unknown";
unsigned long DataUploader::uploadCount = 0;
unsigned long DataUploader::uploadFailures = 0;
size_t DataUploader::totalBytesUploaded = 0;
uint8_t DataUploader::maxRetryAttempts = 1;  // Only 1 retry to fit watchdog window
uint8_t DataUploader::currentRetryCount = 0;
unsigned long DataUploader::lastFailedUploadTime = 0;

void DataUploader::init(const char* serverURL, const char* devID) {
    strncpy(uploadURL, serverURL, sizeof(uploadURL) - 1);
    uploadURL[sizeof(uploadURL) - 1] = '\0';
    
    strncpy(deviceID, devID, sizeof(deviceID) - 1);
    deviceID[sizeof(deviceID) - 1] = '\0';
    
    uploadCount = 0;
    uploadFailures = 0;
    totalBytesUploaded = 0;
    
    LOG_INFO(LOG_TAG_UPLOAD, "Data uploader initialized");
    LOG_DEBUG(LOG_TAG_UPLOAD, "Server: %s", uploadURL);
    LOG_DEBUG(LOG_TAG_UPLOAD, "Device: %s", deviceID);
}

bool DataUploader::addToQueue(const SmartCompressedData& data) {
    if (ringBuffer.size() >= 20) {  // Buffer capacity is 20
        LOG_WARN(LOG_TAG_UPLOAD, "Buffer full (%zu/20), cannot add data", ringBuffer.size());
        return false;
    }
    
    ringBuffer.push(data);
    LOG_DEBUG(LOG_TAG_BUFFER, "Added to queue (size: %zu/20)", ringBuffer.size());
    return true;
}

bool DataUploader::uploadPendingData() {
    if (WiFi.status() != WL_CONNECTED) {
        LOG_DEBUG(LOG_TAG_UPLOAD, "WiFi not connected, skipping upload");
        return false;
    }

    if (ringBuffer.empty()) {
        // Silently skip if buffer is empty
        currentRetryCount = 0;  // Reset retry counter on empty buffer
        return true;
    }
    
    LOG_SECTION("DATA UPLOAD CYCLE");
    
    // Drain all data from buffer
    auto allData = ringBuffer.drain_all();
    
    LOG_INFO(LOG_TAG_UPLOAD, "Preparing %zu compressed batches", allData.size());
    
    // Check available heap before attempting upload
    size_t freeHeap = ESP.getFreeHeap();
    LOG_DEBUG(LOG_TAG_UPLOAD, "Free heap: %zu bytes", freeHeap);
    
    if (freeHeap < 20000) {  // Require at least 20KB free heap
        LOG_ERROR(LOG_TAG_UPLOAD, "Insufficient heap memory (%zu bytes)", freeHeap);
        // Don't restore data - will retry next cycle when memory is available
        return false;
    }
    
    // Attempt upload with retry logic
    bool success = false;
    for (uint8_t attempt = 0; attempt <= maxRetryAttempts; attempt++) {
        if (attempt > 0) {
            unsigned long backoffDelay = calculateBackoffDelay(attempt);
            LOG_WARN(LOG_TAG_UPLOAD, "Retry %u/%u after %lu ms backoff", 
                     attempt, maxRetryAttempts, backoffDelay);
            vTaskDelay(pdMS_TO_TICKS(backoffDelay));  // FreeRTOS-aware delay
        }
        
        success = attemptUpload(allData);
        
        if (success) {
            currentRetryCount = 0;
            break;
        } else {
            currentRetryCount = attempt + 1;
            if (attempt < maxRetryAttempts) {
                LOG_ERROR(LOG_TAG_UPLOAD, "Attempt %u failed, retrying", attempt + 1);
            }
        }
    }
    
    if (!success) {
        LOG_ERROR(LOG_TAG_UPLOAD, "Upload failed after %u attempts", maxRetryAttempts + 1);
        LOG_WARN(LOG_TAG_UPLOAD, "Restoring %zu packets to buffer", allData.size());
        
        // Restore data to buffer for next upload cycle
        for (const auto& entry : allData) {
            if (ringBuffer.size() >= 20) {
                LOG_ERROR(LOG_TAG_BUFFER, "Buffer full! Data packet lost!");
                break;
            }
            ringBuffer.push(entry);
        }
        lastFailedUploadTime = millis();
    }
    
    return success;
}

bool DataUploader::attemptUpload(const std::vector<SmartCompressedData>& allData) {
    
    LOG_INFO(LOG_TAG_UPLOAD, "Sending %zu packets to server", allData.size());
    
    // Create WiFiClient with extended connection timeout
    WiFiClient client;
    client.setTimeout(15000);  // 15 second connection timeout in MILLISECONDS
    
    HTTPClient http;
    http.begin(client, uploadURL);  // Use our WiFiClient with custom timeout
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(15000);  // 15 second HTTP timeout (in milliseconds)
    
    // Feed the watchdog during long operations
    yield();
    
    // CRITICAL: Use heap allocation for large buffers to prevent stack overflow
    // ESP32 task stack is limited (~4-8KB), we need ~10KB+ for JSON processing
    
    // Allocate JSON document on HEAP to prevent stack overflow
    DynamicJsonDocument* doc = new DynamicJsonDocument(6144);
    if (!doc) {
        LOG_ERROR(LOG_TAG_UPLOAD, "Failed to allocate JSON document");
        http.end();
        uploadFailures++;
        return false;
    }
    
    (*doc)["device_id"] = deviceID;
    (*doc)["timestamp"] = getCurrentTimestamp();
    (*doc)["data_type"] = "compressed_sensor_batch";
    (*doc)["total_samples"] = allData.size();
    
    // Add sampling interval from current config (convert microseconds to seconds)
    const SystemConfig& config = ConfigManager::getCurrentConfig();
    (*doc)["sampling_interval"] = (uint32_t)(config.pollFrequency / 1000000);  // Convert μs to seconds
    
    // Build register mapping from most recent entry (reflects current config)
    // NOTE: Changed from first to last entry to handle config changes correctly
    // When config changes mid-queue, older packets may have different register counts
    JsonObject registerMapping = (*doc)["register_mapping"].to<JsonObject>();
    if (!allData.empty()) {
        const auto& lastEntry = allData[allData.size() - 1];  // Use MOST RECENT packet
        for (size_t i = 0; i < lastEntry.registerCount && i < REGISTER_COUNT; i++) {
            char key[8];
            snprintf(key, sizeof(key), "%zu", i);
            registerMapping[key] = REGISTER_MAP[lastEntry.registers[i]].name;
        }
        LOG_INFO(LOG_TAG_UPLOAD, "Global register_mapping: %zu registers (from most recent packet)",
                 lastEntry.registerCount);
    }
    
    // Compressed data packets
    JsonArray compressedPackets = (*doc)["compressed_data"].to<JsonArray>();
    
    size_t totalOriginalBytes = 0;
    size_t totalCompressedBytes = 0;
    
    // Track register count changes across packets (for diagnostics)
    size_t minRegisterCount = SIZE_MAX;
    size_t maxRegisterCount = 0;
    bool heterogeneousPackets = false;
    
    for (const auto& entry : allData) {
        // Feed watchdog during packet processing
        yield();
        
        // Track register count variations
        if (entry.registerCount < minRegisterCount) minRegisterCount = entry.registerCount;
        if (entry.registerCount > maxRegisterCount) maxRegisterCount = entry.registerCount;
        if (minRegisterCount != maxRegisterCount) heterogeneousPackets = true;
        
        JsonObject packet = compressedPackets.createNestedObject();
        
        // Compressed binary data (Base64 encoded)
        char base64Buffer[256];
        convertBinaryToBase64(entry.binaryData, base64Buffer, sizeof(base64Buffer));
        packet["compressed_binary"] = base64Buffer;
        
        // Decompression metadata
        JsonObject decompMeta = packet["decompression_metadata"].to<JsonObject>();
        decompMeta["method"] = entry.compressionMethod;
        decompMeta["register_count"] = entry.registerCount;
        decompMeta["sample_count"] = entry.sampleCount;  // Number of samples in this packet
        decompMeta["original_size_bytes"] = entry.originalSize;
        decompMeta["compressed_size_bytes"] = entry.binaryData.size();
        decompMeta["timestamp"] = entry.timestamp;  // First sample timestamp
        
        // Register layout for this packet
        JsonArray regLayout = decompMeta["register_layout"].to<JsonArray>();
        for (size_t i = 0; i < entry.registerCount; i++) {
            regLayout.add(entry.registers[i]);
        }
        
        // Compression performance metrics
        JsonObject metrics = packet["performance_metrics"].to<JsonObject>();
        metrics["academic_ratio"] = entry.academicRatio;
        metrics["traditional_ratio"] = entry.traditionalRatio;
        metrics["compression_time_us"] = entry.compressionTime;
        metrics["savings_percent"] = (1.0f - entry.academicRatio) * 100.0f;
        metrics["lossless_verified"] = entry.losslessVerified;
        
        totalOriginalBytes += entry.originalSize;
        totalCompressedBytes += entry.binaryData.size();
    }
    
    // Overall session summary
    JsonObject sessionSummary = (*doc)["session_summary"].to<JsonObject>();
    sessionSummary["total_original_bytes"] = totalOriginalBytes;
    sessionSummary["total_compressed_bytes"] = totalCompressedBytes;
    sessionSummary["overall_academic_ratio"] = (totalOriginalBytes > 0) ? 
        (float)totalCompressedBytes / (float)totalOriginalBytes : 1.0f;
    sessionSummary["overall_savings_percent"] = (totalOriginalBytes > 0) ? 
        (1.0f - (float)totalCompressedBytes / (float)totalOriginalBytes) * 100.0f : 0.0f;
    
    // Print compression statistics
    float savingsPercent = (totalOriginalBytes > 0) ? 
        (1.0f - (float)totalCompressedBytes / (float)totalOriginalBytes) * 100.0f : 0.0f;
    
    LOG_INFO(LOG_TAG_COMPRESS, "Compression: %zu → %zu bytes (%.1f%% savings)", 
          totalOriginalBytes, totalCompressedBytes, savingsPercent);
    LOG_DEBUG(LOG_TAG_UPLOAD, "Sending %zu packets", allData.size());
    
    // Log register count information (helpful for debugging config changes)
    if (heterogeneousPackets) {
        LOG_WARN(LOG_TAG_UPLOAD, "Heterogeneous upload: packets have %zu-%zu registers (config changed mid-queue)",
                 minRegisterCount, maxRegisterCount);
    } else if (maxRegisterCount > 0) {
        LOG_DEBUG(LOG_TAG_UPLOAD, "Homogeneous upload: all packets have %zu registers",
                  maxRegisterCount);
    }
    
    // INDUSTRY STANDARD APPROACH:
    // Send the JSON with compressed data embedded (base64 encoded)
    // The server will extract and decompress the compressed_data field
    
    // Allocate JSON string buffer on HEAP to prevent stack overflow
    char* jsonString = new char[4096];
    if (!jsonString) {
        LOG_ERROR(LOG_TAG_UPLOAD, "Failed to allocate JSON string buffer");
        delete doc;
        http.end();
        uploadFailures++;
        return false;
    }
    
    size_t jsonLen = serializeJson(*doc, jsonString, 4096);
    
    // Clean up JSON document (no longer needed)
    delete doc;
    
    if (jsonLen == 0 || jsonLen >= 4095) {
        LOG_ERROR(LOG_TAG_UPLOAD, "JSON serialization failed (size: %zu/4096)", jsonLen);
        delete[] jsonString;
        http.end();
        uploadFailures++;
        return false;
    }
    
    LOG_DEBUG(LOG_TAG_UPLOAD, "JSON payload: %zu bytes", jsonLen);
    
    // Apply Security Layer to the JSON string
    // The security layer will handle Base64 encoding for transport
    LOG_DEBUG(LOG_TAG_SECURITY, "Securing payload with HMAC");
    
    // Feed watchdog before security operation
    yield();
    
    // Allocate buffer for secured payload (base64 encoding increases size by ~1.37x + JSON overhead)
    char* securedPayload = new char[8192];  // Large enough for 4096 JSON + base64 + security wrapper
    if (!securedPayload) {
        LOG_ERROR(LOG_TAG_SECURITY, "Failed to allocate memory for secured payload");
        delete[] jsonString;  // Clean up jsonString
        http.end();
        uploadFailures++;
        return false;
    }
    
    // Pass FALSE for isCompressed - we're sending JSON (not raw binary)
    // The JSON contains base64-encoded compressed data in "compressed_data" field
    // The server will decode the JSON, then extract and decompress the binary data
    if (!SecurityLayer::securePayload(jsonString, securedPayload, 8192, false)) {
        LOG_ERROR(LOG_TAG_SECURITY, "Payload security failed");
        delete[] jsonString;  // Clean up jsonString
        delete[] securedPayload;
        http.end();
        uploadFailures++;
        return false;
    }
    
    // JSON string no longer needed after securing
    delete[] jsonString;
    
    LOG_SUCCESS(LOG_TAG_SECURITY, "Payload secured");
    LOG_INFO(LOG_TAG_UPLOAD, "Uploading to server");
    
    // Feed watchdog before HTTP POST
    yield();
    
    int httpResponseCode = http.POST((uint8_t*)securedPayload, strlen(securedPayload));
    
    bool success = false;
    if (httpResponseCode == 200) {
        LOG_SUCCESS(LOG_TAG_UPLOAD, "Upload successful (HTTP 200)");
        uploadCount++;
        totalBytesUploaded += strlen(securedPayload);
        success = true;
    } else {
        LOG_ERROR(LOG_TAG_UPLOAD, "Upload failed (HTTP %d)", httpResponseCode);
        if (httpResponseCode > 0) {
            String errorResponse = http.getString();
            LOG_DEBUG(LOG_TAG_UPLOAD, "Response: %s", errorResponse.c_str());
        }
        uploadFailures++;
    }
    
    delete[] securedPayload;
    http.end();
    
    return success;
}

void DataUploader::convertBinaryToBase64(const std::vector<uint8_t>& binaryData, 
                                         char* result, size_t resultSize) {
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t pos = 0;
    size_t i = 0;
    
    // Process complete 3-byte groups
    for (; i + 2 < binaryData.size() && pos < resultSize - 5; i += 3) {
        uint32_t value = (binaryData[i] << 16) | (binaryData[i + 1] << 8) | binaryData[i + 2];
        
        result[pos++] = chars[(value >> 18) & 0x3F];
        result[pos++] = chars[(value >> 12) & 0x3F];
        result[pos++] = chars[(value >> 6) & 0x3F];
        result[pos++] = chars[value & 0x3F];
    }
    
    // Handle remaining 1 or 2 bytes
    if (i < binaryData.size() && pos < resultSize - 5) {
        size_t remaining = binaryData.size() - i;
        uint32_t value = binaryData[i] << 16;
        
        if (remaining == 2) {
            // 2 bytes remaining: encode as 3 chars + '='
            value |= binaryData[i + 1] << 8;
            result[pos++] = chars[(value >> 18) & 0x3F];
            result[pos++] = chars[(value >> 12) & 0x3F];
            result[pos++] = chars[(value >> 6) & 0x3F];
            result[pos++] = '=';
        } else {
            // 1 byte remaining: encode as 2 chars + '=='
            result[pos++] = chars[(value >> 18) & 0x3F];
            result[pos++] = chars[(value >> 12) & 0x3F];
            result[pos++] = '=';
            result[pos++] = '=';
        }
    }
    
    result[pos] = '\0';
}

size_t DataUploader::getQueueSize() {
    return ringBuffer.size();
}

bool DataUploader::isQueueFull() {
    return ringBuffer.size() >= 20;  // Buffer capacity is 20
}

bool DataUploader::isQueueEmpty() {
    return ringBuffer.empty();
}

void DataUploader::clearQueue() {
    ringBuffer.clear();
    LOG_INFO(LOG_TAG_BUFFER, "Queue cleared");
}

void DataUploader::getUploadStats(unsigned long& totalUploads, unsigned long& totalFailed, 
                                  size_t& bytesUploaded) {
    totalUploads = uploadCount;
    totalFailed = uploadFailures;
    bytesUploaded = totalBytesUploaded;
}

void DataUploader::resetStats() {
    uploadCount = 0;
    uploadFailures = 0;
    totalBytesUploaded = 0;
    LOG_INFO(LOG_TAG_STATS, "Upload statistics reset");
}

void DataUploader::printStats() {
    LOG_SECTION("DATA UPLOADER STATISTICS");
    LOG_INFO(LOG_TAG_STATS, "Successful: %lu | Failed: %lu", uploadCount, uploadFailures);
    LOG_INFO(LOG_TAG_STATS, "Bytes sent: %zu", totalBytesUploaded);
    
    unsigned long total = uploadCount + uploadFailures;
    if (total > 0) {
        float successRate = (uploadCount * 100.0f) / total;
        LOG_INFO(LOG_TAG_STATS, "Success rate: %.2f%%", successRate);
    }
    
    LOG_INFO(LOG_TAG_BUFFER, "Queue: %zu/20", ringBuffer.size());
}

void DataUploader::setUploadURL(const char* url) {
    strncpy(uploadURL, url, sizeof(uploadURL) - 1);
    uploadURL[sizeof(uploadURL) - 1] = '\0';
    LOG_INFO(LOG_TAG_UPLOAD, "URL updated: %s", uploadURL);
}

const char* DataUploader::getDeviceID() {
    return deviceID;
}

unsigned long DataUploader::calculateBackoffDelay(uint8_t attempt) {
    // Exponential backoff optimized for watchdog: 300ms, 600ms, 1200ms
    // Total for 3 retries = 2.1s, well within 5s watchdog timeout
    unsigned long delay = 300 * (1 << (attempt - 1));  // 300ms * 2^(attempt-1)
    if (delay > 5000) delay = 5000;  // Cap at 5 seconds
    return delay;
}

void DataUploader::setMaxRetries(uint8_t maxRetries) {
    maxRetryAttempts = maxRetries;
    LOG_INFO(LOG_TAG_UPLOAD, "Max retry attempts: %u", maxRetries);
}

uint8_t DataUploader::getMaxRetries() {
    return maxRetryAttempts;
}

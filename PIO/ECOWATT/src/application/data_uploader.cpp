/**
 * @file data_uploader.cpp
 * @brief Implementation of cloud data upload and ring buffer management
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#include "application/data_uploader.h"
#include "peripheral/print.h"
#include "peripheral/formatted_print.h"
#include "peripheral/acquisition.h"
#include "application/security.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

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
    
    print("[DataUploader] Initialized\n");
    print("[DataUploader] Server: %s\n", uploadURL);
    print("[DataUploader] Device: %s\n", deviceID);
}

bool DataUploader::addToQueue(const SmartCompressedData& data) {
    if (ringBuffer.size() >= 20) {  // Buffer capacity is 20
        print("[DataUploader] WARNING: Buffer is full, cannot add data\n");
        return false;
    }
    
    ringBuffer.push(data);
    print("[DataUploader] Added to queue (size: %zu/%d)\n", ringBuffer.size(), 20);
    return true;
}

bool DataUploader::uploadPendingData() {
    if (WiFi.status() != WL_CONNECTED) {
        print("[DataUploader] WiFi not connected, skipping upload\n");
        return false;
    }

    if (ringBuffer.empty()) {
        // Silently skip if buffer is empty
        currentRetryCount = 0;  // Reset retry counter on empty buffer
        return true;
    }
    
    PRINT_SECTION("DATA UPLOAD CYCLE");
    
    // Drain all data from buffer
    auto allData = ringBuffer.drain_all();
    
    print("  [INFO] Preparing %zu compressed batches for upload\n", allData.size());
    
    // Check available heap before attempting upload
    size_t freeHeap = ESP.getFreeHeap();
    print("  [INFO] Free heap: %zu bytes\n", freeHeap);
    
    if (freeHeap < 20000) {  // Require at least 20KB free heap
        PRINT_ERROR("Insufficient heap memory (%zu bytes). Skipping upload.", freeHeap);
        // Don't restore data - will retry next cycle when memory is available
        return false;
    }
    
    // Attempt upload with retry logic
    bool success = false;
    for (uint8_t attempt = 0; attempt <= maxRetryAttempts; attempt++) {
        if (attempt > 0) {
            unsigned long backoffDelay = calculateBackoffDelay(attempt);
            PRINT_WARNING("Retry attempt %u/%u after %lu ms backoff...", 
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
                PRINT_ERROR("Upload attempt %u failed, will retry...", attempt + 1);
            }
        }
    }
    
    if (!success) {
        PRINT_ERROR("Upload failed after %u attempts", maxRetryAttempts + 1);
        PRINT_WARNING("Restoring %zu packets to buffer for next cycle", allData.size());
        
        // Restore data to buffer for next upload cycle
        for (const auto& entry : allData) {
            if (ringBuffer.size() >= 20) {
                PRINT_ERROR("Buffer full! Data packet lost!");
                break;
            }
            ringBuffer.push(entry);
        }
        lastFailedUploadTime = millis();
    }
    
    return success;
}

bool DataUploader::attemptUpload(const std::vector<SmartCompressedData>& allData) {
    
    print("  [INFO] Sending %zu packets to server\n", allData.size());
    
    // Create WiFiClient with extended connection timeout
    WiFiClient client;
    client.setTimeout(15000);  // 15 second connection timeout in MILLISECONDS
    
    HTTPClient http;
    http.begin(client, uploadURL);  // Use our WiFiClient with custom timeout
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(15000);  // 15 second HTTP timeout (in milliseconds)
    
    // Feed the watchdog during long operations
    yield();
    
    // Build JSON payload - reduced size to prevent stack overflow
    DynamicJsonDocument doc(6144);  // Reduced from 8192 to 6144
    
    doc["device_id"] = deviceID;
    doc["timestamp"] = millis();
    doc["data_type"] = "compressed_sensor_batch";
    doc["total_samples"] = allData.size();
    
    // Build register mapping from first entry
    JsonObject registerMapping = doc["register_mapping"].to<JsonObject>();
    if (!allData.empty()) {
        const auto& firstEntry = allData[0];
        for (size_t i = 0; i < firstEntry.registerCount && i < REGISTER_COUNT; i++) {
            char key[8];
            snprintf(key, sizeof(key), "%zu", i);
            registerMapping[key] = REGISTER_MAP[firstEntry.registers[i]].name;
        }
    }
    
    // Compressed data packets
    JsonArray compressedPackets = doc["compressed_data"].to<JsonArray>();
    
    size_t totalOriginalBytes = 0;
    size_t totalCompressedBytes = 0;
    
    for (const auto& entry : allData) {
        // Feed watchdog during packet processing
        yield();
        
        JsonObject packet = compressedPackets.createNestedObject();
        
        // Compressed binary data (Base64 encoded)
        char base64Buffer[256];
        convertBinaryToBase64(entry.binaryData, base64Buffer, sizeof(base64Buffer));
        packet["compressed_binary"] = base64Buffer;
        
        // Decompression metadata
        JsonObject decompMeta = packet["decompression_metadata"].to<JsonObject>();
        decompMeta["method"] = entry.compressionMethod;
        decompMeta["register_count"] = entry.registerCount;
        decompMeta["original_size_bytes"] = entry.originalSize;
        decompMeta["compressed_size_bytes"] = entry.binaryData.size();
        decompMeta["timestamp"] = entry.timestamp;
        
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
    JsonObject sessionSummary = doc["session_summary"].to<JsonObject>();
    sessionSummary["total_original_bytes"] = totalOriginalBytes;
    sessionSummary["total_compressed_bytes"] = totalCompressedBytes;
    sessionSummary["overall_academic_ratio"] = (totalOriginalBytes > 0) ? 
        (float)totalCompressedBytes / (float)totalOriginalBytes : 1.0f;
    sessionSummary["overall_savings_percent"] = (totalOriginalBytes > 0) ? 
        (1.0f - (float)totalCompressedBytes / (float)totalOriginalBytes) * 100.0f : 0.0f;
    
    // Print compression statistics
    float savingsPercent = (totalOriginalBytes > 0) ? 
        (1.0f - (float)totalCompressedBytes / (float)totalOriginalBytes) * 100.0f : 0.0f;
    
    print("  [INFO] Compression: %zu bytes -> %zu bytes (%.1f%% savings)\n", 
          totalOriginalBytes, totalCompressedBytes, savingsPercent);
    print("  [INFO] Sending %zu packets\n", allData.size());
    
    // INDUSTRY STANDARD APPROACH:
    // Send the JSON with compressed data embedded (base64 encoded)
    // The server will extract and decompress the compressed_data field
    
    // Serialize the JSON to string
    char jsonString[2048];
    serializeJson(doc, jsonString, sizeof(jsonString));
    
    print("  [INFO] JSON payload size: %zu bytes\n", strlen(jsonString));
    
    // Apply Security Layer to the JSON string
    // The security layer will handle Base64 encoding for transport
    PRINT_PROGRESS("Securing payload with HMAC...");
    
    // Feed watchdog before security operation
    yield();
    
    char* securedPayload = new char[8192];
    if (!securedPayload) {
        PRINT_ERROR("Failed to allocate memory for secured payload");
        http.end();
        uploadFailures++;
        return false;
    }
    
    // Pass false for isCompressed since we're sending JSON (not raw binary)
    // The JSON contains compressed data in the "compressed_data" field
    if (!SecurityLayer::securePayload(jsonString, securedPayload, 8192, false)) {
        PRINT_ERROR("Payload security failed");
        delete[] securedPayload;
        http.end();
        uploadFailures++;
        return false;
    }
    
    PRINT_SUCCESS("Payload secured successfully");
    PRINT_PROGRESS("Uploading to server...");
    
    // Feed watchdog before HTTP POST
    yield();
    
    int httpResponseCode = http.POST((uint8_t*)securedPayload, strlen(securedPayload));
    
    bool success = false;
    if (httpResponseCode == 200) {
        PRINT_SUCCESS("Upload successful! (HTTP 200)");
        uploadCount++;
        totalBytesUploaded += strlen(securedPayload);
        success = true;
    } else {
        PRINT_ERROR("Upload failed - HTTP %d", httpResponseCode);
        if (httpResponseCode > 0) {
            String errorResponse = http.getString();
            PRINT_DATA("Error Response", "%s", errorResponse.c_str());
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
    
    for (size_t i = 0; i < binaryData.size() && pos < resultSize - 5; i += 3) {
        uint32_t value = binaryData[i] << 16;
        if (i + 1 < binaryData.size()) value |= binaryData[i + 1] << 8;
        if (i + 2 < binaryData.size()) value |= binaryData[i + 2];
        
        result[pos++] = chars[(value >> 18) & 0x3F];
        result[pos++] = chars[(value >> 12) & 0x3F];
        result[pos++] = chars[(value >> 6) & 0x3F];
        result[pos++] = chars[value & 0x3F];
    }
    
    while (pos % 4 && pos < resultSize - 1) result[pos++] = '=';
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
    print("[DataUploader] Queue cleared\n");
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
    print("[DataUploader] Statistics reset\n");
}

void DataUploader::printStats() {
    print("\n========== DATA UPLOADER STATISTICS ==========\n");
    print("  Successful Uploads:  %lu\n", uploadCount);
    print("  Failed Uploads:      %lu\n", uploadFailures);
    print("  Total Bytes Sent:    %zu\n", totalBytesUploaded);
    
    unsigned long total = uploadCount + uploadFailures;
    if (total > 0) {
        float successRate = (uploadCount * 100.0f) / total;
        print("  Success Rate:        %.2f%%\n", successRate);
    }
    
    print("  Current Queue Size:  %zu/20\n", ringBuffer.size());
    print("==============================================\n\n");
}

void DataUploader::setUploadURL(const char* url) {
    strncpy(uploadURL, url, sizeof(uploadURL) - 1);
    uploadURL[sizeof(uploadURL) - 1] = '\0';
    print("[DataUploader] Upload URL updated: %s\n", uploadURL);
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
    print("[DataUploader] Max retry attempts set to %u\n", maxRetries);
}

uint8_t DataUploader::getMaxRetries() {
    return maxRetryAttempts;
}

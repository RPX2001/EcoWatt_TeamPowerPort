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
        return true;
    }
    
    PRINT_SECTION("DATA UPLOAD CYCLE");
    
    // Drain all data from buffer
    auto allData = ringBuffer.drain_all();
    
    print("  [INFO] Preparing %zu compressed batches for upload\n", allData.size());
    
    HTTPClient http;
    http.begin(uploadURL);
    http.addHeader("Content-Type", "application/json");
    
    // Build JSON payload
    DynamicJsonDocument doc(8192);
    
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
    
    // Serialize to string
    char jsonString[2048];
    serializeJson(doc, jsonString, sizeof(jsonString));
    
    // Print compression statistics
    float savingsPercent = (totalOriginalBytes > 0) ? 
        (1.0f - (float)totalCompressedBytes / (float)totalOriginalBytes) * 100.0f : 0.0f;
    
    print("  [INFO] Compression: %zu bytes -> %zu bytes (%.1f%% savings)\n", 
          totalOriginalBytes, totalCompressedBytes, savingsPercent);
    print("  [INFO] Sending %zu packets\n", allData.size());
    
    // Apply Security Layer
    PRINT_PROGRESS("Encrypting payload with AES-128...");
    
    char* securedPayload = new char[8192];
    if (!securedPayload) {
        PRINT_ERROR("Failed to allocate memory for secured payload");
        http.end();
        uploadFailures++;
        
        // Restore data to buffer for retry
        for (const auto& entry : allData) {
            ringBuffer.push(entry);
        }
        return false;
    }
    
    if (!SecurityLayer::securePayload(jsonString, securedPayload, 8192)) {
        PRINT_ERROR("Payload encryption failed");
        delete[] securedPayload;
        http.end();
        uploadFailures++;
        
        // Restore data to buffer
        for (const auto& entry : allData) {
            ringBuffer.push(entry);
        }
        return false;
    }
    
    PRINT_SUCCESS("Payload encrypted successfully");
    PRINT_PROGRESS("Uploading to server...");
    
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
        PRINT_WARNING("Restoring data to buffer for retry...");
        
        // Restore data to buffer
        for (const auto& entry : allData) {
            ringBuffer.push(entry);
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

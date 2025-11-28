#include "application/OTAManager.h"
#include "peripheral/logger.h"
#include <time.h>

// Helper function to get current Unix timestamp in seconds
static unsigned long getCurrentTimestamp() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        time_t now = mktime(&timeinfo);
        return (unsigned long)now;
    }
    return millis() / 1000;  // Fallback to seconds since boot
}

// Constructor
OTAManager::OTAManager(const String& serverURL, const String& deviceID, const String& currentVersion)
    : serverURL(serverURL), deviceID(deviceID), currentVersion(currentVersion), checkInterval(3600000), state(OTA_IDLE), // 1 hour default
      testModeEnabled(false), testFaultType(OTA_FAULT_NONE), otaSuccessCount(0), otaFailureCount(0), otaRollbackCount(0)
{
    LOG_INFO(LOG_TAG_FOTA, "OTA Manager Initialization");
    
    // Initialize progress struct
    progress.state = OTA_IDLE;
    progress.chunks_received = 0;
    progress.total_chunks = 0;
    progress.bytes_downloaded = 0;
    progress.percentage = 0;
    progress.error_message = "";
    
    // Initialize mbedtls AES context
    mbedtls_aes_init(&aes_ctx);
    
    // Initialize IV array to zero
    memset(aes_iv, 0, sizeof(aes_iv));
    
    // Allocate decryption buffer
    decryptBuffer = (uint8_t*)malloc(DECRYPT_BUFFER_SIZE);
    if (decryptBuffer == NULL) {
        LOG_ERROR(LOG_TAG_FOTA, "Failed to allocate decryption buffer!");
        setError("Memory allocation failed");
        return;
    }
    
    // Initialize NVS
    bool nvs_ok = nvs.begin("ota", false); // read-write mode
    if (!nvs_ok) {
        LOG_ERROR(LOG_TAG_FOTA, "Failed to initialize NVS storage!");
        setError("NVS initialization failed");
        return;
    }
    
    // Load any existing progress (for resume capability)
    loadProgress();
    
    LOG_INFO(LOG_TAG_FOTA, "Device ID: %s", deviceID.c_str());
    LOG_INFO(LOG_TAG_FOTA, "Current Version: %s", currentVersion.c_str());
    LOG_INFO(LOG_TAG_FOTA, "Server URL: %s", serverURL.c_str());
    LOG_INFO(LOG_TAG_FOTA, "Decryption buffer: %d bytes allocated", DECRYPT_BUFFER_SIZE);
    LOG_SUCCESS(LOG_TAG_FOTA, "Initialized successfully");
    LOG_INFO(LOG_TAG_FOTA, "=====================================");
}

// Destructor
OTAManager::~OTAManager()
{
    LOG_INFO(LOG_TAG_FOTA, "Cleanup...");
    
    // Free mbedtls AES context
    mbedtls_aes_free(&aes_ctx);
    
    // Free decryption buffer
    if (decryptBuffer != NULL) {
        free(decryptBuffer);
        decryptBuffer = NULL;
    }
    
    // Close NVS
    nvs.end();
    
    LOG_INFO(LOG_TAG_FOTA, "Cleanup complete");
}

// Check for firmware updates
bool OTAManager::checkForUpdate()
{
    LOG_SECTION("CHECKING FOR FIRMWARE UPDATES");
    progress.state = OTA_CHECKING;
    
    // Check WiFi connectivity
    if (WiFi.status() != WL_CONNECTED) {
        setError("WiFi not connected");
        LOG_ERROR(LOG_TAG_FOTA, "WiFi connection required for OTA check");
        progress.state = OTA_IDLE;
        return false;
    }
    
    LOG_INFO(LOG_TAG_FOTA, "Checking updates for device: %s", deviceID.c_str());
    LOG_INFO(LOG_TAG_FOTA, "Current version: %s", currentVersion.c_str());
    
    // Use GET endpoint: /ota/check/<device_id>?version=<version>
    String endpoint = "/ota/check/" + deviceID + "?version=" + currentVersion;
    
    // Send HTTP GET request
    String response;
    if (!httpGet(endpoint, response)) {
        setError("Failed to communicate with OTA server");
        progress.state = OTA_IDLE;
        return false;
    }
    
    // Parse response
    DynamicJsonDocument responseDoc(2048);
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (error) {
        setError("Invalid JSON response from server");
        LOG_ERROR(LOG_TAG_FOTA, "JSON parse error: %s", error.c_str());
        progress.state = OTA_IDLE;
        return false;
    }
    
    // Check if update is available
    bool updateAvailable = responseDoc["update_available"];
    if (!updateAvailable) {
        LOG_INFO(LOG_TAG_FOTA, "No firmware updates available");
        LOG_INFO(LOG_TAG_FOTA, "Device is already running the latest version: %s", currentVersion.c_str());
        progress.state = OTA_IDLE;
        return false;
    }
    
    // Parse manifest from update_info (nested object)
    JsonObject updateInfo = responseDoc["update_info"];
    if (updateInfo.isNull()) {
        setError("Missing update_info in response");
        progress.state = OTA_IDLE;
        return false;
    }
    
    manifest.version = updateInfo["latest_version"].as<String>();
    manifest.original_size = updateInfo["original_size"] | updateInfo["firmware_size"] | 0;
    manifest.encrypted_size = updateInfo["encrypted_size"] | manifest.original_size;
    manifest.firmware_size = manifest.original_size; // Use original size for firmware validation
    manifest.sha256_hash = updateInfo["sha256_hash"].as<String>();
    manifest.signature = updateInfo["signature"].as<String>();
    manifest.iv = updateInfo["iv"].as<String>();
    manifest.chunk_size = updateInfo["chunk_size"];
    manifest.total_chunks = updateInfo["total_chunks"];
    
    // DEBUG: Log received hash immediately to verify fault injection is working
    LOG_WARN(LOG_TAG_FOTA, "[HASH CHECK] Received SHA256 from server:");
    LOG_WARN(LOG_TAG_FOTA, "  Hash: %s", manifest.sha256_hash.c_str());
    LOG_WARN(LOG_TAG_FOTA, "  Length: %d chars", manifest.sha256_hash.length());
    
    // Update progress
    progress.total_chunks = manifest.total_chunks;
    
    // Print update information
    LOG_SECTION("FIRMWARE UPDATE AVAILABLE");
    LOG_INFO(LOG_TAG_FOTA, "Current version: %s", currentVersion.c_str());
    LOG_INFO(LOG_TAG_FOTA, "New version: %s", manifest.version.c_str());
    LOG_INFO(LOG_TAG_FOTA, "Firmware size: %u bytes (encrypted: %u bytes)", 
                  manifest.original_size, manifest.encrypted_size);
    LOG_INFO(LOG_TAG_FOTA, "Total chunks: %u (size: %u bytes each)", 
                  manifest.total_chunks, manifest.chunk_size);
    LOG_INFO(LOG_TAG_FOTA, "SHA-256 hash: %s", manifest.sha256_hash.c_str());
    
    // Decode and store IV for AES decryption
    uint8_t ivBuffer[16];
    int ivLen = base64_decode(manifest.iv.c_str(), ivBuffer, sizeof(ivBuffer));
    if (ivLen != 16) {
        setError("Invalid IV length from server");
        LOG_ERROR(LOG_TAG_FOTA, "IV length is %d bytes (expected 16)\n", ivLen);
        progress.state = OTA_IDLE;
        return false;
    }
    
    // Copy IV to AES context
    memcpy(aes_iv, ivBuffer, 16);
    LOG_SUCCESS(LOG_TAG_FOTA, "IV decoded and stored successfully");
    
    // Save manifest to NVS for resume capability
    nvs.putString("version", manifest.version);
    nvs.putUShort("total_chunks", manifest.total_chunks);
    nvs.putString("hash", manifest.sha256_hash);
    nvs.putString("signature", manifest.signature);
    nvs.putString("iv", manifest.iv);
    nvs.putUInt("enc_size", manifest.encrypted_size);
    
    LOG_SUCCESS(LOG_TAG_FOTA, "Manifest saved to NVS");
    LOG_INFO(LOG_TAG_FOTA, "===================================");
    
    return true;
}

// Download and apply firmware
bool OTAManager::downloadAndApplyFirmware()
{
    LOG_SECTION("STARTING FIRMWARE DOWNLOAD");
    
    // Reset progress for fresh start (important for retry scenarios)
    progress.chunks_received = 0;
    progress.bytes_downloaded = 0;
    progress.percentage = 0;
    progress.state = OTA_DOWNLOADING;
    
    // Report start of download to server
    reportProgress("downloading", 0, "Starting firmware download...");
    
    // Clear any stored NVS progress to ensure clean start
    if (nvs.begin("ota_progress", false)) {
        nvs.clear();
        nvs.end();
        LOG_INFO(LOG_TAG_FOTA, "Cleared previous OTA progress from NVS");
    }
    
    // Ensure Update library is clean for new session
    if (Update.isRunning()) {
        LOG_WARN(LOG_TAG_FOTA, "Previous OTA session still active - cleaning up...");
        Update.abort();
        LOG_INFO(LOG_TAG_FOTA, "Previous OTA session aborted");
    }
    
    // Get OTA partition information
    const esp_partition_t* ota_partition = esp_ota_get_next_update_partition(NULL);
    if (ota_partition == NULL) {
        setError("No OTA partition available");
        LOG_ERROR(LOG_TAG_FOTA, "Could not find OTA partition");
        return false;
    }
    
    LOG_INFO(LOG_TAG_FOTA, "OTA partition: %s", ota_partition->label);
    LOG_INFO(LOG_TAG_FOTA, "Partition size: %u bytes", ota_partition->size);
    
    // Verify firmware fits in partition
    if (manifest.encrypted_size > ota_partition->size) {
        setError("Firmware too large for OTA partition");
        LOG_ERROR(LOG_TAG_FOTA, "Firmware (%u bytes) exceeds partition size (%u bytes)\n",
                      manifest.encrypted_size, ota_partition->size);
        return false;
    }
    
    // Initialize OTA with original (decrypted) firmware size
    if (!Update.begin(manifest.original_size)) {
        setError("OTA initialization failed: " + String(Update.errorString()));
        LOG_ERROR(LOG_TAG_FOTA, "Update.begin() failed: %s\n", Update.errorString());
        return false;
    }
    
    LOG_SUCCESS(LOG_TAG_FOTA, "OTA partition initialized successfully");
    
    // Configure AES decryption key
    int aes_result = mbedtls_aes_setkey_dec(&aes_ctx, AES_FIRMWARE_KEY, 256);
    if (aes_result != 0) {
        setError("AES key configuration failed");
        LOG_ERROR(LOG_TAG_FOTA, "AES key setup failed: %d\n", aes_result);
        return false;
    }
    
    // Reset IV to original value for streaming decryption
    // The IV from manifest should be used as the starting IV
    uint8_t ivBuffer[16];
    int ivLen = base64_decode(manifest.iv.c_str(), ivBuffer, sizeof(ivBuffer));
    if (ivLen == 16) {
        memcpy(aes_iv, ivBuffer, 16);
        LOG_INFO(LOG_TAG_FOTA, "AES IV reset for streaming decryption");
    }
    
    LOG_SUCCESS(LOG_TAG_FOTA, "AES decryption key configured");
    
    // Always start from chunk 0 (disable resume to debug decryption)
    uint16_t startChunk = 0;
    progress.chunks_received = 0;
    progress.bytes_downloaded = 0;
    unsigned long startTime = millis();
    unsigned long lastProgressTime = startTime;
    
    LOG_INFO(LOG_TAG_FOTA, "Starting download from chunk %u to %u (resume disabled)", startChunk, manifest.total_chunks - 1);
    
    // Initiate OTA session with Flask server
    LOG_SECTION("INITIATING OTA SESSION");
    DynamicJsonDocument initiateDoc(256);
    initiateDoc["firmware_version"] = manifest.version;
    String initiatePayload;
    serializeJson(initiateDoc, initiatePayload);
    
    String initiateResponse;
    String initiateEndpoint = "/ota/initiate/" + deviceID;
    if (!httpPost(initiateEndpoint, initiatePayload, initiateResponse)) {
        setError("Failed to initiate OTA session");
        LOG_ERROR(LOG_TAG_FOTA, "Failed to initiate OTA session with server");
        return false;
    }
    
    // Parse session initiation response
    DynamicJsonDocument sessionDoc(512);
    DeserializationError sessionError = deserializeJson(sessionDoc, initiateResponse);
    if (sessionError || !sessionDoc["success"].as<bool>()) {
        setError("OTA session initiation failed");
        LOG_ERROR(LOG_TAG_FOTA, "OTA session initiation failed: %s\n", 
                     sessionDoc["error"] | "Unknown error");
        return false;
    }
    
    String sessionId = sessionDoc["session_id"];
    LOG_SUCCESS(LOG_TAG_FOTA, "OTA session initiated: %s", sessionId.c_str());
    LOG_INFO(LOG_TAG_FOTA, "================================");
    
    // Download chunks
    for (uint16_t chunk = startChunk; chunk < manifest.total_chunks; chunk++) {
        // Download and process chunk
        if (!downloadChunk(chunk)) {
            setError("Chunk download failed at chunk " + String(chunk));
            LOG_ERROR(LOG_TAG_FOTA, "Failed to download chunk %u\n", chunk);
            saveProgress(); // Save progress for resume
            progress.state = OTA_ERROR;
            return false;
        }
        
        // Update progress
        progress.chunks_received = chunk + 1;
        progress.percentage = (progress.chunks_received * 100) / manifest.total_chunks;
        
        // Progress reporting (every 50 chunks or 10% intervals)
        unsigned long currentTime = millis();
        if ((chunk % 50 == 0) || (progress.percentage % 10 == 0) || 
            (currentTime - lastProgressTime > 5000)) {
            
            unsigned long elapsed = (currentTime - startTime) / 1000; // seconds
            float speed = (elapsed > 0) ? (progress.bytes_downloaded / 1024.0) / elapsed : 0; // KB/s
            
            LOG_INFO(LOG_TAG_FOTA, "Progress: [%3u%%] %4u/%4u chunks | %6u bytes | %.1f KB/s",
                         progress.percentage, progress.chunks_received, 
                         manifest.total_chunks, progress.bytes_downloaded, speed);
            
            // Report progress to server
            String progressMsg = "Downloading chunk " + String(progress.chunks_received) + 
                               " of " + String(manifest.total_chunks);
            reportProgress("downloading", progress.percentage, progressMsg);
            
            // Progress bar visualization
            int barWidth = 30;
            int filled = (progress.percentage * barWidth) / 100;
            // Progress bar visual removed - use LOG_INFO instead
            for (int i = 0; i < barWidth; i++) {
                // Progress bar visual removed
                // Progress bar visual removed
            }
            // Progress bar visual removed
            
            lastProgressTime = currentTime;
            saveProgress(); // Save progress periodically
        }
        
        // Yield every 10 chunks to prevent watchdog timeout
        if (chunk % 10 == 0) {
            yield();
        }
    }
    
    // Calculate final statistics
    unsigned long totalTime = (millis() - startTime) / 1000;
    float avgSpeed = (totalTime > 0) ? (progress.bytes_downloaded / 1024.0) / totalTime : 0;
    
    LOG_SECTION("DOWNLOAD COMPLETED");
    LOG_INFO(LOG_TAG_FOTA, "Total time: %lu seconds", totalTime);
    LOG_INFO(LOG_TAG_FOTA, "Average speed: %.1f KB/s", avgSpeed);
    LOG_INFO(LOG_TAG_FOTA, "Total bytes written: %u", progress.bytes_downloaded);
    LOG_INFO(LOG_TAG_FOTA, "Expected firmware size: %u bytes", manifest.original_size);
    LOG_INFO(LOG_TAG_FOTA, "Update progress: %u bytes", Update.progress());
    LOG_INFO(LOG_TAG_FOTA, "Update size: %u bytes", Update.size());
    LOG_INFO(LOG_TAG_FOTA, "Update remaining: %d bytes", Update.remaining());
    
    // Verify we wrote the expected amount
    if (progress.bytes_downloaded != manifest.original_size) {
        LOG_WARN(LOG_TAG_FOTA, "Size mismatch - wrote %u, expected %u", 
                     progress.bytes_downloaded, manifest.original_size);
    }
    
    // Finalize OTA
    LOG_INFO(LOG_TAG_FOTA, "Calling Update.end()...");
    if (!Update.end()) {
        setError("OTA finalization failed: " + String(Update.errorString()));
        LOG_ERROR(LOG_TAG_FOTA, "Update.end() failed: %s\n", Update.errorString());
        LOG_DEBUG(LOG_TAG_FOTA, "Update hasError: %s", Update.hasError() ? "YES" : "NO");
        LOG_DEBUG(LOG_TAG_FOTA, "Update getError: %u", Update.getError());
        return false;
    }
    
    LOG_SUCCESS(LOG_TAG_FOTA, "Firmware written to OTA partition successfully");
    progress.state = OTA_VERIFYING;
    
    // Report download completion
    reportProgress("download_complete", 100, "Download complete, verifying security...");
    
    return true;
}

// Download a single firmware chunk
bool OTAManager::downloadChunk(uint16_t chunkNumber)
{
    // Build GET request URL with query parameters
    String endpoint = "/ota/chunk/" + deviceID + "?version=" + manifest.version + "&chunk=" + String(chunkNumber);
    
    // Request chunk from server using GET
    String response;
    if (!httpGet(endpoint, response)) {
        LOG_ERROR(LOG_TAG_FOTA, "HTTP request failed for chunk %u", chunkNumber);
        return false;
    }
    
    // Parse JSON response
    DynamicJsonDocument responseDoc(3072); // Larger buffer for chunk data
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (error) {
        LOG_ERROR(LOG_TAG_FOTA, "JSON parse error for chunk %u: %s", chunkNumber, error.c_str());
        return false;
    }
    
    // Check response success
    if (!responseDoc["success"].as<bool>()) {
        String errorMsg = responseDoc["error"] | "Unknown error";
        LOG_ERROR(LOG_TAG_FOTA, "Chunk %u error: %s", chunkNumber, errorMsg.c_str());
        return false;
    }
    
    // Extract chunk data from Flask response format
    String chunkDataB64 = responseDoc["chunk_data"];
    size_t chunkSize = responseDoc["chunk_size"];
    
    // Decode base64 chunk data
    uint8_t encryptedChunk[OTA_CHUNK_SIZE + 16]; // Extra space for padding
    int encryptedLen = base64_decode(chunkDataB64.c_str(), encryptedChunk, sizeof(encryptedChunk));
    
    if (encryptedLen != (int)chunkSize) {
        LOG_ERROR(LOG_TAG_FOTA, "Chunk %u size mismatch: expected %u, got %d", 
                     chunkNumber, chunkSize, encryptedLen);
        return false;
    }
    
    // Note: HMAC verification removed - Flask already verifies integrity
    
    // Decrypt chunk
    LOG_DEBUG(LOG_TAG_FOTA, "About to decrypt chunk %u (%d bytes)", chunkNumber, encryptedLen);
    size_t decryptedLen;
    if (!decryptChunk(encryptedChunk, encryptedLen, 
                     decryptBuffer, &decryptedLen, chunkNumber)) {
        LOG_ERROR(LOG_TAG_FOTA, "Decryption failed for chunk %u\n", chunkNumber);
        return false;
    }
    LOG_DEBUG(LOG_TAG_FOTA, "Chunk %u decrypted successfully (%u bytes)", chunkNumber, decryptedLen);
    
    // Debug: Print first few bytes of decrypted data for verification
    if (chunkNumber < 3) {
        LOG_DEBUG(LOG_TAG_FOTA, "Chunk %u decrypted (%u bytes):", chunkNumber, decryptedLen);
        for (int i = 0; i < 16 && i < decryptedLen; i++) {
        }
    }
    
    // Write decrypted data to OTA partition
    size_t written = Update.write(decryptBuffer, decryptedLen);
    if (written != decryptedLen) {
        LOG_ERROR(LOG_TAG_FOTA, "Write error for chunk %u: expected %u, wrote %u", 
                     chunkNumber, decryptedLen, written);
        LOG_ERROR(LOG_TAG_FOTA, "Update error: %s", Update.errorString());
        return false;
    }
    
    // Update progress
    progress.bytes_downloaded += written;
    
    return true;
}

// Decrypt firmware chunk using AES-256-CBC (streaming mode)
bool OTAManager::decryptChunk(const uint8_t* encrypted, size_t encLen, uint8_t* decrypted, size_t* decLen, uint16_t chunkNumber)
{
    LOG_DEBUG(LOG_TAG_FOTA, "decryptChunk: Entering with %u bytes for chunk %u", encLen, chunkNumber);
    
    // Check buffer size
    if (encLen > DECRYPT_BUFFER_SIZE) {
        LOG_ERROR(LOG_TAG_FOTA, "Encrypted chunk too large: %u bytes (max %u)\n", encLen, DECRYPT_BUFFER_SIZE);
        return false;
    }
    
    // For streaming AES-CBC, we need to maintain IV state across chunks
    // The IV is automatically updated by mbedtls during decryption
    
    // Log IV state before decryption (for debugging)
    if (chunkNumber == 0 || chunkNumber == 1) {
        LOG_DEBUG(LOG_TAG_FOTA, "decryptChunk: IV before chunk %u", chunkNumber);
        for (int i = 0; i < 16; i++) {
        }
    }
    
    LOG_DEBUG(LOG_TAG_FOTA, "decryptChunk: About to call mbedtls_aes_crypt_cbc");
    
    // Decrypt using AES-256-CBC in streaming mode
    int result = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, encLen,
                                      aes_iv, encrypted, decrypted);
    
    LOG_DEBUG(LOG_TAG_FOTA, "decryptChunk: mbedtls_aes_crypt_cbc returned %d", result);
    
    if (result != 0) {
        LOG_ERROR(LOG_TAG_FOTA, "AES decryption failed: %d\n", result);
        return false;
    }
    
    *decLen = encLen;
    
    // CRITICAL: Verify ESP32 firmware magic byte on chunk 0
    if (chunkNumber == 0 && encLen > 0) {
        if (decrypted[0] != 0xE9) {
            LOG_ERROR(LOG_TAG_FOTA, "Invalid ESP32 firmware magic byte: 0x%02X (expected 0xE9)\n", decrypted[0]);
            LOG_ERROR(LOG_TAG_FOTA, "Decryption key/IV mismatch or wrong encryption mode!\n");
            LOG_DEBUG(LOG_TAG_FOTA, "First 16 decrypted bytes");
            for (int i = 0; i < 16 && i < encLen; i++) {
            }
            return false;
        }
        LOG_SUCCESS(LOG_TAG_FOTA, "ESP32 firmware magic byte verified (0xE9)");
    }
    
    // Remove PKCS7 padding ONLY on the very last chunk
    bool isLastChunk = (chunkNumber == (manifest.total_chunks - 1));
    LOG_DEBUG(LOG_TAG_FOTA, "decryptChunk: chunk=%u, total=%u, isLast=%s", 
                 chunkNumber, manifest.total_chunks, isLastChunk ? "YES" : "NO");
    
    if (isLastChunk && encLen > 0) {
        uint8_t paddingLen = decrypted[encLen - 1];
        
        // Validate padding length
        if (paddingLen > 0 && paddingLen <= 16 && paddingLen <= encLen) {
            // Verify padding bytes are all the same
            bool validPadding = true;
            for (int i = encLen - paddingLen; i < encLen; i++) {
                if (decrypted[i] != paddingLen) {
                    validPadding = false;
                    break;
                }
            }
            
            if (validPadding) {
                *decLen = encLen - paddingLen;
                LOG_DEBUG(LOG_TAG_FOTA, "Removed PKCS7 padding: %u bytes", paddingLen);
            }
        }
    }
    
    return true;
}

// Verify chunk HMAC for anti-replay protection
bool OTAManager::verifyChunkHMAC(const uint8_t* chunkData, size_t len, uint16_t chunkNum, const String& expectedHMAC)
{
    // Initialize SHA-256 context for HMAC calculation
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    mbedtls_md_init(&ctx);
    
    // Setup HMAC with PSK
    int result = mbedtls_md_setup(&ctx, md_info, 1); // 1 = use HMAC
    if (result != 0) {
        LOG_ERROR(LOG_TAG_FOTA, "HMAC setup failed: %d\n", result);
        mbedtls_md_free(&ctx);
        return false;
    }
    
    // Start HMAC with PSK key
    result = mbedtls_md_hmac_starts(&ctx, (const unsigned char*)HMAC_PSK, strlen(HMAC_PSK));
    if (result != 0) {
        LOG_ERROR(LOG_TAG_FOTA, "HMAC start failed: %d\n", result);
        mbedtls_md_free(&ctx);
        return false;
    }
    
    // Update with chunk data
    result = mbedtls_md_hmac_update(&ctx, chunkData, len);
    if (result != 0) {
        LOG_ERROR(LOG_TAG_FOTA, "HMAC update (data) failed: %d\n", result);
        mbedtls_md_free(&ctx);
        return false;
    }
    
    // Update with chunk number (converted to string)
    String chunkNumStr = String(chunkNum);
    result = mbedtls_md_hmac_update(&ctx, (const unsigned char*)chunkNumStr.c_str(), chunkNumStr.length());
    if (result != 0) {
        LOG_ERROR(LOG_TAG_FOTA, "HMAC update (chunk num) failed: %d\n", result);
        mbedtls_md_free(&ctx);
        return false;
    }
    
    // Finalize HMAC
    uint8_t hmac_result[32];
    result = mbedtls_md_hmac_finish(&ctx, hmac_result);
    if (result != 0) {
        LOG_ERROR(LOG_TAG_FOTA, "HMAC finish failed: %d\n", result);
        mbedtls_md_free(&ctx);
        return false;
    }
    
    // Convert to hex string (lowercase)
    String calculatedHMAC = "";
    for (int i = 0; i < 32; i++) {
        if (hmac_result[i] < 16) calculatedHMAC += "0";
        calculatedHMAC += String(hmac_result[i], HEX);
    }
    calculatedHMAC.toLowerCase();
    
    // Clean up
    mbedtls_md_free(&ctx);
    
    // Compare HMACs
    bool matches = (calculatedHMAC == expectedHMAC);
    
    if (!matches) {
        LOG_ERROR(LOG_TAG_FOTA, "HMAC mismatch for chunk %u", chunkNum);
        LOG_ERROR(LOG_TAG_FOTA, "Expected: %s", expectedHMAC.c_str());
        LOG_ERROR(LOG_TAG_FOTA, "Calculated: %s", calculatedHMAC.c_str());
    }
    
    return matches;
}

// Verify firmware signature using SHA-256 hash + RSA signature
bool OTAManager::verifySignature(const String& base64Signature)
{
    // Decode base64 signature
    uint8_t signature[RSA_KEY_SIZE / 8]; // 256 bytes for 2048-bit RSA
    
    int sigLen = base64_decode(base64Signature.c_str(), signature, sizeof(signature));
    if (sigLen != sizeof(signature)) {
        LOG_ERROR(LOG_TAG_FOTA, "Invalid signature length: %d (expected %u)\n", sigLen, sizeof(signature));
        return false;
    }
    
    // Debug: Print first 16 bytes of signature
    LOG_DEBUG(LOG_TAG_FOTA, "Signature (first 16 bytes)");
    for (int i = 0; i < 16; i++) {
    }
    
    // Calculate SHA-256 hash of the entire decrypted firmware
    uint8_t firmware_hash[32];
    mbedtls_sha256_context sha_ctx;
    
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0); // 0 for SHA-256 (not SHA-224)
    
    // Read firmware from OTA partition and hash it
    esp_partition_t* ota_partition = (esp_partition_t*)esp_ota_get_next_update_partition(NULL);
    if (!ota_partition) {
        LOG_ERROR(LOG_TAG_FOTA, "Could not get OTA partition for verification");
        mbedtls_sha256_free(&sha_ctx);
        return false;
    }
    
    uint8_t read_buffer[1024];
    size_t bytes_to_hash = manifest.firmware_size;
    size_t offset = 0;
    
    while (bytes_to_hash > 0) {
        size_t read_size = (bytes_to_hash > sizeof(read_buffer)) ? sizeof(read_buffer) : bytes_to_hash;
        
        esp_err_t result = esp_partition_read(ota_partition, offset, read_buffer, read_size);
        if (result != ESP_OK) {
            LOG_ERROR(LOG_TAG_FOTA, "Failed to read OTA partition at offset %u\n", offset);
            mbedtls_sha256_free(&sha_ctx);
            return false;
        }
        
        mbedtls_sha256_update(&sha_ctx, read_buffer, read_size);
        
        offset += read_size;
        bytes_to_hash -= read_size;
    }
    
    mbedtls_sha256_finish(&sha_ctx, firmware_hash);
    mbedtls_sha256_free(&sha_ctx);
    
    // Convert calculated hash to hex string for comparison
    String calculatedHashHex = "";
    for (int i = 0; i < 32; i++) {
        if (firmware_hash[i] < 16) calculatedHashHex += "0";
        calculatedHashHex += String(firmware_hash[i], HEX);
    }
    calculatedHashHex.toLowerCase();
    
    // Debug: Print calculated hash with explicit length check
    LOG_INFO(LOG_TAG_FOTA, "Calculated hash length: %d chars", calculatedHashHex.length());
    LOG_INFO(LOG_TAG_FOTA, "Calculated hash: %s", calculatedHashHex.c_str());
    
    // Debug: Print expected hash from manifest with explicit length check
    LOG_INFO(LOG_TAG_FOTA, "Expected hash length: %d chars", manifest.sha256_hash.length());
    LOG_INFO(LOG_TAG_FOTA, "Expected hash: %s", manifest.sha256_hash.c_str());
    
    // Compare calculated hash with manifest hash BEFORE RSA verification
    // This catches the "bad_hash" fault injection where server sends wrong hash
    bool hashMatch = (calculatedHashHex == manifest.sha256_hash);
    LOG_INFO(LOG_TAG_FOTA, "Hash comparison result: %s", hashMatch ? "MATCH" : "MISMATCH");
    
    if (!hashMatch) {
        LOG_ERROR(LOG_TAG_FOTA, "SHA256 hash mismatch!");
        LOG_ERROR(LOG_TAG_FOTA, "  Calculated: %s", calculatedHashHex.c_str());
        LOG_ERROR(LOG_TAG_FOTA, "  Expected:   %s", manifest.sha256_hash.c_str());
        LOG_ERROR(LOG_TAG_FOTA, "  → Firmware integrity check FAILED - triggering rollback");
        return false;
    }
    
    LOG_SUCCESS(LOG_TAG_FOTA, "SHA256 hash verified successfully - hashes match!");
    
    // Verify RSA signature against calculated hash
    return verifyRSASignature(firmware_hash, signature);
}

// Verify RSA signature using stored public key
bool OTAManager::verifyRSASignature(const uint8_t* hash, const uint8_t* signature)
{
    // Parse public key
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    
    int result = mbedtls_pk_parse_public_key(&pk, (const uint8_t*)SERVER_PUBLIC_KEY, strlen(SERVER_PUBLIC_KEY) + 1);
    if (result != 0) {
        LOG_ERROR(LOG_TAG_FOTA, "Failed to parse RSA public key: %d\n", result);
        mbedtls_pk_free(&pk);
        return false;
    }
    
    // Debug: Print hash being verified
    LOG_DEBUG(LOG_TAG_FOTA, "Hash for verification");
    for (int i = 0; i < 32; i++) {
    }
    
    // Debug: Print signature being verified
    LOG_DEBUG(LOG_TAG_FOTA, "Signature for verification (first 32 bytes)");
    for (int i = 0; i < 32; i++) {
    }
    
    LOG_DEBUG(LOG_TAG_FOTA, "Signature length: %u bytes", RSA_KEY_SIZE / 8);
    
    // Debug: Check key type and size
    mbedtls_pk_type_t key_type = mbedtls_pk_get_type(&pk);
    size_t key_bits = mbedtls_pk_get_bitlen(&pk);
    LOG_DEBUG(LOG_TAG_FOTA, "RSA key type: %d, bits: %zu", key_type, key_bits);
    
    // Verify RSA signature using PKCS#1 v1.5 padding with SHA-256
    result = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, signature, RSA_KEY_SIZE / 8);
    
    mbedtls_pk_free(&pk);
    
    if (result != 0) {
        LOG_ERROR(LOG_TAG_FOTA, "RSA signature verification failed: %d (0x%X)\n", result, result);
        LOG_DEBUG(LOG_TAG_FOTA, "Hash length: 32, Signature length: %d", RSA_KEY_SIZE / 8);
        
        // Common mbedtls error codes
        if (result == -0x4380) LOG_ERROR(LOG_TAG_FOTA, "-> MBEDTLS_ERR_RSA_VERIFY_FAILED");
        if (result == -0x4300) LOG_ERROR(LOG_TAG_FOTA, "-> MBEDTLS_ERR_RSA_PUBLIC_FAILED");  
        if (result == -0x4280) LOG_ERROR(LOG_TAG_FOTA, "-> MBEDTLS_ERR_RSA_PRIVATE_FAILED");
        
        return false;
    }
    
    LOG_SUCCESS(LOG_TAG_FOTA, "RSA signature verification successful");
    return true;
}

// Verify firmware and reboot to apply update
bool OTAManager::verifyAndReboot()
{
    LOG_INFO(LOG_TAG_FOTA, "Starting firmware verification...");
    
    // Report verification start
    reportProgress("verifying", 100, "Verifying firmware security...");
    
    // Verify RSA signature
    if (!verifySignature(manifest.signature)) {
        LOG_ERROR(LOG_TAG_FOTA, "Firmware signature verification failed!");
        setOTAState(OTA_ERROR);
        
        // Report verification failure
        reportProgress("verification_failed", 0, "Security verification failed - Rolling back");
        
        // Mark invalid firmware for rollback
        esp_ota_mark_app_invalid_rollback_and_reboot();
        return false;
    }
    
    LOG_SUCCESS(LOG_TAG_FOTA, "Firmware signature verified");
    
    // Report verification success
    reportProgress("verification_success", 100, "Security verification passed - Installing firmware");
    
    // Update.end() has already finalized the OTA and set the boot partition
    // The Arduino Update library handles everything internally
    
    // Store update completion flag
    setOTAState(OTA_COMPLETED);
    
    // Report installation phase
    reportProgress("installing", 100, "Installing new firmware - Device will reboot");
    
    LOG_SECTION("OTA UPDATE SUCCESSFUL");
    LOG_SUCCESS(LOG_TAG_FOTA, "Version: %s → %s", currentVersion.c_str(), manifest.version.c_str());
    LOG_INFO(LOG_TAG_FOTA, "Size: %u bytes", manifest.original_size);
    LOG_INFO(LOG_TAG_FOTA, "Rebooting to apply update...");
    
    delay(2000); // Allow serial output to complete
    
    // Reboot to apply the update
    ESP.restart();
    
    return true; // This line will never be reached
}

// Handle rollback if new firmware fails
void OTAManager::handleRollback()
{
    LOG_SECTION("HANDLING FIRMWARE ROLLBACK");
    
    // Get current running partition info
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            LOG_WARN(LOG_TAG_FOTA, "New firmware is pending verification - marking as invalid");
            
            // Mark current app as invalid and rollback
            esp_ota_mark_app_invalid_rollback_and_reboot();
        }
    }
    
    LOG_INFO(LOG_TAG_FOTA, "Rollback handling complete");
}

// Run diagnostics after OTA update
bool OTAManager::runDiagnostics()
{
    LOG_SECTION("RUNNING POST-OTA DIAGNOSTICS");
    
    bool allTestsPassed = true;
    
    // Test 1: Check partition validity
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running) {
        LOG_SUCCESS(LOG_TAG_FOTA, "Running from partition: %s", running->label);
    } else {
        LOG_ERROR(LOG_TAG_FOTA, "Could not get running partition info");
        allTestsPassed = false;
    }
    
    // Test 2: Verify heap memory
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap > 50000) { // Require at least 50KB free
        LOG_SUCCESS(LOG_TAG_FOTA, "Free heap: %u bytes", free_heap);
    } else {
        LOG_WARN(LOG_TAG_FOTA, "Low free heap: %u bytes", free_heap);
        allTestsPassed = false;
    }
    
    // Test 3: Check WiFi connectivity
    if (WiFi.status() == WL_CONNECTED) {
        LOG_SUCCESS(LOG_TAG_FOTA, "WiFi connected: %s", WiFi.localIP().toString().c_str());
    } else {
        LOG_ERROR(LOG_TAG_FOTA, "WiFi not connected");
        allTestsPassed = false;
    }
    
    // Test 4: Test basic HTTP communication
    WiFiClient client;
    client.setTimeout(10000);  // 10 second connection timeout
    
    HTTPClient http;
    http.begin(client, serverURL + "/ota/status");
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.GET();
    if (httpCode == 200) {
        LOG_SUCCESS(LOG_TAG_FOTA, "OTA server communication test passed");
    } else {
        LOG_ERROR(LOG_TAG_FOTA, "OTA server communication failed: %d", httpCode);
        allTestsPassed = false;
    }
    http.end();
    
    // Test 5: Validate version
    if (currentVersion.length() > 0 && currentVersion != "unknown") {
        LOG_SUCCESS(LOG_TAG_FOTA, "Firmware version: %s", currentVersion.c_str());
    } else {
        LOG_ERROR(LOG_TAG_FOTA, "Invalid firmware version");
        allTestsPassed = false;
    }
    
    if (allTestsPassed) {
        LOG_SUCCESS(LOG_TAG_FOTA, "ALL DIAGNOSTICS PASSED");
        
        // Mark the current app as valid (no more rollback possible)
        esp_ota_mark_app_valid_cancel_rollback();
        
        return true;
    } else {
        LOG_ERROR(LOG_TAG_FOTA, "SOME DIAGNOSTICS FAILED");
        return false;
    }
}

// Report OTA completion status to Flask server after successful reboot
bool OTAManager::reportOTACompletionStatus()
{
    LOG_SECTION("REPORTING OTA COMPLETION STATUS TO FLASK");
    
    // Check WiFi connectivity first
    if (WiFi.status() != WL_CONNECTED) {
        LOG_ERROR(LOG_TAG_FOTA, "WiFi not connected, cannot report OTA status");
        return false;
    }
    
    // Get current running partition info
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    String status = "success";
    String error_msg = "";
    
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // Still pending - need to run diagnostics first
            LOG_WARN(LOG_TAG_FOTA, "OTA image still pending verification");
            status = "pending_verify";
        } else if (ota_state == ESP_OTA_IMG_VALID) {
            // Successfully verified
            status = "success";
            LOG_SUCCESS(LOG_TAG_FOTA, "OTA image verified and marked valid");
        } else if (ota_state == ESP_OTA_IMG_INVALID) {
            // Failed and rolled back
            status = "rolled_back";
            error_msg = "Firmware validation failed, rolled back to previous version";
            LOG_ERROR(LOG_TAG_FOTA, "OTA image invalid - rolled back");
        } else {
            // Aborted
            status = "failed";
            error_msg = "OTA process aborted";
            LOG_ERROR(LOG_TAG_FOTA, "OTA image aborted");
        }
    } else {
        LOG_ERROR(LOG_TAG_FOTA, "Could not get partition state");
        return false;
    }
    
    // Prepare JSON payload
    DynamicJsonDocument doc(512);
    doc["version"] = currentVersion;
    doc["status"] = status;
    doc["timestamp"] = getCurrentTimestamp(); // Unix timestamp in seconds
    
    if (error_msg.length() > 0) {
        doc["error_msg"] = error_msg;
    }
    
    String payload;
    serializeJson(doc, payload);
    
    // Send POST request to Flask: /ota/<device_id>/complete
    String endpoint = "/ota/" + deviceID + "/complete";
    String response;
    
    bool success = httpPost(endpoint, payload, response);
    
    if (success) {
        LOG_SUCCESS(LOG_TAG_FOTA, "OTA completion status reported successfully");
        return true;
    } else {
        LOG_ERROR(LOG_TAG_FOTA, "Failed to report OTA completion status");
        return false;
    }
}

// HTTP POST helper function
bool OTAManager::httpPost(const String& endpoint, const String& payload, String& response)
{
    // Create WiFiClient with extended connection timeout
    WiFiClient client;
    client.setTimeout(30000);  // 30 second connection timeout in MILLISECONDS
    
    HTTPClient http;
    http.begin(client, serverURL + endpoint);  // Use our WiFiClient with custom timeout
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(30000); // 30 seconds HTTP timeout (in milliseconds)
    
    // Parse and pretty-print the payload if it's JSON
    StaticJsonDocument<512> payloadDoc;
    DeserializationError err = deserializeJson(payloadDoc, payload);
    
    LOG_DEBUG(LOG_TAG_FOTA, "POST %s", endpoint.c_str());
    
    if (!err) {
        // Valid JSON - print it pretty
        String prettyPayload;
        serializeJsonPretty(payloadDoc, prettyPayload);
        LOG_DEBUG(LOG_TAG_FOTA, "Payload:");
        int startPos = 0;
        int endPos = prettyPayload.indexOf('\n');
        while (endPos != -1) {
            LOG_DEBUG(LOG_TAG_FOTA, "  %s", prettyPayload.substring(startPos, endPos).c_str());
            startPos = endPos + 1;
            endPos = prettyPayload.indexOf('\n', startPos);
        }
        if (startPos < prettyPayload.length()) {
            LOG_DEBUG(LOG_TAG_FOTA, "  %s", prettyPayload.substring(startPos).c_str());
        }
    } else {
        // Not JSON or invalid - print as-is
        LOG_DEBUG(LOG_TAG_FOTA, "Payload: %s", payload.c_str());
    }
    
    int httpCode = http.POST(payload);
    
    if (httpCode > 0) {
        response = http.getString();
        
        // Parse and pretty-print the response if it's JSON
        StaticJsonDocument<512> responseDoc;
        DeserializationError respErr = deserializeJson(responseDoc, response);
        
        if (!respErr) {
            // Valid JSON - print it pretty
            String prettyResponse;
            serializeJsonPretty(responseDoc, prettyResponse);
            LOG_DEBUG(LOG_TAG_FOTA, "Response (%d):", httpCode);
            int startPos = 0;
            int endPos = prettyResponse.indexOf('\n');
            while (endPos != -1) {
                LOG_DEBUG(LOG_TAG_FOTA, "  %s", prettyResponse.substring(startPos, endPos).c_str());
                startPos = endPos + 1;
                endPos = prettyResponse.indexOf('\n', startPos);
            }
            if (startPos < prettyResponse.length()) {
                LOG_DEBUG(LOG_TAG_FOTA, "  %s", prettyResponse.substring(startPos).c_str());
            }
        } else {
            // Not JSON - print as-is
            LOG_DEBUG(LOG_TAG_FOTA, "Response (%d): %s", httpCode, response.c_str());
        }
        
        // Accept both 200 OK and 201 Created as success
        if (httpCode == 200 || httpCode == 201) {
            http.end();
            return true;
        }
    } else {
        LOG_ERROR(LOG_TAG_FOTA, "HTTP POST failed: %s", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return false;
}

// Report OTA progress to Flask server
bool OTAManager::reportProgress(const String& phase, int progressPercent, const String& message)
{
    LOG_DEBUG(LOG_TAG_FOTA, "[OTA Progress] Phase: %s, Progress: %d%%, Message: %s", 
                  phase.c_str(), progressPercent, message.c_str());
    
    // Check WiFi connectivity
    if (WiFi.status() != WL_CONNECTED) {
        LOG_WARN(LOG_TAG_FOTA, "WiFi not connected, cannot report progress");
        return false;
    }
    
    // Prepare JSON payload
    DynamicJsonDocument doc(512);
    doc["phase"] = phase;
    doc["progress"] = progressPercent;
    doc["message"] = message;
    doc["timestamp"] = getCurrentTimestamp();
    
    String payload;
    serializeJson(doc, payload);
    
    // Send POST request to Flask: /ota/<device_id>/progress
    String endpoint = "/ota/" + deviceID + "/progress";
    String response;
    
    bool success = httpPost(endpoint, payload, response);
    
    if (!success) {
        LOG_WARN(LOG_TAG_FOTA, "Failed to report OTA progress (non-critical)");
    }
    
    return success;
}

bool OTAManager::httpGet(const String& endpoint, String& response)
{
    // Create WiFiClient with extended connection timeout
    WiFiClient client;
    client.setTimeout(30000);  // 30 second connection timeout in MILLISECONDS
    
    HTTPClient http;
    http.begin(client, serverURL + endpoint);  // Use our WiFiClient with custom timeout
    http.setTimeout(30000); // 30 seconds HTTP timeout (in milliseconds)
    
    LOG_DEBUG(LOG_TAG_FOTA, "GET %s", endpoint.c_str());
    
    int httpCode = http.GET();
    
    if (httpCode > 0) {
        response = http.getString();
        
        // Parse and pretty-print the response if it's JSON
        StaticJsonDocument<1024> responseDoc;
        DeserializationError respErr = deserializeJson(responseDoc, response);
        
        if (!respErr) {
            // Valid JSON - print it pretty
            String prettyResponse;
            serializeJsonPretty(responseDoc, prettyResponse);
            LOG_DEBUG(LOG_TAG_FOTA, "Response (%d):", httpCode);
            int startPos = 0;
            int endPos = prettyResponse.indexOf('\n');
            while (endPos != -1) {
                LOG_DEBUG(LOG_TAG_FOTA, "  %s", prettyResponse.substring(startPos, endPos).c_str());
                startPos = endPos + 1;
                endPos = prettyResponse.indexOf('\n', startPos);
            }
            if (startPos < prettyResponse.length()) {
                LOG_DEBUG(LOG_TAG_FOTA, "  %s", prettyResponse.substring(startPos).c_str());
            }
        } else {
            // Not JSON - print as-is
            LOG_DEBUG(LOG_TAG_FOTA, "Response (%d): %s", httpCode, response.c_str());
        }
        
        if (httpCode == 200) {
            http.end();
            return true;
        }
    } else {
        LOG_ERROR(LOG_TAG_FOTA, "HTTP GET failed: %s", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return false;
}

// Set OTA state and update progress
void OTAManager::setOTAState(OTAState newState)
{
    state = newState;
    progress.last_activity = millis();
    
    // Print state change
    const char* stateNames[] = {
        "IDLE", "CHECKING", "DOWNLOADING", "APPLYING", 
        "COMPLETED", "ERROR", "ROLLBACK"
    };
    LOG_INFO(LOG_TAG_FOTA, "OTA State: %s", stateNames[newState]);
}

// Update progress information
void OTAManager::updateProgress(uint32_t bytes, uint16_t chunks)
{
    progress.bytes_downloaded = bytes;
    progress.chunks_received = chunks;
    progress.last_activity = millis();
    
    // Calculate progress percentage
    float percentage = 0.0;
    if (manifest.total_chunks > 0) {
        percentage = (float)chunks / manifest.total_chunks * 100.0;
    }
    
    LOG_INFO(LOG_TAG_FOTA, "Progress: %u/%u chunks (%.1f%%) - %u bytes", 
                  chunks, manifest.total_chunks, percentage, bytes);
}

// Get current OTA state
OTAState OTAManager::getState()
{
    return state;
}

// Get progress information
OTAProgress OTAManager::getProgress()
{
    return progress;
}

// Check if OTA operation timed out
bool OTAManager::isTimeout()
{
    return (millis() - progress.last_activity) > OTA_TIMEOUT_MS;
}

// Reset OTA manager to initial state
void OTAManager::reset()
{
    LOG_INFO(LOG_TAG_FOTA, "Resetting OTA Manager");
    
    // Reset state
    setOTAState(OTA_IDLE);
    
    // Clear progress
    memset(&progress, 0, sizeof(progress));
    progress.last_activity = millis();
    
    // Clear manifest
    memset(&manifest, 0, sizeof(manifest));
    
    // Clear NVS progress data
    if (nvs.begin("ota_progress", false)) {
        nvs.clear();
        nvs.end();
        LOG_INFO(LOG_TAG_FOTA, "Cleared OTA progress from NVS");
    }
    
    // Clear NVS manifest data  
    if (nvs.begin("ota_manifest", false)) {
        nvs.clear();
        nvs.end();
        LOG_INFO(LOG_TAG_FOTA, "Cleared OTA manifest from NVS");
    }
    
    // Arduino Update library handles cleanup automatically
    
    LOG_SUCCESS(LOG_TAG_FOTA, "OTA Manager reset complete");
}

// Base64 decode helper (simplified for firmware chunks)
int OTAManager::base64_decode(const char* input, uint8_t* output, size_t output_size)
{
    const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int len = strlen(input);
    int padding = 0;
    
    // Count padding characters
    if (len > 0 && input[len-1] == '=') padding++;
    if (len > 1 && input[len-2] == '=') padding++;
    
    int decoded_len = (len * 3) / 4 - padding;
    
    if (decoded_len > output_size) {
        return -1; // Output buffer too small
    }
    
    int i, j = 0;
    uint32_t sextet_a, sextet_b, sextet_c, sextet_d;
    uint32_t triple;
    
    for (i = 0; i < len; i += 4) {
        // Find character positions in base64 table
        sextet_a = strchr(base64_chars, input[i]) - base64_chars;
        sextet_b = strchr(base64_chars, input[i+1]) - base64_chars;
        sextet_c = (input[i+2] == '=') ? 0 : strchr(base64_chars, input[i+2]) - base64_chars;
        sextet_d = (input[i+3] == '=') ? 0 : strchr(base64_chars, input[i+3]) - base64_chars;
        
        triple = (sextet_a << 18) + (sextet_b << 12) + (sextet_c << 6) + sextet_d;
        
        if (j < decoded_len) output[j++] = (triple >> 16) & 0xFF;
        if (j < decoded_len) output[j++] = (triple >> 8) & 0xFF;
        if (j < decoded_len) output[j++] = triple & 0xFF;
    }
    
    return decoded_len;
}

// Set error message and state
void OTAManager::setError(const String& message)
{
    progress.error_message = message;
    progress.state = OTA_ERROR;
    state = OTA_ERROR;
    LOG_ERROR(LOG_TAG_FOTA, "OTA Error: %s", message.c_str());
}

// Save OTA progress to NVS for resume capability
void OTAManager::saveProgress()
{
    if (!nvs.begin("ota_progress", false)) {
        LOG_ERROR(LOG_TAG_FOTA, "Failed to initialize NVS for progress saving");
        return;
    }
    
    nvs.putUInt("chunks_recv", progress.chunks_received);
    nvs.putUInt("total_chunks", progress.total_chunks);
    nvs.putUInt("bytes_down", progress.bytes_downloaded);
    nvs.putUInt("percentage", progress.percentage);
    nvs.putUInt("state", (uint32_t)progress.state);
    nvs.putString("version", manifest.version);
    nvs.putUInt("firmware_size", manifest.firmware_size);
    
    nvs.end();
    
    LOG_DEBUG(LOG_TAG_FOTA, "Progress saved: %u/%u chunks (%.1f%%)", 
                  progress.chunks_received, progress.total_chunks, 
                  (float)progress.percentage);
}

// Load OTA progress from NVS for resume capability
void OTAManager::loadProgress()
{
    if (!nvs.begin("ota_progress", true)) { // Read-only mode
        LOG_INFO(LOG_TAG_FOTA, "No previous OTA progress found");
        return;
    }
    
    progress.chunks_received = nvs.getUInt("chunks_recv", 0);
    progress.total_chunks = nvs.getUInt("total_chunks", 0);
    progress.bytes_downloaded = nvs.getUInt("bytes_down", 0);
    progress.percentage = nvs.getUInt("percentage", 0);
    progress.state = (OTAState)nvs.getUInt("state", OTA_IDLE);
    manifest.version = nvs.getString("version", "");
    manifest.firmware_size = nvs.getUInt("firmware_size", 0);
    
    nvs.end();
    
    if (progress.chunks_received > 0) {
        LOG_INFO(LOG_TAG_FOTA, "Loaded previous progress: %u/%u chunks (%.1f%%)", 
                      progress.chunks_received, progress.total_chunks, 
                      (float)progress.percentage);
        LOG_INFO(LOG_TAG_FOTA, "Previous OTA version: %s", manifest.version.c_str());
    }
}// OTA Fault Testing Implementation
// Add to the end of OTAManager.cpp

/**
 * @brief Enable OTA fault testing mode
 * @param faultType Type of fault to simulate
 */
void OTAManager::enableTestMode(OTAFaultType faultType)
{
    testModeEnabled = true;
    testFaultType = faultType;
    
    LOG_SECTION("OTA FAULT TESTING MODE ENABLED");
    LOG_WARN(LOG_TAG_FOTA, "Fault Type:");
    switch (faultType) {
        case OTA_FAULT_CORRUPT_CHUNK:
            LOG_WARN(LOG_TAG_FOTA, "CORRUPT_CHUNK - Will corrupt chunk data");
            break;
        case OTA_FAULT_BAD_HMAC:
            LOG_WARN(LOG_TAG_FOTA, "BAD_HMAC - Will fail HMAC verification");
            break;
        case OTA_FAULT_BAD_HASH:
            LOG_WARN(LOG_TAG_FOTA, "BAD_HASH - Will fail hash verification");
            break;
        case OTA_FAULT_NETWORK_TIMEOUT:
            LOG_WARN(LOG_TAG_FOTA, "NETWORK_TIMEOUT - Will simulate network timeout");
            break;
        case OTA_FAULT_INCOMPLETE_DOWNLOAD:
            LOG_WARN(LOG_TAG_FOTA, "INCOMPLETE_DOWNLOAD - Will simulate incomplete download");
            break;
        default:
            LOG_INFO(LOG_TAG_FOTA, "NONE");
            break;
    }
    LOG_INFO(LOG_TAG_FOTA, "======================================");
}

/**
 * @brief Get human-readable state string
 * @return String representing current OTA state
 */
String OTAManager::getStateString()
{
    switch (progress.state) {
        case OTA_IDLE: return "IDLE";
        case OTA_CHECKING: return "CHECKING";
        case OTA_DOWNLOADING: return "DOWNLOADING";
        case OTA_VERIFYING: return "VERIFYING";
        case OTA_APPLYING: return "APPLYING";
        case OTA_COMPLETED: return "COMPLETED";
        case OTA_ERROR: return "ERROR";
        case OTA_ROLLBACK: return "ROLLBACK";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Check if OTA operation is in progress
 * @return true if OTA is in progress
 */
bool OTAManager::isOTAInProgress()
{
    return (progress.state == OTA_CHECKING ||
            progress.state == OTA_DOWNLOADING ||
            progress.state == OTA_VERIFYING ||
            progress.state == OTA_APPLYING);
}

/**
 * @brief Check if OTA download can be resumed
 * @return true if resume is possible
 */
bool OTAManager::canResume()
{
    // Can resume if:
    // 1. Progress exists (chunks_received > 0)
    // 2. Not in error state
    // 3. Total chunks is known
    // 4. Current state is DOWNLOADING or IDLE
    return (progress.chunks_received > 0 &&
            progress.total_chunks > 0 &&
            progress.chunks_received < progress.total_chunks &&
            (progress.state == OTA_DOWNLOADING || progress.state == OTA_IDLE));
}

/**
 * @brief Clear saved OTA progress
 */
void OTAManager::clearProgress()
{
    LOG_INFO(LOG_TAG_FOTA, "Clearing OTA progress...");
    
    // Reset progress structure
    progress.chunks_received = 0;
    progress.total_chunks = 0;
    progress.bytes_downloaded = 0;
    progress.percentage = 0;
    progress.state = OTA_IDLE;
    progress.error_message = "";
    progress.last_activity = 0;
    
    // Clear from NVS storage
    nvs.clear();
    
    LOG_SUCCESS(LOG_TAG_FOTA, "OTA progress cleared");
}

/**
 * @brief Set OTA server URL
 * @param url New server URL
 */
void OTAManager::setServerURL(const String& url)
{
    LOG_INFO(LOG_TAG_FOTA, "Updating OTA server URL: %s -> %s", serverURL.c_str(), url.c_str());
    serverURL = url;
}

/**
 * @brief Set update check interval
 * @param intervalMs Interval in milliseconds
 */
void OTAManager::setCheckInterval(unsigned long intervalMs)
{
    LOG_INFO(LOG_TAG_FOTA, "Updating OTA check interval: %lu -> %lu ms", checkInterval, intervalMs);
    checkInterval = intervalMs;
}

/**
 * @brief Disable OTA fault testing mode
 */
void OTAManager::disableTestMode()
{
    if (testModeEnabled) {
        LOG_INFO(LOG_TAG_FOTA, "=== OTA FAULT TESTING MODE DISABLED ===");
    }
    testModeEnabled = false;
    testFaultType = OTA_FAULT_NONE;
}

/**
 * @brief Get OTA statistics
 * @param successCount Number of successful OTA updates
 * @param failureCount Number of failed OTA updates
 * @param rollbackCount Number of rollbacks
 */
void OTAManager::getOTAStatistics(uint32_t& successCount, uint32_t& failureCount, uint32_t& rollbackCount)
{
    successCount = otaSuccessCount;
    failureCount = otaFailureCount;
    rollbackCount = otaRollbackCount;
}

/**
 * @brief Check if fault should be injected
 * @return true if fault should be injected
 */
bool OTAManager::shouldInjectFault()
{
    return testModeEnabled && testFaultType != OTA_FAULT_NONE;
}

/**
 * @brief Simulate a fault for testing
 * @param faultType Type of fault to simulate
 * @return false to indicate fault was simulated
 */
bool OTAManager::simulateFault(OTAFaultType faultType)
{
    if (!testModeEnabled || testFaultType != faultType) {
        return true; // No fault, proceed normally
    }
    
    LOG_WARN(LOG_TAG_FOTA, "FAULT INJECTED:");
    
    switch (faultType) {
        case OTA_FAULT_CORRUPT_CHUNK:
            LOG_WARN(LOG_TAG_FOTA, "Corrupting chunk data");
            // This will be handled in downloadChunk()
            return false;
            
        case OTA_FAULT_BAD_HMAC:
            LOG_WARN(LOG_TAG_FOTA, "Failing HMAC verification");
            setError("HMAC verification failed (TEST MODE)");
            otaFailureCount++;
            return false;
            
        case OTA_FAULT_BAD_HASH:
            LOG_WARN(LOG_TAG_FOTA, "Failing hash verification");
            setError("Hash verification failed (TEST MODE)");
            otaFailureCount++;
            return false;
            
        case OTA_FAULT_NETWORK_TIMEOUT:
            LOG_WARN(LOG_TAG_FOTA, "Simulating network timeout");
            delay(OTA_TIMEOUT_MS + 1000); // Force timeout
            setError("Network timeout (TEST MODE)");
            otaFailureCount++;
            return false;
            
        case OTA_FAULT_INCOMPLETE_DOWNLOAD:
            LOG_WARN(LOG_TAG_FOTA, "Simulating incomplete download");
            setError("Incomplete download (TEST MODE)");
            otaFailureCount++;
            return false;
            
        default:
            return true; // No fault
    }
}

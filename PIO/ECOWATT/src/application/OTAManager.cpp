#include "application/OTAManager.h"

// Constructor
OTAManager::OTAManager(const String& serverURL, const String& deviceID, const String& currentVersion)
    : serverURL(serverURL), deviceID(deviceID), currentVersion(currentVersion), checkInterval(3600000), state(OTA_IDLE), // 1 hour default
      testModeEnabled(false), testFaultType(OTA_FAULT_NONE), otaSuccessCount(0), otaFailureCount(0), otaRollbackCount(0)
{
    Serial.println("=== OTA Manager Initialization ===");
    
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
        Serial.println("ERROR: Failed to allocate decryption buffer!");
        setError("Memory allocation failed");
        return;
    }
    
    // Initialize NVS
    bool nvs_ok = nvs.begin("ota", false); // read-write mode
    if (!nvs_ok) {
        Serial.println("ERROR: Failed to initialize NVS storage!");
        setError("NVS initialization failed");
        return;
    }
    
    // Load any existing progress (for resume capability)
    loadProgress();
    
    Serial.printf("Device ID: %s\n", deviceID.c_str());
    Serial.printf("Current Version: %s\n", currentVersion.c_str());
    Serial.printf("Server URL: %s\n", serverURL.c_str());
    Serial.printf("Decryption buffer: %d bytes allocated\n", DECRYPT_BUFFER_SIZE);
    Serial.println("OTA Manager initialized successfully");
    Serial.println("=====================================");
}

// Destructor
OTAManager::~OTAManager()
{
    Serial.println("OTA Manager cleanup...");
    
    // Free mbedtls AES context
    mbedtls_aes_free(&aes_ctx);
    
    // Free decryption buffer
    if (decryptBuffer != NULL) {
        free(decryptBuffer);
        decryptBuffer = NULL;
    }
    
    // Close NVS
    nvs.end();
    
    Serial.println("OTA Manager cleanup complete");
}

// Check for firmware updates
bool OTAManager::checkForUpdate()
{
    Serial.println("\n=== CHECKING FOR FIRMWARE UPDATES ===");
    progress.state = OTA_CHECKING;
    
    // Check WiFi connectivity
    if (WiFi.status() != WL_CONNECTED) {
        setError("WiFi not connected");
        Serial.println("ERROR: WiFi connection required for OTA check");
        progress.state = OTA_IDLE;
        return false;
    }
    
    Serial.printf("Checking updates for device: %s\n", deviceID.c_str());
    Serial.printf("Current version: %s\n", currentVersion.c_str());
    
    // Create JSON request
    DynamicJsonDocument requestDoc(256);
    requestDoc["device_id"] = deviceID;
    requestDoc["current_version"] = currentVersion;
    
    String payload;
    serializeJson(requestDoc, payload);
    
    // Send HTTP request
    String response;
    if (!httpPost("/ota/check", payload, response)) {
        setError("Failed to communicate with OTA server");
        progress.state = OTA_IDLE;
        return false;
    }
    
    // Parse response
    DynamicJsonDocument responseDoc(2048);
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (error) {
        setError("Invalid JSON response from server");
        Serial.printf("JSON parse error: %s\n", error.c_str());
        progress.state = OTA_IDLE;
        return false;
    }
    
    // Check if update is available
    bool updateAvailable = responseDoc["update_available"];
    if (!updateAvailable) {
        Serial.println("No firmware updates available");
        Serial.printf("Device is already running the latest version: %s\n", currentVersion.c_str());
        progress.state = OTA_IDLE;
        return false;
    }
    
    // Parse manifest from response
    manifest.version = responseDoc["version"].as<String>();
    manifest.original_size = responseDoc["original_size"];
    manifest.encrypted_size = responseDoc["encrypted_size"];
    manifest.firmware_size = manifest.original_size; // Use original size for firmware validation
    manifest.sha256_hash = responseDoc["sha256_hash"].as<String>();
    manifest.signature = responseDoc["signature"].as<String>();
    manifest.iv = responseDoc["iv"].as<String>();
    manifest.chunk_size = responseDoc["chunk_size"];
    manifest.total_chunks = responseDoc["total_chunks"];
    
    // Update progress
    progress.total_chunks = manifest.total_chunks;
    
    // Print update information
    Serial.println("*** FIRMWARE UPDATE AVAILABLE ***");
    Serial.printf("Current version: %s\n", currentVersion.c_str());
    Serial.printf("New version: %s\n", manifest.version.c_str());
    Serial.printf("Firmware size: %u bytes (encrypted: %u bytes)\n", 
                  manifest.original_size, manifest.encrypted_size);
    Serial.printf("Total chunks: %u (size: %u bytes each)\n", 
                  manifest.total_chunks, manifest.chunk_size);
    Serial.printf("SHA-256 hash: %s\n", manifest.sha256_hash.c_str());
    
    // Decode and store IV for AES decryption
    uint8_t ivBuffer[16];
    int ivLen = base64_decode(manifest.iv.c_str(), ivBuffer, sizeof(ivBuffer));
    if (ivLen != 16) {
        setError("Invalid IV length from server");
        Serial.printf("ERROR: IV length is %d bytes (expected 16)\n", ivLen);
        progress.state = OTA_IDLE;
        return false;
    }
    
    // Copy IV to AES context
    memcpy(aes_iv, ivBuffer, 16);
    Serial.println("IV decoded and stored successfully");
    
    // Save manifest to NVS for resume capability
    nvs.putString("version", manifest.version);
    nvs.putUShort("total_chunks", manifest.total_chunks);
    nvs.putString("hash", manifest.sha256_hash);
    nvs.putString("signature", manifest.signature);
    nvs.putString("iv", manifest.iv);
    nvs.putUInt("enc_size", manifest.encrypted_size);
    
    Serial.println("Manifest saved to NVS");
    Serial.println("===================================");
    
    return true;
}

// Download and apply firmware
bool OTAManager::downloadAndApplyFirmware()
{
    Serial.println("\n=== STARTING FIRMWARE DOWNLOAD ===");
    
    // Reset progress for fresh start (important for retry scenarios)
    progress.chunks_received = 0;
    progress.bytes_downloaded = 0;
    progress.percentage = 0;
    progress.state = OTA_DOWNLOADING;
    
    // Clear any stored NVS progress to ensure clean start
    if (nvs.begin("ota_progress", false)) {
        nvs.clear();
        nvs.end();
        Serial.println("Cleared previous OTA progress from NVS");
    }
    
    // Ensure Update library is clean for new session
    if (Update.isRunning()) {
        Serial.println("Previous OTA session still active - cleaning up...");
        Update.abort();
        Serial.println("Previous OTA session aborted");
    }
    
    // Get OTA partition information
    const esp_partition_t* ota_partition = esp_ota_get_next_update_partition(NULL);
    if (ota_partition == NULL) {
        setError("No OTA partition available");
        Serial.println("ERROR: Could not find OTA partition");
        return false;
    }
    
    Serial.printf("OTA partition: %s\n", ota_partition->label);
    Serial.printf("Partition size: %u bytes\n", ota_partition->size);
    
    // Verify firmware fits in partition
    if (manifest.encrypted_size > ota_partition->size) {
        setError("Firmware too large for OTA partition");
        Serial.printf("ERROR: Firmware (%u bytes) exceeds partition size (%u bytes)\n",
                      manifest.encrypted_size, ota_partition->size);
        return false;
    }
    
    // Initialize OTA with original (decrypted) firmware size
    if (!Update.begin(manifest.original_size)) {
        setError("OTA initialization failed: " + String(Update.errorString()));
        Serial.printf("ERROR: Update.begin() failed: %s\n", Update.errorString());
        return false;
    }
    
    Serial.println("OTA partition initialized successfully");
    
    // Configure AES decryption key
    int aes_result = mbedtls_aes_setkey_dec(&aes_ctx, AES_FIRMWARE_KEY, 256);
    if (aes_result != 0) {
        setError("AES key configuration failed");
        Serial.printf("ERROR: AES key setup failed: %d\n", aes_result);
        return false;
    }
    
    // Reset IV to original value for streaming decryption
    // The IV from manifest should be used as the starting IV
    uint8_t ivBuffer[16];
    int ivLen = base64_decode(manifest.iv.c_str(), ivBuffer, sizeof(ivBuffer));
    if (ivLen == 16) {
        memcpy(aes_iv, ivBuffer, 16);
        Serial.println("AES IV reset for streaming decryption");
    }
    
    Serial.println("AES decryption key configured");
    
    // Always start from chunk 0 (disable resume to debug decryption)
    uint16_t startChunk = 0;
    progress.chunks_received = 0;
    progress.bytes_downloaded = 0;
    unsigned long startTime = millis();
    unsigned long lastProgressTime = startTime;
    
    Serial.printf("Starting download from chunk %u to %u (resume disabled)\n", startChunk, manifest.total_chunks - 1);
    
    // Download chunks
    for (uint16_t chunk = startChunk; chunk < manifest.total_chunks; chunk++) {
        // Download and process chunk
        if (!downloadChunk(chunk)) {
            setError("Chunk download failed at chunk " + String(chunk));
            Serial.printf("ERROR: Failed to download chunk %u\n", chunk);
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
            
            Serial.printf("Progress: [%3u%%] %4u/%4u chunks | %6u bytes | %.1f KB/s\n",
                         progress.percentage, progress.chunks_received, 
                         manifest.total_chunks, progress.bytes_downloaded, speed);
            
            // Progress bar visualization
            int barWidth = 30;
            int filled = (progress.percentage * barWidth) / 100;
            Serial.print("          [");
            for (int i = 0; i < barWidth; i++) {
                if (i < filled) Serial.print("█");
                else Serial.print("·");
            }
            Serial.printf("] %u%%\n", progress.percentage);
            
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
    
    Serial.println("\n*** DOWNLOAD COMPLETED ***");
    Serial.printf("Total time: %lu seconds\n", totalTime);
    Serial.printf("Average speed: %.1f KB/s\n", avgSpeed);
    Serial.printf("Total bytes written: %u\n", progress.bytes_downloaded);
    Serial.printf("Expected firmware size: %u bytes\n", manifest.original_size);
    Serial.printf("Update progress: %u bytes\n", Update.progress());
    Serial.printf("Update size: %u bytes\n", Update.size());
    Serial.printf("Update remaining: %d bytes\n", Update.remaining());
    
    // Verify we wrote the expected amount
    if (progress.bytes_downloaded != manifest.original_size) {
        Serial.printf("WARNING: Size mismatch - wrote %u, expected %u\n", 
                     progress.bytes_downloaded, manifest.original_size);
    }
    
    // Finalize OTA
    Serial.println("Calling Update.end()...");
    if (!Update.end()) {
        setError("OTA finalization failed: " + String(Update.errorString()));
        Serial.printf("ERROR: Update.end() failed: %s\n", Update.errorString());
        Serial.printf("Update hasError: %s\n", Update.hasError() ? "YES" : "NO");
        Serial.printf("Update getError: %u\n", Update.getError());
        return false;
    }
    
    Serial.println("Firmware written to OTA partition successfully");
    progress.state = OTA_VERIFYING;
    
    return true;
}

// Download a single firmware chunk
bool OTAManager::downloadChunk(uint16_t chunkNumber)
{
    // Create JSON request for chunk
    DynamicJsonDocument requestDoc(256);
    requestDoc["device_id"] = deviceID;
    requestDoc["version"] = manifest.version;
    requestDoc["chunk_number"] = chunkNumber;
    
    String payload;
    serializeJson(requestDoc, payload);
    
    // Request chunk from server
    String response;
    if (!httpPost("/ota/chunk", payload, response)) {
        Serial.printf("HTTP request failed for chunk %u\n", chunkNumber);
        return false;
    }
    
    // Parse JSON response
    DynamicJsonDocument responseDoc(3072); // Larger buffer for chunk data
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (error) {
        Serial.printf("JSON parse error for chunk %u: %s\n", chunkNumber, error.c_str());
        return false;
    }
    
    // Extract chunk data
    JsonObject chunkObj = responseDoc["chunk"];
    String chunkDataB64 = chunkObj["data"];
    String chunkHMAC = chunkObj["hmac"];
    size_t chunkSize = chunkObj["size"];
    
    // Decode base64 chunk data
    uint8_t encryptedChunk[OTA_CHUNK_SIZE + 16]; // Extra space for padding
    int encryptedLen = base64_decode(chunkDataB64.c_str(), encryptedChunk, sizeof(encryptedChunk));
    
    if (encryptedLen != (int)chunkSize) {
        Serial.printf("Chunk %u size mismatch: expected %u, got %d\n", 
                     chunkNumber, chunkSize, encryptedLen);
        return false;
    }
    
    // Verify chunk HMAC
    if (!verifyChunkHMAC(encryptedChunk, encryptedLen, 
                        chunkNumber, chunkHMAC)) {
        Serial.printf("HMAC verification failed for chunk %u\n", chunkNumber);
        return false;
    }
    
    // Decrypt chunk
    Serial.printf("About to decrypt chunk %u (%d bytes)\n", chunkNumber, encryptedLen);
    size_t decryptedLen;
    if (!decryptChunk(encryptedChunk, encryptedLen, 
                     decryptBuffer, &decryptedLen, chunkNumber)) {
        Serial.printf("ERROR: Decryption failed for chunk %u\n", chunkNumber);
        return false;
    }
    Serial.printf("Chunk %u decrypted successfully (%u bytes)\n", chunkNumber, decryptedLen);
    
    // Debug: Print first few bytes of decrypted data for verification
    if (chunkNumber < 3) {
        Serial.printf("Chunk %u decrypted (%u bytes): ", chunkNumber, decryptedLen);
        for (int i = 0; i < 16 && i < decryptedLen; i++) {
            Serial.printf("%02X ", decryptBuffer[i]);
        }
        Serial.println();
    }
    
    // Write decrypted data to OTA partition
    size_t written = Update.write(decryptBuffer, decryptedLen);
    if (written != decryptedLen) {
        Serial.printf("Write error for chunk %u: expected %u, wrote %u\n", 
                     chunkNumber, decryptedLen, written);
        Serial.printf("Update error: %s\n", Update.errorString());
        return false;
    }
    
    // Update progress
    progress.bytes_downloaded += written;
    
    return true;
}

// Decrypt firmware chunk using AES-256-CBC (streaming mode)
bool OTAManager::decryptChunk(const uint8_t* encrypted, size_t encLen, uint8_t* decrypted, size_t* decLen, uint16_t chunkNumber)
{
    Serial.printf("decryptChunk: Entering with %u bytes for chunk %u\n", encLen, chunkNumber);
    
    // Check buffer size
    if (encLen > DECRYPT_BUFFER_SIZE) {
        Serial.printf("ERROR: Encrypted chunk too large: %u bytes (max %u)\n", encLen, DECRYPT_BUFFER_SIZE);
        return false;
    }
    
    // For streaming AES-CBC, we need to maintain IV state across chunks
    // The IV is automatically updated by mbedtls during decryption
    
    Serial.printf("decryptChunk: About to call mbedtls_aes_crypt_cbc\n");
    
    // Decrypt using AES-256-CBC in streaming mode
    int result = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, encLen,
                                      aes_iv, encrypted, decrypted);
    
    Serial.printf("decryptChunk: mbedtls_aes_crypt_cbc returned %d\n", result);
    
    if (result != 0) {
        Serial.printf("ERROR: AES decryption failed: %d\n", result);
        return false;
    }
    
    *decLen = encLen;
    
    // Remove PKCS7 padding ONLY on the very last chunk
    bool isLastChunk = (chunkNumber == (manifest.total_chunks - 1));
    Serial.printf("decryptChunk: chunk=%u, total=%u, isLast=%s\n", 
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
                Serial.printf("Removed PKCS7 padding: %u bytes\n", paddingLen);
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
        Serial.printf("ERROR: HMAC setup failed: %d\n", result);
        mbedtls_md_free(&ctx);
        return false;
    }
    
    // Start HMAC with PSK key
    result = mbedtls_md_hmac_starts(&ctx, (const unsigned char*)HMAC_PSK, strlen(HMAC_PSK));
    if (result != 0) {
        Serial.printf("ERROR: HMAC start failed: %d\n", result);
        mbedtls_md_free(&ctx);
        return false;
    }
    
    // Update with chunk data
    result = mbedtls_md_hmac_update(&ctx, chunkData, len);
    if (result != 0) {
        Serial.printf("ERROR: HMAC update (data) failed: %d\n", result);
        mbedtls_md_free(&ctx);
        return false;
    }
    
    // Update with chunk number (converted to string)
    String chunkNumStr = String(chunkNum);
    result = mbedtls_md_hmac_update(&ctx, (const unsigned char*)chunkNumStr.c_str(), chunkNumStr.length());
    if (result != 0) {
        Serial.printf("ERROR: HMAC update (chunk num) failed: %d\n", result);
        mbedtls_md_free(&ctx);
        return false;
    }
    
    // Finalize HMAC
    uint8_t hmac_result[32];
    result = mbedtls_md_hmac_finish(&ctx, hmac_result);
    if (result != 0) {
        Serial.printf("ERROR: HMAC finish failed: %d\n", result);
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
        Serial.printf("HMAC mismatch for chunk %u\n", chunkNum);
        Serial.printf("Expected: %s\n", expectedHMAC.c_str());
        Serial.printf("Calculated: %s\n", calculatedHMAC.c_str());
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
        Serial.printf("ERROR: Invalid signature length: %d (expected %u)\n", sigLen, sizeof(signature));
        return false;
    }
    
    // Debug: Print first 16 bytes of signature
    Serial.print("Signature (first 16 bytes): ");
    for (int i = 0; i < 16; i++) {
        Serial.printf("%02x", signature[i]);
    }
    Serial.println();
    
    // Calculate SHA-256 hash of the entire decrypted firmware
    uint8_t firmware_hash[32];
    mbedtls_sha256_context sha_ctx;
    
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0); // 0 for SHA-256 (not SHA-224)
    
    // Read firmware from OTA partition and hash it
    esp_partition_t* ota_partition = (esp_partition_t*)esp_ota_get_next_update_partition(NULL);
    if (!ota_partition) {
        Serial.println("ERROR: Could not get OTA partition for verification");
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
            Serial.printf("ERROR: Failed to read OTA partition at offset %u\n", offset);
            mbedtls_sha256_free(&sha_ctx);
            return false;
        }
        
        mbedtls_sha256_update(&sha_ctx, read_buffer, read_size);
        
        offset += read_size;
        bytes_to_hash -= read_size;
    }
    
    mbedtls_sha256_finish(&sha_ctx, firmware_hash);
    mbedtls_sha256_free(&sha_ctx);
    
    // Debug: Print calculated hash
    Serial.print("ESP32 calculated hash: ");
    for (int i = 0; i < 32; i++) {
        Serial.printf("%02x", firmware_hash[i]);
    }
    Serial.println();
    
    // Debug: Print expected hash from manifest
    Serial.printf("Server expected hash: %s\n", manifest.sha256_hash.c_str());
    
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
        Serial.printf("ERROR: Failed to parse RSA public key: %d\n", result);
        mbedtls_pk_free(&pk);
        return false;
    }
    
    // Debug: Print hash being verified
    Serial.print("Hash for verification: ");
    for (int i = 0; i < 32; i++) {
        Serial.printf("%02x", hash[i]);
    }
    Serial.println();
    
    // Debug: Print signature being verified
    Serial.print("Signature for verification (first 32 bytes): ");
    for (int i = 0; i < 32; i++) {
        Serial.printf("%02x", signature[i]);
    }
    Serial.println();
    
    Serial.printf("Signature length: %u bytes\n", RSA_KEY_SIZE / 8);
    
    // Debug: Check key type and size
    mbedtls_pk_type_t key_type = mbedtls_pk_get_type(&pk);
    size_t key_bits = mbedtls_pk_get_bitlen(&pk);
    Serial.printf("RSA key type: %d, bits: %zu\n", key_type, key_bits);
    
    // Verify RSA signature using PKCS#1 v1.5 padding with SHA-256
    result = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, signature, RSA_KEY_SIZE / 8);
    
    mbedtls_pk_free(&pk);
    
    if (result != 0) {
        Serial.printf("ERROR: RSA signature verification failed: %d (0x%X)\n", result, result);
        Serial.printf("Hash length: 32, Signature length: %d\n", RSA_KEY_SIZE / 8);
        
        // Common mbedtls error codes
        if (result == -0x4380) Serial.println("-> MBEDTLS_ERR_RSA_VERIFY_FAILED");
        if (result == -0x4300) Serial.println("-> MBEDTLS_ERR_RSA_PUBLIC_FAILED");  
        if (result == -0x4280) Serial.println("-> MBEDTLS_ERR_RSA_PRIVATE_FAILED");
        
        return false;
    }
    
    Serial.println("✓ RSA signature verification successful");
    return true;
}

// Verify firmware and reboot to apply update
bool OTAManager::verifyAndReboot()
{
    Serial.println("Starting firmware verification...");
    
    // Verify RSA signature
    if (!verifySignature(manifest.signature)) {
        Serial.println("ERROR: Firmware signature verification failed!");
        setOTAState(OTA_ERROR);
        
        // Mark invalid firmware for rollback
        esp_ota_mark_app_invalid_rollback_and_reboot();
        return false;
    }
    
    Serial.println("✓ Firmware signature verified");
    
    // Update.end() has already finalized the OTA and set the boot partition
    // The Arduino Update library handles everything internally
    
    // Store update completion flag
    setOTAState(OTA_COMPLETED);
    
    Serial.println("=== OTA UPDATE SUCCESSFUL ===");
    Serial.printf("Version: %s → %s\n", currentVersion.c_str(), manifest.version.c_str());
    Serial.printf("Size: %u bytes\n", manifest.original_size);
    Serial.println("Rebooting to apply update...");
    
    delay(2000); // Allow serial output to complete
    
    // Reboot to apply the update
    ESP.restart();
    
    return true; // This line will never be reached
}

// Handle rollback if new firmware fails
void OTAManager::handleRollback()
{
    Serial.println("=== HANDLING FIRMWARE ROLLBACK ===");
    
    // Get current running partition info
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            Serial.println("New firmware is pending verification - marking as invalid");
            
            // Mark current app as invalid and rollback
            esp_ota_mark_app_invalid_rollback_and_reboot();
        }
    }
    
    Serial.println("Rollback handling complete");
}

// Run diagnostics after OTA update
bool OTAManager::runDiagnostics()
{
    Serial.println("=== RUNNING POST-OTA DIAGNOSTICS ===");
    
    bool allTestsPassed = true;
    
    // Test 1: Check partition validity
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running) {
        Serial.printf("✓ Running from partition: %s\n", running->label);
    } else {
        Serial.println("✗ Could not get running partition info");
        allTestsPassed = false;
    }
    
    // Test 2: Verify heap memory
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap > 50000) { // Require at least 50KB free
        Serial.printf("✓ Free heap: %u bytes\n", free_heap);
    } else {
        Serial.printf("✗ Low free heap: %u bytes\n", free_heap);
        allTestsPassed = false;
    }
    
    // Test 3: Check WiFi connectivity
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("✓ WiFi connected: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("✗ WiFi not connected");
        allTestsPassed = false;
    }
    
    // Test 4: Test basic HTTP communication
    HTTPClient http;
    http.begin(serverURL + "/ota/status");
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.GET();
    if (httpCode == 200) {
        Serial.println("✓ OTA server communication test passed");
    } else {
        Serial.printf("✗ OTA server communication failed: %d\n", httpCode);
        allTestsPassed = false;
    }
    http.end();
    
    // Test 5: Validate version
    if (currentVersion.length() > 0 && currentVersion != "unknown") {
        Serial.printf("✓ Firmware version: %s\n", currentVersion.c_str());
    } else {
        Serial.println("✗ Invalid firmware version");
        allTestsPassed = false;
    }
    
    if (allTestsPassed) {
        Serial.println("✓ ALL DIAGNOSTICS PASSED");
        
        // Mark the current app as valid (no more rollback possible)
        esp_ota_mark_app_valid_cancel_rollback();
        
        return true;
    } else {
        Serial.println("✗ SOME DIAGNOSTICS FAILED");
        return false;
    }
}

// HTTP POST helper function
bool OTAManager::httpPost(const String& endpoint, const String& payload, String& response)
{
    HTTPClient http;
    http.begin(serverURL + endpoint);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(30000); // 30 seconds
    
    Serial.printf("POST %s\n", endpoint.c_str());
    Serial.printf("Payload: %s\n", payload.c_str());
    
    int httpCode = http.POST(payload);
    
    if (httpCode > 0) {
        response = http.getString();
        Serial.printf("Response (%d): %s\n", httpCode, response.c_str());
        
        if (httpCode == 200) {
            http.end();
            return true;
        }
    } else {
        Serial.printf("HTTP POST failed: %s\n", http.errorToString(httpCode).c_str());
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
    Serial.printf("OTA State: %s\n", stateNames[newState]);
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
    
    Serial.printf("Progress: %u/%u chunks (%.1f%%) - %u bytes\n", 
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
    Serial.println("Resetting OTA Manager");
    
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
        Serial.println("Cleared OTA progress from NVS");
    }
    
    // Clear NVS manifest data  
    if (nvs.begin("ota_manifest", false)) {
        nvs.clear();
        nvs.end();
        Serial.println("Cleared OTA manifest from NVS");
    }
    
    // Arduino Update library handles cleanup automatically
    
    Serial.println("OTA Manager reset complete");
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
    Serial.printf("OTA Error: %s\n", message.c_str());
}

// Save OTA progress to NVS for resume capability
void OTAManager::saveProgress()
{
    if (!nvs.begin("ota_progress", false)) {
        Serial.println("Failed to initialize NVS for progress saving");
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
    
    Serial.printf("Progress saved: %u/%u chunks (%.1f%%)\n", 
                  progress.chunks_received, progress.total_chunks, 
                  (float)progress.percentage);
}

// Load OTA progress from NVS for resume capability
void OTAManager::loadProgress()
{
    if (!nvs.begin("ota_progress", true)) { // Read-only mode
        Serial.println("No previous OTA progress found");
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
        Serial.printf("Loaded previous progress: %u/%u chunks (%.1f%%)\n", 
                      progress.chunks_received, progress.total_chunks, 
                      (float)progress.percentage);
        Serial.printf("Previous OTA version: %s\n", manifest.version.c_str());
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
    
    Serial.println("\n=== OTA FAULT TESTING MODE ENABLED ===");
    Serial.printf("Fault Type: ");
    switch (faultType) {
        case OTA_FAULT_CORRUPT_CHUNK:
            Serial.println("CORRUPT_CHUNK - Will corrupt chunk data");
            break;
        case OTA_FAULT_BAD_HMAC:
            Serial.println("BAD_HMAC - Will fail HMAC verification");
            break;
        case OTA_FAULT_BAD_HASH:
            Serial.println("BAD_HASH - Will fail hash verification");
            break;
        case OTA_FAULT_NETWORK_TIMEOUT:
            Serial.println("NETWORK_TIMEOUT - Will simulate network timeout");
            break;
        case OTA_FAULT_INCOMPLETE_DOWNLOAD:
            Serial.println("INCOMPLETE_DOWNLOAD - Will simulate incomplete download");
            break;
        default:
            Serial.println("NONE");
            break;
    }
    Serial.println("======================================\n");
}

/**
 * @brief Disable OTA fault testing mode
 */
void OTAManager::disableTestMode()
{
    if (testModeEnabled) {
        Serial.println("=== OTA FAULT TESTING MODE DISABLED ===");
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
    
    Serial.printf("\n⚠️  FAULT INJECTED: ");
    
    switch (faultType) {
        case OTA_FAULT_CORRUPT_CHUNK:
            Serial.println("Corrupting chunk data");
            // This will be handled in downloadChunk()
            return false;
            
        case OTA_FAULT_BAD_HMAC:
            Serial.println("Failing HMAC verification");
            setError("HMAC verification failed (TEST MODE)");
            otaFailureCount++;
            return false;
            
        case OTA_FAULT_BAD_HASH:
            Serial.println("Failing hash verification");
            setError("Hash verification failed (TEST MODE)");
            otaFailureCount++;
            return false;
            
        case OTA_FAULT_NETWORK_TIMEOUT:
            Serial.println("Simulating network timeout");
            delay(OTA_TIMEOUT_MS + 1000); // Force timeout
            setError("Network timeout (TEST MODE)");
            otaFailureCount++;
            return false;
            
        case OTA_FAULT_INCOMPLETE_DOWNLOAD:
            Serial.println("Simulating incomplete download");
            setError("Incomplete download (TEST MODE)");
            otaFailureCount++;
            return false;
            
        default:
            return true; // No fault
    }
}

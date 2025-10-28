#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
// Undef print macro before including ArduinoJson to avoid conflicts
#ifdef print
#undef print
#include <ArduinoJson.h>
#define print(...) debug.log(__VA_ARGS__)
#else
#include <ArduinoJson.h>
#endif
#include <base64.h>

// mbedtls includes for cryptographic functions
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>

// Include security keys
#include "keys.h"

// OTA Constants
#define OTA_TIMEOUT_MS 30000
#define OTA_CHUNK_SIZE 1024
#define RSA_KEY_SIZE 2048

// OTA State enumeration
enum OTAState {
    OTA_IDLE,
    OTA_CHECKING,
    OTA_DOWNLOADING,
    OTA_VERIFYING,
    OTA_APPLYING,
    OTA_COMPLETED,
    OTA_ERROR,
    OTA_ROLLBACK
};

// OTA Fault Test Types (for testing robustness)
enum OTAFaultType {
    OTA_FAULT_NONE = 0,
    OTA_FAULT_CORRUPT_CHUNK,      // Simulate corrupted chunk data
    OTA_FAULT_BAD_HMAC,            // Simulate HMAC verification failure
    OTA_FAULT_BAD_HASH,            // Simulate hash mismatch
    OTA_FAULT_NETWORK_TIMEOUT,     // Simulate network timeout
    OTA_FAULT_INCOMPLETE_DOWNLOAD  // Simulate incomplete download
};

// Firmware manifest structure
struct FirmwareManifest {
    String version;
    String sha256_hash;
    String signature;
    String iv;
    uint32_t original_size;
    uint32_t encrypted_size;
    uint32_t firmware_size;
    uint16_t chunk_size;
    uint16_t total_chunks;
    
    // Default constructor
    FirmwareManifest() : original_size(0), encrypted_size(0), firmware_size(0), chunk_size(0), total_chunks(0) {}
};

// OTA progress tracking structure
struct OTAProgress {
    uint16_t chunks_received;
    uint16_t total_chunks;
    uint32_t bytes_downloaded;
    uint8_t percentage;
    OTAState state;
    String error_message;
    unsigned long last_activity;
    
    // Default constructor
    OTAProgress() : chunks_received(0), total_chunks(0), bytes_downloaded(0), 
                   percentage(0), state(OTA_IDLE), last_activity(0) {}
};

class OTAManager {
public:
    // Constructor and destructor
    OTAManager(const String& serverURL, const String& deviceID, const String& currentVersion);
    ~OTAManager();
    
    // Main OTA operations
    bool checkForUpdate();
    bool downloadAndApplyFirmware();
    bool verifyAndReboot();
    void handleRollback();
    
    // Progress and status methods
    OTAProgress getProgress();
    String getStateString();
    bool isOTAInProgress();
    bool canResume();
    void clearProgress();
    
    // Configuration methods
    void setServerURL(const String& url);
    void setCheckInterval(unsigned long intervalMs);
    
    // Fault testing methods (for robustness testing)
    void enableTestMode(OTAFaultType faultType);
    void disableTestMode();
    bool isTestMode() const { return testModeEnabled; }
    OTAFaultType getTestFaultType() const { return testFaultType; }
    void getOTAStatistics(uint32_t& successCount, uint32_t& failureCount, uint32_t& rollbackCount);

private:
    // Configuration
    String serverURL;
    String deviceID;
    String currentVersion;
    unsigned long checkInterval;
    
    // OTA state and data structures
    OTAState state;
    FirmwareManifest manifest;
    OTAProgress progress;
    
    // NVS storage for persistence
    Preferences nvs;
    
    // Cryptographic contexts
    mbedtls_aes_context aes_ctx;
    uint8_t aes_iv[16];
    
    // Note: Using Arduino Update library, not ESP-IDF OTA API
    // esp_ota_handle_t not needed
    
    // Decryption buffer
    uint8_t* decryptBuffer;
    static const size_t DECRYPT_BUFFER_SIZE = 2048;
    
    // Fault testing variables
    bool testModeEnabled;
    OTAFaultType testFaultType;
    uint32_t otaSuccessCount;
    uint32_t otaFailureCount;
    uint32_t otaRollbackCount;
    
    // Private methods - Network operations
    bool requestManifest();
    bool downloadChunk(uint16_t chunkNumber);
    bool httpPost(const String& endpoint, const String& payload, String& response);
    bool httpGet(const String& endpoint, String& response);
    
    // Private methods - Cryptographic operations
    bool decryptChunk(const uint8_t* encrypted, size_t encLen, uint8_t* decrypted, size_t* decLen, uint16_t chunkNumber);
    bool verifySignature(const String& base64Signature);
    bool verifyFirmwareHash();
    bool verifyRSASignature(const uint8_t* hash, const uint8_t* signature);
    bool verifyChunkHMAC(const uint8_t* chunkData, size_t len, uint16_t chunkNum, const String& expectedHMAC);
    String calculateSHA256(const uint8_t* data, size_t len);
    
    // Private methods - Fault injection (for testing)
    bool simulateFault(OTAFaultType faultType);
    bool shouldInjectFault();
    
    // Private methods - Progress and state management
    void saveProgress();
    void loadProgress();
    bool runDiagnostics();
    void setError(const String& message);
    void setOTAState(OTAState newState);
    void updateProgress(uint32_t bytes, uint16_t chunks);
    OTAState getState();
    bool isTimeout();
    void reset();
    int base64_decode(const char* input, uint8_t* output, size_t output_size);
};

#endif // OTA_MANAGER_H
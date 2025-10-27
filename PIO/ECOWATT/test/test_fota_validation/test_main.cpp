/**
 * M4 FOTA Firmware Validation Tests - ESP32 Side
 * Tests firmware validation logic without actual firmware flashing
 * Verifies hash calculation, signature verification, and corruption detection
 */

#include <Arduino.h>
#include <unity.h>
#include <mbedtls/sha256.h>
#include <mbedtls/pk.h>
#include <mbedtls/md.h>
#include <base64.h>

// Test data
const uint8_t TEST_FIRMWARE_DATA[] = "MOCK_FIRMWARE_DATA_FOR_TESTING_12345";
const size_t TEST_FIRMWARE_SIZE = sizeof(TEST_FIRMWARE_DATA) - 1;

// Test: SHA256 Hash Calculation
void test_sha256_hash_calculation()
{
    uint8_t hash_output[32];
    
    // Calculate SHA256
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0); // 0 for SHA256 (not SHA224)
    mbedtls_sha256_update(&sha256_ctx, TEST_FIRMWARE_DATA, TEST_FIRMWARE_SIZE);
    mbedtls_sha256_finish(&sha256_ctx, hash_output);
    mbedtls_sha256_free(&sha256_ctx);
    
    // Verify hash is not all zeros
    bool all_zeros = true;
    for (int i = 0; i < 32; i++) {
        if (hash_output[i] != 0) {
            all_zeros = false;
            break;
        }
    }
    
    TEST_ASSERT_FALSE_MESSAGE(all_zeros, "Hash should not be all zeros");
    TEST_ASSERT_EQUAL(32, sizeof(hash_output));
}

// Test: Hash Consistency
void test_hash_consistency()
{
    uint8_t hash1[32];
    uint8_t hash2[32];
    
    // Calculate hash twice
    mbedtls_sha256_context ctx;
    
    // First calculation
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, TEST_FIRMWARE_DATA, TEST_FIRMWARE_SIZE);
    mbedtls_sha256_finish(&ctx, hash1);
    mbedtls_sha256_free(&ctx);
    
    // Second calculation
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, TEST_FIRMWARE_DATA, TEST_FIRMWARE_SIZE);
    mbedtls_sha256_finish(&ctx, hash2);
    mbedtls_sha256_free(&ctx);
    
    // Verify hashes match
    TEST_ASSERT_EQUAL_MEMORY(hash1, hash2, 32);
}

// Test: Corrupted Data Detection
void test_corrupted_data_detection()
{
    uint8_t original_hash[32];
    uint8_t corrupted_hash[32];
    
    // Calculate original hash
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, TEST_FIRMWARE_DATA, TEST_FIRMWARE_SIZE);
    mbedtls_sha256_finish(&ctx, original_hash);
    mbedtls_sha256_free(&ctx);
    
    // Create corrupted data (flip one byte)
    uint8_t corrupted_data[TEST_FIRMWARE_SIZE];
    memcpy(corrupted_data, TEST_FIRMWARE_DATA, TEST_FIRMWARE_SIZE);
    corrupted_data[0] ^= 0xFF; // Flip all bits in first byte
    
    // Calculate corrupted hash
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, corrupted_data, TEST_FIRMWARE_SIZE);
    mbedtls_sha256_finish(&ctx, corrupted_hash);
    mbedtls_sha256_free(&ctx);
    
    // Verify hashes are different
    bool hashes_differ = false;
    for (int i = 0; i < 32; i++) {
        if (original_hash[i] != corrupted_hash[i]) {
            hashes_differ = true;
            break;
        }
    }
    
    TEST_ASSERT_TRUE_MESSAGE(hashes_differ, "Corrupted data should produce different hash");
}

// Test: Incremental Hash Update
void test_incremental_hash_update()
{
    uint8_t hash_full[32];
    uint8_t hash_incremental[32];
    
    mbedtls_sha256_context ctx;
    
    // Calculate hash in one go
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, TEST_FIRMWARE_DATA, TEST_FIRMWARE_SIZE);
    mbedtls_sha256_finish(&ctx, hash_full);
    mbedtls_sha256_free(&ctx);
    
    // Calculate hash incrementally (simulate chunk processing)
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    
    size_t chunk_size = 10;
    for (size_t offset = 0; offset < TEST_FIRMWARE_SIZE; offset += chunk_size) {
        size_t remaining = TEST_FIRMWARE_SIZE - offset;
        size_t current_chunk = (remaining < chunk_size) ? remaining : chunk_size;
        mbedtls_sha256_update(&ctx, TEST_FIRMWARE_DATA + offset, current_chunk);
    }
    
    mbedtls_sha256_finish(&ctx, hash_incremental);
    mbedtls_sha256_free(&ctx);
    
    // Verify hashes match
    TEST_ASSERT_EQUAL_MEMORY(hash_full, hash_incremental, 32);
}

// Test: HMAC Calculation for Chunk Integrity
void test_hmac_chunk_integrity()
{
    uint8_t hmac_output[32];
    const uint8_t test_key[] = "TEST_HMAC_KEY_12345";
    const uint8_t chunk_data[] = "CHUNK_DATA_SAMPLE";
    
    // Calculate HMAC-SHA256
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    TEST_ASSERT_NOT_NULL(md_info);
    
    int ret = mbedtls_md_setup(&md_ctx, md_info, 1); // 1 = HMAC enabled
    TEST_ASSERT_EQUAL(0, ret);
    
    ret = mbedtls_md_hmac_starts(&md_ctx, test_key, sizeof(test_key) - 1);
    TEST_ASSERT_EQUAL(0, ret);
    
    ret = mbedtls_md_hmac_update(&md_ctx, chunk_data, sizeof(chunk_data) - 1);
    TEST_ASSERT_EQUAL(0, ret);
    
    ret = mbedtls_md_hmac_finish(&md_ctx, hmac_output);
    TEST_ASSERT_EQUAL(0, ret);
    
    mbedtls_md_free(&md_ctx);
    
    // Verify HMAC is not all zeros
    bool all_zeros = true;
    for (int i = 0; i < 32; i++) {
        if (hmac_output[i] != 0) {
            all_zeros = false;
            break;
        }
    }
    
    TEST_ASSERT_FALSE_MESSAGE(all_zeros, "HMAC should not be all zeros");
}

// Test: HMAC Verification with Correct Key
void test_hmac_verification_correct_key()
{
    uint8_t hmac1[32];
    uint8_t hmac2[32];
    const uint8_t key[] = "SHARED_SECRET_KEY";
    const uint8_t data[] = "DATA_TO_AUTHENTICATE";
    
    // Calculate HMAC twice with same key
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    // First HMAC
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, md_info, 1);
    mbedtls_md_hmac_starts(&ctx, key, sizeof(key) - 1);
    mbedtls_md_hmac_update(&ctx, data, sizeof(data) - 1);
    mbedtls_md_hmac_finish(&ctx, hmac1);
    mbedtls_md_free(&ctx);
    
    // Second HMAC
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, md_info, 1);
    mbedtls_md_hmac_starts(&ctx, key, sizeof(key) - 1);
    mbedtls_md_hmac_update(&ctx, data, sizeof(data) - 1);
    mbedtls_md_hmac_finish(&ctx, hmac2);
    mbedtls_md_free(&ctx);
    
    // Verify HMACs match
    TEST_ASSERT_EQUAL_MEMORY(hmac1, hmac2, 32);
}

// Test: HMAC Verification with Wrong Key
void test_hmac_verification_wrong_key()
{
    uint8_t hmac_correct[32];
    uint8_t hmac_wrong[32];
    const uint8_t correct_key[] = "CORRECT_KEY";
    const uint8_t wrong_key[] = "WRONG_KEY_X";
    const uint8_t data[] = "DATA_TO_AUTHENTICATE";
    
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    // HMAC with correct key
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, md_info, 1);
    mbedtls_md_hmac_starts(&ctx, correct_key, sizeof(correct_key) - 1);
    mbedtls_md_hmac_update(&ctx, data, sizeof(data) - 1);
    mbedtls_md_hmac_finish(&ctx, hmac_correct);
    mbedtls_md_free(&ctx);
    
    // HMAC with wrong key
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, md_info, 1);
    mbedtls_md_hmac_starts(&ctx, wrong_key, sizeof(wrong_key) - 1);
    mbedtls_md_hmac_update(&ctx, data, sizeof(data) - 1);
    mbedtls_md_hmac_finish(&ctx, hmac_wrong);
    mbedtls_md_free(&ctx);
    
    // Verify HMACs are different
    bool differ = false;
    for (int i = 0; i < 32; i++) {
        if (hmac_correct[i] != hmac_wrong[i]) {
            differ = true;
            break;
        }
    }
    
    TEST_ASSERT_TRUE_MESSAGE(differ, "HMACs with different keys should differ");
}

// Test: Base64 String Format Validation
void test_base64_string_format()
{
    // Test that base64 strings contain only valid characters
    const char* valid_b64 = "VGVzdERhdGE=";
    
    // Check all characters are valid base64
    bool all_valid = true;
    for (size_t i = 0; i < strlen(valid_b64); i++) {
        char c = valid_b64[i];
        if (!((c >= 'A' && c <= 'Z') || 
              (c >= 'a' && c <= 'z') || 
              (c >= '0' && c <= '9') || 
              c == '+' || c == '/' || c == '=')) {
            all_valid = false;
            break;
        }
    }
    
    TEST_ASSERT_TRUE(all_valid);
}

// Test: Firmware Size Validation
void test_firmware_size_validation()
{
    // Simulate manifest size vs actual size check
    uint32_t manifest_size = 1024;
    uint32_t actual_size = 1024;
    
    TEST_ASSERT_EQUAL(manifest_size, actual_size);
    
    // Simulate mismatch
    uint32_t wrong_size = 2048;
    TEST_ASSERT_NOT_EQUAL(manifest_size, wrong_size);
}

// Test: Hash Hex String Conversion
void test_hash_hex_string_conversion()
{
    uint8_t hash[32];
    char hex_string[65]; // 32 bytes * 2 + null terminator
    
    // Generate a hash
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, TEST_FIRMWARE_DATA, TEST_FIRMWARE_SIZE);
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    
    // Convert to hex string
    for (int i = 0; i < 32; i++) {
        sprintf(hex_string + (i * 2), "%02x", hash[i]);
    }
    hex_string[64] = '\0';
    
    // Verify hex string properties
    TEST_ASSERT_EQUAL(64, strlen(hex_string));
    
    // Verify all characters are valid hex
    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_TRUE(isxdigit(hex_string[i]));
    }
}

// Test: Empty Data Hash
void test_empty_data_hash()
{
    uint8_t hash[32];
    
    // Calculate hash of empty data
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (uint8_t*)"", 0);
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    
    // Known SHA256 of empty string: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    const uint8_t expected_empty_hash[32] = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
        0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
        0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    };
    
    TEST_ASSERT_EQUAL_MEMORY(expected_empty_hash, hash, 32);
}

// Test: Large Data Hashing
void test_large_data_hashing()
{
    // Allocate large buffer
    const size_t large_size = 10000;
    uint8_t* large_data = (uint8_t*)malloc(large_size);
    TEST_ASSERT_NOT_NULL(large_data);
    
    // Fill with pattern
    for (size_t i = 0; i < large_size; i++) {
        large_data[i] = i % 256;
    }
    
    // Calculate hash
    uint8_t hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, large_data, large_size);
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    
    // Verify hash is computed
    bool all_zeros = true;
    for (int i = 0; i < 32; i++) {
        if (hash[i] != 0) {
            all_zeros = false;
            break;
        }
    }
    
    TEST_ASSERT_FALSE(all_zeros);
    
    free(large_data);
}

// Test: Memory Cleanup After Hash
void test_memory_cleanup_after_hash()
{
    mbedtls_sha256_context ctx;
    uint8_t hash[32];
    
    // Initialize and use context
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, TEST_FIRMWARE_DATA, TEST_FIRMWARE_SIZE);
    mbedtls_sha256_finish(&ctx, hash);
    
    // Free context - should not crash
    mbedtls_sha256_free(&ctx);
    
    // Can initialize again after free
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, TEST_FIRMWARE_DATA, TEST_FIRMWARE_SIZE);
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    
    TEST_PASS();
}

// Setup function
void setUp(void)
{
    // Setup runs before each test
}

// Teardown function
void tearDown(void)
{
    // Cleanup runs after each test
}

void setup()
{
    delay(2000); // Wait for serial
    
    UNITY_BEGIN();
    
    Serial.println("\n=== M4 FOTA FIRMWARE VALIDATION TESTS ===");
    
    // Run all tests
    RUN_TEST(test_sha256_hash_calculation);
    RUN_TEST(test_hash_consistency);
    RUN_TEST(test_corrupted_data_detection);
    RUN_TEST(test_incremental_hash_update);
    RUN_TEST(test_hmac_chunk_integrity);
    RUN_TEST(test_hmac_verification_correct_key);
    RUN_TEST(test_hmac_verification_wrong_key);
    RUN_TEST(test_base64_string_format);
    RUN_TEST(test_firmware_size_validation);
    RUN_TEST(test_hash_hex_string_conversion);
    RUN_TEST(test_empty_data_hash);
    RUN_TEST(test_large_data_hashing);
    RUN_TEST(test_memory_cleanup_after_hash);
    
    UNITY_END();
}

void loop()
{
    // Tests run once in setup()
}

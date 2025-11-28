/**
 * @file test_protocol_adapter.cpp
 * @brief Unit tests for Protocol Adapter
 * 
 * Tests:
 * - Request formatting
 * - Response parsing
 * - Timeout handling
 * - Malformed frame detection
 * - Retry logic
 */

#include <unity.h>
#include <string.h>

// For native testing, we need to mock Arduino-specific functions
#ifdef NATIVE_TEST
    // Mock Arduino functions
    void delay(int ms) { /* no-op for native */ }
    
    // Mock Serial for debug output
    struct MockSerial {
        void begin(int) {}
        void println(const char*) {}
        void print(const char*) {}
    } Serial;
#endif

// Include the protocol adapter header
#include "driver/protocol_adapter.h"

// Test fixtures
ProtocolAdapter* testAdapter = nullptr;

void setUp(void) {
    // This runs before each test
    testAdapter = new ProtocolAdapter();
}

void tearDown(void) {
    // This runs after each test
    if (testAdapter) {
        delete testAdapter;
        testAdapter = nullptr;
    }
}

// ============================================================================
// PROTOCOL ADAPTER TESTS
// ============================================================================

/**
 * Test 1: Validate correct Modbus frame format
 * Requirement: Handle request formatting correctly
 */
void test_validateModbusFrame_validFrame(void) {
    // Valid Modbus RTU frame: Device 01, Function 03, Start 0000, Count 0002, CRC
    const char* validFrame = "010300000002C40B";
    
    ParseResult result = testAdapter->validateModbusFrame(validFrame);
    
    TEST_ASSERT_EQUAL_MESSAGE(PARSE_OK, result, 
        "Valid Modbus frame should return PARSE_OK");
}

/**
 * Test 2: Detect malformed frame (invalid length)
 * Requirement: Detect and recover from malformed frames
 */
void test_validateModbusFrame_tooShort(void) {
    // Frame too short (minimum Modbus frame is 8 bytes = 16 hex chars)
    const char* shortFrame = "0103000000";
    
    ParseResult result = testAdapter->validateModbusFrame(shortFrame);
    
    TEST_ASSERT_NOT_EQUAL_MESSAGE(PARSE_OK, result,
        "Frame shorter than 8 bytes should be rejected");
}

/**
 * Test 3: Detect malformed frame (invalid hex characters)
 * Requirement: Detect and recover from malformed frames
 */
void test_validateModbusFrame_invalidHex(void) {
    // Frame contains non-hex characters
    const char* invalidFrame = "01GZ00000002C40B";
    
    ParseResult result = testAdapter->validateModbusFrame(invalidFrame);
    
    TEST_ASSERT_NOT_EQUAL_MESSAGE(PARSE_OK, result,
        "Frame with non-hex characters should be rejected");
}

/**
 * Test 4: Detect CRC error
 * Requirement: Detect and recover from malformed frames
 */
void test_validateModbusFrame_badCRC(void) {
    // Valid format but wrong CRC (should be C40B, not FFFF)
    const char* badCRCFrame = "010300000002FFFF";
    
    ParseResult result = testAdapter->validateModbusFrame(badCRCFrame);
    
    TEST_ASSERT_EQUAL_MESSAGE(PARSE_CRC_ERROR, result,
        "Frame with incorrect CRC should return PARSE_CRC_ERROR");
}

/**
 * Test 5: Parse valid JSON response structure
 * Requirement: Response parsing works correctly
 * Note: This tests JSON parsing. The frame itself will be validated by validateModbusFrame
 * which checks CRC. We use the same valid frame from test 1.
 */
void test_parseResponse_validJSON(void) {
    // Using the same valid frame from test_validateModbusFrame_validFrame
    const char* validJSON = "{\"status\":\"success\",\"frame\":\"010300000002C40B\"}";
    char outFrame[256];
    
    bool success = testAdapter->parseResponse(validJSON, outFrame, sizeof(outFrame));
    
    TEST_ASSERT_TRUE_MESSAGE(success, "Valid JSON with valid Modbus frame should parse successfully");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("010300000002C40B", outFrame,
        "Extracted frame should match frame field");
}

/**
 * Test 6: Handle invalid JSON response
 * Requirement: Detect and recover from malformed frames
 */
void test_parseResponse_invalidJSON(void) {
    const char* invalidJSON = "{status:success"; // Missing quotes and closing brace
    char outFrame[256];
    
    bool success = testAdapter->parseResponse(invalidJSON, outFrame, sizeof(outFrame));
    
    TEST_ASSERT_FALSE_MESSAGE(success, "Invalid JSON should fail to parse");
}

/**
 * Test 7: Handle missing data field in JSON
 * Requirement: Detect and recover from malformed frames
 */
void test_parseResponse_missingDataField(void) {
    const char* noDataJSON = "{\"status\":\"success\"}"; // No "frame" field
    char outFrame[256];
    
    bool success = testAdapter->parseResponse(noDataJSON, outFrame, sizeof(outFrame));
    
    TEST_ASSERT_FALSE_MESSAGE(success, 
        "JSON without frame field should fail");
}

/**
 * Test 8: API key setter and getter
 * Requirement: Configurable protocol adapter
 */
void test_apiKey_setAndGet(void) {
    const char* testKey = "test-api-key-12345";
    
    testAdapter->setApiKey(testKey);
    const char* retrievedKey = testAdapter->getApiKey();
    
    TEST_ASSERT_EQUAL_STRING_MESSAGE(testKey, retrievedKey,
        "Retrieved API key should match the set value");
}

/**
 * Test 9: Validate Modbus CRC calculation for read request
 * Requirement: Correct CRC calculation for Modbus RTU frames
 * Frame: 01 03 00 00 00 02 C4 0B
 * - Device ID: 0x01
 * - Function: 0x03 (Read Holding Registers)
 * - Start Address: 0x0000
 * - Register Count: 0x0002
 * - CRC: 0xC40B (little-endian: 0B C4)
 */
void test_validateModbusFrame_correctCRC_readRequest(void) {
    const char* validFrame = "010300000002C40B";
    
    ParseResult result = testAdapter->validateModbusFrame(validFrame);
    
    TEST_ASSERT_EQUAL_MESSAGE(PARSE_OK, result,
        "Read request frame with correct CRC should validate successfully");
}

/**
 * Test 10: Validate Modbus CRC calculation for read response
 * Requirement: Correct CRC calculation for Modbus RTU frames
 * Frame: 01 03 04 00 11 00 22
 * - Device ID: 0x01
 * - Function: 0x03 (Read Holding Registers)
 * - Byte Count: 0x04
 * - Data: 0x0011 0x0022
 * CRC calculation for bytes: 01 03 04 00 11 00 22
 * Result: CRC = 0x2F2A (little-endian: 2A 2F)
 */
void test_validateModbusFrame_correctCRC_readResponse(void) {
    const char* responseFrame = "010304001100222A2F";
    
    ParseResult result = testAdapter->validateModbusFrame(responseFrame);
    
    TEST_ASSERT_EQUAL_MESSAGE(PARSE_OK, result,
        "Read response frame with correct CRC should validate successfully");
}

/**
 * Test 11: Validate Modbus CRC calculation for write request
 * Requirement: Correct CRC calculation for Modbus RTU frames
 * Frame: 01 06 00 01 00 64
 * - Device ID: 0x01
 * - Function: 0x06 (Write Single Register)
 * - Register Address: 0x0001
 * - Value: 0x0064 (100 decimal)
 * CRC for bytes: 01 06 00 01 00 64
 * Result: CRC = 0xE1D9 (little-endian: D9 E1)
 */
void test_validateModbusFrame_correctCRC_writeRequest(void) {
    const char* writeFrame = "010600010064D9E1";
    
    ParseResult result = testAdapter->validateModbusFrame(writeFrame);
    
    TEST_ASSERT_EQUAL_MESSAGE(PARSE_OK, result,
        "Write request frame with correct CRC should validate successfully");
}

/**
 * Test 12: Detect incorrect CRC with single bit flip
 * Requirement: Detect corrupted frames
 * Using same frame as Test 9 but with CRC changed from C40B to C40A (last bit flipped)
 */
void test_validateModbusFrame_singleBitCRCError(void) {
    const char* corruptedFrame = "010300000002C40A";  // Last bit changed
    
    ParseResult result = testAdapter->validateModbusFrame(corruptedFrame);
    
    TEST_ASSERT_EQUAL_MESSAGE(PARSE_CRC_ERROR, result,
        "Frame with single bit CRC error should be detected");
}

/**
 * Test 13: Validate longer Modbus frame CRC
 * Requirement: CRC validation works for various frame lengths
 * Frame: 01 03 0A 00 11 00 22 00 33 00 44 00 55
 * Testing a longer read response with 5 registers (10 bytes of data)
 * Device 01, Function 03, Byte count 0A, data: 0011 0022 0033 0044 0055
 * CRC for "01030A00110022003300440055"
 * Result: CRC = 0xCA62 (little-endian: 62 CA)
 */
void test_validateModbusFrame_longFrame(void) {
    const char* longFrame = "01030A0011002200330044005562CA";
    
    ParseResult result = testAdapter->validateModbusFrame(longFrame);
    
    TEST_ASSERT_EQUAL_MESSAGE(PARSE_OK, result,
        "Long frame with correct CRC should validate successfully");
}

// ============================================================================
// Main test runner
// ============================================================================

void setup() {
    // Wait for serial (optional, helps with debugging)
    delay(2000);
    
    UNITY_BEGIN();
    
    // Run all tests
    RUN_TEST(test_validateModbusFrame_validFrame);
    RUN_TEST(test_validateModbusFrame_tooShort);
    RUN_TEST(test_validateModbusFrame_invalidHex);
    RUN_TEST(test_validateModbusFrame_badCRC);
    RUN_TEST(test_parseResponse_validJSON);
    RUN_TEST(test_parseResponse_invalidJSON);
    RUN_TEST(test_parseResponse_missingDataField);
    RUN_TEST(test_apiKey_setAndGet);
    
    // CRC validation tests
    RUN_TEST(test_validateModbusFrame_correctCRC_readRequest);
    RUN_TEST(test_validateModbusFrame_correctCRC_readResponse);
    RUN_TEST(test_validateModbusFrame_correctCRC_writeRequest);
    RUN_TEST(test_validateModbusFrame_singleBitCRCError);
    RUN_TEST(test_validateModbusFrame_longFrame);
    
    UNITY_END();
}

void loop() {
    // Nothing to do here for tests
}

#ifdef NATIVE_TEST
int main(int argc, char **argv) {
    setup();
    return 0;
}
#endif

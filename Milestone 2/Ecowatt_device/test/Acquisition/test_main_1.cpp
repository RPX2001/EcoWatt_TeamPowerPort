#include <Arduino.h>
#include <unity.h>
#include "protocol_adapter.h"
#include "aquisition.h"

void test_build_frame(void) {
    const RegID selection[] = {REG_VAC1, REG_IAC1, REG_IPV1, REG_PAC};
    uint16_t startAddr, count;
    String frame = buildReadFrame(0x11, selection, 4, startAddr, count);
    TEST_ASSERT_EQUAL_STRING("110300000004C5CB", frame.c_str());  // <-- expected frame (example)
}

void test_decode_frame(void) {
    const RegID selection[] = {REG_VAC1, REG_IAC1};
    uint16_t startAddr = 0;
    uint16_t count = 2;

    String response_frame = "1103040904002A2870"; // Vac1=2308, Iac1=42 (example)
    DecodedValues values = decodeReadResponse(response_frame, startAddr, count, selection, 2);

    TEST_ASSERT_EQUAL(2308, values.values[0]);
    TEST_ASSERT_EQUAL(42, values.values[1]);
}

void setup() {
    delay(2000); // Wait for serial monitor
    UNITY_BEGIN();
    RUN_TEST(test_build_frame);
    RUN_TEST(test_decode_frame);
    UNITY_END();
}

void loop() {
    // not used in unit testing
}

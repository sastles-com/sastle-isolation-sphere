#include <Arduino.h>
#include <M5Unified.h>
#include <LittleFS.h>
#include <unity.h>

// Test PSRAM initialization and size
void test_psram_check(void) {
    TEST_ASSERT_TRUE(psramFound());
    // AtomS3 has 8MB PSRAM, let's check for at least 7MB
    TEST_ASSERT_GREATER_THAN_UINT32(7 * 1024 * 1024, ESP.getPsramSize());
}

// Test LittleFS initialization
void test_filesystem_init(void) {
    TEST_ASSERT_TRUE(LittleFS.begin(true));
}

void setup() {
    delay(2000);
    M5.begin();
    
    UNITY_BEGIN();
    
    // Run hardware-specific tests
    RUN_TEST(test_psram_check);
    RUN_TEST(test_filesystem_init);
    
    UNITY_END();
}

void loop() {
    // Stop after tests are done
}

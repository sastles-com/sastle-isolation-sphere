#include <Arduino.h>
#include <unity.h>

// Forward declarations of test functions from test_device.cpp
void test_psram_check(void);
void test_filesystem_init_and_diag(void);
void test_update_delegates_processing(void);
void test_get_imu_data_delegation(void);
void test_led_control_delegation(void);
void test_display_message_delegation(void);

void setup() {
    // Wait for serial port to connect. Needed for native USB.
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_psram_check);
    RUN_TEST(test_filesystem_init_and_diag);
    RUN_TEST(test_update_delegates_processing);
    RUN_TEST(test_get_imu_data_delegation);
    RUN_TEST(test_led_control_delegation);
    RUN_TEST(test_display_message_delegation);
    UNITY_END();
}

void loop() {
    // Nothing to do here
}

#include <unity.h>
#include "DeviceManager.h"
#include "ConfigManager.h"
#include "../src/boards/Board_M5AtomS3R.h" // Include board definition

#ifdef NATIVE_ENV
// ===== Mocks for Native Test =====
#include "../mocks/MockIMU.h"
#include "../mocks/MockLEDController.h"
#include "../mocks/MockSpeaker.h"
#include "../mocks/MockDisplay.h"
#include "../mocks/NullDisplay.h"
#include "../mocks/LittleFS.h"
#include "../mocks/Arduino.h"
#else
// ===== Real Hardware Includes =====
#include <M5Unified.h>
#include "../src/adapters/M5DisplayAdapter.h"
#endif


// ===== Global Variables =====
ConfigManager* configManager;
DeviceManager* deviceManager;

#ifdef NATIVE_ENV
// --- Mock Objects ---
MockIMU* mockIMU;
MockLEDController* mockLEDController;
MockSpeaker* mockSpeaker;
IDisplay* display; // Use interface pointer
#else
// --- Real Objects ---
IMU* realIMU;
LEDController* realLEDController;
Speaker* realSpeaker;
IDisplay* realDisplay;
#endif


// ===== Test Setup & Teardown =====

void setUp(void) {
#ifdef NATIVE_ENV
    // --- Native (Mock) Setup ---
    configManager = new ConfigManager();
    mockIMU = new MockIMU();
    mockLEDController = new MockLEDController(*configManager);
    mockSpeaker = new MockSpeaker();

    #if defined(BOARD_HAS_LCD) && BOARD_HAS_LCD > 0
        display = new MockDisplay();
    #else
        display = new NullDisplay();
    #endif
    
    deviceManager = new DeviceManager(*configManager, *mockIMU, *mockLEDController, *mockSpeaker, *display);
#else
    // --- Hardware Setup ---
    M5.begin();
    delay(100); // Wait for M5.begin() to complete
    
    configManager = new ConfigManager();
    realIMU = new IMU(I2C_SDA_PIN, I2C_SCL_PIN);
    realLEDController = new LEDController(*configManager);
    realSpeaker = new Speaker(SPEAKER_PIN);
    
    #if defined(BOARD_HAS_LCD) && BOARD_HAS_LCD > 0
        realDisplay = new M5DisplayAdapter();
    #else
        // If we were to test a board without LCD, we would use NullDisplay here.
        // For this test on M5AtomS3R, we assume LCD is present.
        realDisplay = new M5DisplayAdapter(); 
    #endif
    
    deviceManager = new DeviceManager(*configManager, *realIMU, *realLEDController, *realSpeaker, *realDisplay);
#endif
}

void tearDown(void) {
#ifdef NATIVE_ENV
    // --- Native (Mock) Teardown ---
    delete deviceManager;
    delete display;
    delete mockSpeaker;
    delete mockLEDController;
    delete mockIMU;
    delete configManager;
#else
    // --- Hardware Teardown ---
    delete deviceManager;
    delete realDisplay;
    delete realSpeaker;
    delete realLEDController;
    delete realIMU;
    delete configManager;
#endif
}

// void test_m5_initialization(void) { // This test is for M5Unified, which we are decoupling from
//     printf("--- Running test: test_m5_initialization ---\n");
//     deviceManager->begin();
//     TEST_ASSERT_TRUE(M5.beginCalled);
//     printf("--- Finished test: test_m5_initialization ---\n");
// }

void test_psram_check(void) {
    printf("--- Running test: test_psram_check ---\n");
    #ifdef NATIVE_ENV
    // This test mainly verifies it doesn't crash and calls ESP.getFreePsram
    // We can't easily verify the print output without capturing Serial, 
    // but we can assume if it runs without error it's fine for now.
    deviceManager->begin();
    // In a real scenario we'd mock Serial and check the buffer.
    #else
    // Hardware test: check if PSRAM is actually found and has a reasonable size
    deviceManager->begin();
    TEST_ASSERT_TRUE(psramFound());
    TEST_ASSERT_GREATER_THAN_UINT32(2 * 1024 * 1024, ESP.getPsramSize());
    #endif
    printf("--- Finished test: test_psram_check ---\n");
}

void test_filesystem_init_and_diag(void) {
    printf("--- Running test: test_filesystem_init_and_diag ---\n");
    #ifdef NATIVE_ENV
    // LittleFS mock always returns true for begin()
    deviceManager->begin();
    // Verify side effects if possible, or just ensure no crash
    #else
    // Hardware test: check if LittleFS mounts successfully
    TEST_ASSERT_TRUE(deviceManager->begin());
    #endif
    printf("--- Finished test: test_filesystem_init_and_diag ---\n");
}

void test_update_delegates_processing(void) {
    printf("--- Running test: test_update_delegates_processing ---\n");
    deviceManager->update();
    #ifdef NATIVE_ENV
    TEST_ASSERT_TRUE(mockIMU->updateCalled);
    TEST_ASSERT_TRUE(mockLEDController->updateCalled); // or showCalled depending on implementation
    TEST_ASSERT_TRUE(mockSpeaker->updateCalled);
    #else
    // On hardware, just ensure it runs without crashing
    #endif
    printf("--- Finished test: test_update_delegates_processing ---\n");
}

void test_get_imu_data_delegation(void) {
    printf("--- Running test: test_get_imu_data_delegation ---\n");
    float w, x, y, z;
    deviceManager->getImuData(w, x, y, z);

    #ifdef NATIVE_ENV
    // Setup mock data
    mockIMU->mockW = 0.5;
    mockIMU->mockX = 0.1;
    mockIMU->mockY = 0.2;
    mockIMU->mockZ = 0.3;
    deviceManager->getImuData(w, x, y, z); // Call again to get mock data
    TEST_ASSERT_EQUAL_FLOAT(0.5, w);
    TEST_ASSERT_EQUAL_FLOAT(0.1, x);
    TEST_ASSERT_EQUAL_FLOAT(0.2, y);
    TEST_ASSERT_EQUAL_FLOAT(0.3, z);
    #else
    // On hardware, just print the real values for inspection
    printf("IMU Quaternion: w=%.3f, x=%.3f, y=%.3f, z=%.3f\n", w, x, y, z);
    #endif
    printf("--- Finished test: test_get_imu_data_delegation ---\n");
}

void test_led_control_delegation(void) {
    printf("--- Running test: test_led_control_delegation ---\n");
    CRGB color(255, 0, 0);
    deviceManager->setLed(5, color);

    #ifdef NATIVE_ENV
    TEST_ASSERT_EQUAL(5, mockLEDController->lastLedIndex);
    TEST_ASSERT_EQUAL_UINT8(255, mockLEDController->lastLedColor.r);
    TEST_ASSERT_EQUAL_UINT8(0, mockLEDController->lastLedColor.g);
    TEST_ASSERT_EQUAL_UINT8(0, mockLEDController->lastLedColor.b);
    #else
    // On hardware, just visually check if the LED turns red
    deviceManager->showLeds();
    delay(500);
    #endif
    printf("--- Finished test: test_led_control_delegation ---\n");
}

void test_display_message_delegation(void) {
    #if defined(BOARD_HAS_LCD) && BOARD_HAS_LCD > 0
    printf("--- Running test: test_display_message_delegation ---\n");
    deviceManager->displayMessage("Hello, %s!", "World");
    #ifdef NATIVE_ENV
    // Cast display pointer to MockDisplay to access buffer
    MockDisplay* mockDisplay = static_cast<MockDisplay*>(display);
    TEST_ASSERT_EQUAL_STRING("Hello, World!", mockDisplay->buffer.c_str());
    #else
    // On hardware, just visually check the display
    delay(500);
    #endif
    printf("--- Finished test: test_display_message_delegation ---\n");
    #else
    TEST_IGNORE_MESSAGE("Board does not have an LCD, skipping display test.");
    #endif
}



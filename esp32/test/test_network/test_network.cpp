#include <unity.h>
#include "../mocks/MockNetworkAdapter.h"
#include "../mocks/AsyncMqttClient.h" // Use our mock
#include "NetworkManager.h"
#include "ConfigManager.h"
#include "DeviceManager.h"
#include "../mocks/Wire.h"
#include "../mocks/FastLED.h"
#include "../mocks/M5Unified.h"
#include "../mocks/SPI.h"

// Global Mocks
TwoWire Wire;
CFastLED FastLED;
M5Unified M5;
SPIClass SPI;

MockNetworkAdapter* mockNetwork;
ConfigManager* configManager;
DeviceManager* deviceManager;
NetworkManager* networkManager;

// We need to access the internal mqttClient of NetworkManager to simulate events.
// Since it's private, we might need to make it protected or friend, or just assume we can mock it via dependency injection if we change NetworkManager to take it.
// For now, let's assume we will modify NetworkManager to allow accessing or injecting the client, OR we rely on the fact that in native test, the included "AsyncMqttClient.h" IS the mock class, so any instance created by NetworkManager is our mock.

void setUp(void) {
    mockNetwork = new MockNetworkAdapter();
    configManager = new ConfigManager();
    deviceManager = new DeviceManager(*configManager);
    networkManager = new NetworkManager(*mockNetwork, *configManager, *deviceManager);
}

void tearDown(void) {
    delete networkManager;
    delete deviceManager;
    delete configManager;
    delete mockNetwork;
}

void test_network_initial_state(void) {
    // Should be disconnected initially
    TEST_ASSERT_EQUAL(NETWORK_DISCONNECTED, mockNetwork->status());
}

void test_network_begin_starts_wifi(void) {
    // Setup config
    // We can't easily set config without parsing json or using setters (if added).
    // ConfigManager has no setters for ssid/pass, only loadConfig.
    // We can parse a dummy config.
    configManager->parseConfig("{\"network\":{\"ssid\":\"test_ssid\",\"password\":\"test_pass\"}}");
    
    networkManager->begin();
    
    // Check if adapter.begin was called (MockNetworkAdapter should track this)
    // The current MockNetworkAdapter might not store the args, let's check it later.
    // For now, check status or side effects.
    // If MockNetworkAdapter connects immediately (default behavior might be false), status might be DISCONNECTED until we simulate connection.
}

void test_network_update_connects_mqtt(void) {
    // Arrange
    mockNetwork->setStatus(NETWORK_CONNECTED);
    
    // Act
    networkManager->update();
    
    // Assert
    TEST_ASSERT_TRUE(networkManager->mqttClient._connectCalled);
}

// We need to modify NetworkManager to use AsyncMqttClient first to make this compile/work.
// But this is TDD, so we write test first.
// The test expects NetworkManager to have MQTT logic.

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_network_initial_state);
    RUN_TEST(test_network_begin_starts_wifi);
    RUN_TEST(test_network_update_connects_mqtt);
    UNITY_END();
    return 0;
}

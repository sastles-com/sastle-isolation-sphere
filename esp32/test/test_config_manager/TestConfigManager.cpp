#include "ConfigManager.h"
#include <unity.h>

#include <unity.h>
#include "../mocks/Wire.h"
#include "../mocks/FastLED.h"
#include "../mocks/M5Unified.h"
#include "../mocks/SPI.h"

// Global Mocks
TwoWire Wire;
CFastLED FastLED;
M5Unified M5;
SPIClass SPI;

ConfigManager *configManager;

void setUp(void) { configManager = new ConfigManager(); }

void tearDown(void) { delete configManager; }

void test_config_defaults(void) {
  // Check default values defined in ConfigManager constructor
  TEST_ASSERT_EQUAL_STRING("isolation_sphere_esp32",
                           configManager->getNodeName().c_str());
  TEST_ASSERT_EQUAL(1883, configManager->getAgentPort());
  TEST_ASSERT_FALSE(configManager->getLcdDebug());

  // Check that other values are empty by default
  TEST_ASSERT_EQUAL_STRING("", configManager->getWifiSSID().c_str());
  TEST_ASSERT_EQUAL_STRING("", configManager->getWifiPassword().c_str());
  TEST_ASSERT_EQUAL_STRING("", configManager->getAgentIP().c_str());
}

void test_parse_valid_config(void) {
  String json = "{\"network\":{\"ssid\":\"test_ssid\",\"password\":\"test_"
                "pass\"},\"agent\":{\"ip\":\"192.168.1.10\",\"port\":1883},"
                "\"node_name\":\"test_node\",\"debug\":{\"lcd_enable\":true}}";

  TEST_ASSERT_TRUE(configManager->parseConfig(json));
  TEST_ASSERT_EQUAL_STRING("test_ssid", configManager->getWifiSSID().c_str());
  TEST_ASSERT_EQUAL_STRING("test_pass",
                           configManager->getWifiPassword().c_str());
  TEST_ASSERT_EQUAL_STRING("192.168.1.10", configManager->getAgentIP().c_str());
  TEST_ASSERT_EQUAL(1883, configManager->getAgentPort());
  TEST_ASSERT_EQUAL_STRING("test_node", configManager->getNodeName().c_str());
  TEST_ASSERT_TRUE(configManager->getLcdDebug());
}

void test_parse_invalid_config(void) {
  String json = "{invalid_json}";
  TEST_ASSERT_FALSE(configManager->parseConfig(json));
}

void test_parse_valid_layout(void) {
  // Format: faceID, stripID, stripIndex, x, y, z
  String csv = "0,0,0,1.0,0.0,0.0\n0,0,1,0.0,1.0,0.0";

  TEST_ASSERT_TRUE(configManager->parseLayout(csv));
  const auto &layout = configManager->getLayout();
  TEST_ASSERT_EQUAL(2, layout.size());

  TEST_ASSERT_FLOAT_WITHIN(0.001, 1.0, layout[0].x);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, layout[0].y);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, layout[0].z);

  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, layout[1].x);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 1.0, layout[1].y);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, layout[1].z);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_config_defaults);
  RUN_TEST(test_parse_valid_config);
  RUN_TEST(test_parse_invalid_config);
  RUN_TEST(test_parse_valid_layout);
  UNITY_END();
  return 0;
}

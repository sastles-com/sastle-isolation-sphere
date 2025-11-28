#include <Arduino.h>
#include <LittleFS.h>
#include <unity.h>

void setUp(void) {
  // set stuff up here
}

void tearDown(void) {
  // clean stuff up here
}

void test_littlefs_mount(void) {
  TEST_ASSERT_TRUE_MESSAGE(LittleFS.begin(false), "LittleFS mount failed");
}

void test_config_file_exists(void) {
  TEST_ASSERT_TRUE_MESSAGE(LittleFS.exists("/config.json"),
                           "config.json not found");
}

void test_layout_file_exists(void) {
  TEST_ASSERT_TRUE_MESSAGE(LittleFS.exists("/led_layout.csv"),
                           "led_layout.csv not found");
}

void test_read_config_file(void) {
  File file = LittleFS.open("/config.json", "r");
  TEST_ASSERT_TRUE_MESSAGE(file, "Failed to open config.json");

  String content = file.readString();
  TEST_ASSERT_TRUE_MESSAGE(content.length() > 0, "config.json is empty");
  TEST_ASSERT_TRUE_MESSAGE(content.indexOf("test_ssid") >= 0,
                           "Content mismatch");

  file.close();
}

void test_write_read_file(void) {
  const char *filename = "/test.txt";
  const char *message = "Hello LittleFS";

  // Write
  File file = LittleFS.open(filename, "w");
  TEST_ASSERT_TRUE_MESSAGE(file, "Failed to open test file for writing");
  file.print(message);
  file.close();

  // Read
  file = LittleFS.open(filename, "r");
  TEST_ASSERT_TRUE_MESSAGE(file, "Failed to open test file for reading");
  String readContent = file.readString();
  TEST_ASSERT_EQUAL_STRING(message, readContent.c_str());
  file.close();
}

void setup() {
  delay(2000); // Wait for board to settle
  UNITY_BEGIN();
  RUN_TEST(test_littlefs_mount);
  RUN_TEST(test_config_file_exists);
  RUN_TEST(test_layout_file_exists);
  RUN_TEST(test_read_config_file);
  RUN_TEST(test_write_read_file);
  UNITY_END();
}

void loop() {}

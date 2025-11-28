#include <Arduino.h>
#include <IMU.h>
#include <unity.h>
#include <Wire.h> // Include Wire mock

// Mock Adafruit_BNO055
bool mock_bno_begin_success;
imu::Quaternion mock_bno_quat;

void setUp(void) {
  // set up for each test
  mock_bno_begin_success = true; // Reset mock to success for each test
  mock_bno_quat = imu::Quaternion(1.0, 0.0, 0.0, 0.0); // Reset mock quaternion
}

void tearDown(void) {
  // clean up after each test
}

void test_imu_constructor() {
  IMU imu(10, 11); // Example SDA, SCL pins
  TEST_PASS(); // If no crash, constructor is fine for now
}

void test_imu_begin_success() {
  IMU imu(10, 11);
  mock_bno_begin_success = true; // Simulate successful begin
  TEST_ASSERT_TRUE(imu.begin());
}

void test_imu_begin_failure() {
  IMU imu(10, 11);
  mock_bno_begin_success = false; // Simulate failed begin
  TEST_ASSERT_FALSE(imu.begin());
}

void test_imu_update_and_get_quaternion() {
  IMU imu(10, 11);
  mock_bno_begin_success = true;
  imu.begin(); // Initialize IMU

  // Set mock quaternion for update
  mock_bno_quat = imu::Quaternion(0.5, 0.6, 0.7, 0.8);
  imu.update();

  float w, x, y, z;
  imu.getQuaternion(w, x, y, z);

  TEST_ASSERT_EQUAL_FLOAT(0.5, w);
  TEST_ASSERT_EQUAL_FLOAT(0.6, x);
  TEST_ASSERT_EQUAL_FLOAT(0.7, y);
  TEST_ASSERT_EQUAL_FLOAT(0.8, z);
}

void setup() {
  // NOTE!!! Wait for >2 secs <=5 secs after board reset to run tests
  delay(2000); 

  UNITY_BEGIN();
  RUN_TEST(test_imu_constructor);
  RUN_TEST(test_imu_begin_success);
  RUN_TEST(test_imu_begin_failure);
  RUN_TEST(test_imu_update_and_get_quaternion);
  UNITY_END();
}

void loop() {
  // Not used in native tests
}

#ifndef PIO_UNIT_TESTING_MAIN
int main(int argc, char **argv) {
    setup();
    return 0;
}
#endif
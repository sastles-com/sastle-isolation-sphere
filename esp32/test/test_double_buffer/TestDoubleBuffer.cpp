#include "DoubleBuffer.h"
#include <cstring>
#include <unity.h>
#include <vector>

DoubleBuffer<uint8_t> *buffer;
const size_t BUFFER_SIZE = 1024;

void setUp(void) { buffer = new DoubleBuffer<uint8_t>(BUFFER_SIZE); }

void tearDown(void) { delete buffer; }

void test_initial_state(void) {
  TEST_ASSERT_NOT_NULL(buffer->getReadBuffer());
  TEST_ASSERT_NOT_NULL(buffer->getWriteBuffer());
  TEST_ASSERT_NOT_EQUAL(buffer->getReadBuffer(), buffer->getWriteBuffer());
}

void test_swap_logic(void) {
  uint8_t *writeBuf = buffer->getWriteBuffer();
  uint8_t *readBuf = buffer->getReadBuffer();

  // Write some data
  writeBuf[0] = 0xAA;

  // Swap
  buffer->swap();

  // Now read buffer should be the old write buffer
  TEST_ASSERT_EQUAL(writeBuf, buffer->getReadBuffer());
  // And write buffer should be the old read buffer
  TEST_ASSERT_EQUAL(readBuf, buffer->getWriteBuffer());

  // Verify data
  TEST_ASSERT_EQUAL(0xAA, buffer->getReadBuffer()[0]);
}

void test_resize(void) {
  // Optional: test if buffer can be resized or reallocated
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_initial_state);
  RUN_TEST(test_swap_logic);
  UNITY_END();
  return 0;
}

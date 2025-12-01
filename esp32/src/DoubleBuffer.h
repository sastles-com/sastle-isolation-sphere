#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

template <typename T> class DoubleBuffer {
public:
  DoubleBuffer(size_t size) {
    buffer1.resize(size);
    buffer2.resize(size);
    readBuffer = &buffer1;
    writeBuffer = &buffer2;
  }

  T *getReadBuffer() { return readBuffer->data(); }

  T *getWriteBuffer() { return writeBuffer->data(); }

  void swap() {
    std::vector<T> *temp = readBuffer;
    readBuffer = writeBuffer;
    writeBuffer = temp;
  }

  void resize(size_t size) {
    buffer1.resize(size);
    buffer2.resize(size);
  }

  size_t size() const { return buffer1.size(); }

private:
  std::vector<T> buffer1;
  std::vector<T> buffer2;
  std::vector<T> *readBuffer;
  std::vector<T> *writeBuffer;
};

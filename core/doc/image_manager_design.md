> **English** · [日本語](image_manager_design.ja.md)

# ImageManager Class Design

## Overview
A class that decodes JPEG images received over UDP and provides data for LED rendering via a double buffer.

## Requirements
1. Receive JPEG-compressed images over UDP
2. Non-blocking image updates via double buffering
3. Retrieve RGB values by pixel coordinates
4. Large-capacity buffers leveraging PSRAM
5. Future: support for reading JPEGs from the file system

## Class Structure

```cpp
class ImageManager {
public:
    // Initialization
    bool begin(ConfigManager& config, NetworkManager& network);
    
    // UDP image reception / decode (non-blocking)
    bool receiveAndDecode();
    
    // Buffer swap (on decode completion)
    void swapBuffers();
    
    // Pixel access (from the draw buffer)
    bool getPixel(uint16_t x, uint16_t y, uint8_t& r, uint8_t& g, uint8_t& b);
    uint16_t getPixelRGB565(uint16_t x, uint16_t y);
    
    // Load JPEG from file (future extension)
    bool loadFromFile(const char* path);
    
    // Statistics
    uint32_t getFrameCount();
    uint32_t getDroppedFrames();
    float getFPS();
    
private:
    // Double buffer (PSRAM)
    uint16_t* _bufferA;      // RGB565 buffer A
    uint16_t* _bufferB;      // RGB565 buffer B
    uint16_t* _drawBuffer;   // For drawing (read-only)
    uint16_t* _decodeBuffer; // For decoding (write)
    
    // Image parameters
    uint16_t _width;
    uint16_t _height;
    size_t _bufferSize;
    
    // UDP receive buffer (PSRAM)
    uint8_t* _udpBuffer;
    size_t _udpBufferSize;
    
    // TJpg_Decoder callback
    static bool jpegOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
    
    // Statistics
    uint32_t _frameCount;
    uint32_t _droppedFrames;
    unsigned long _lastFrameTime;
};
```

## Memory Layout

```
PSRAM (8MB)
├─ Buffer A (320x160x2 = 100KB)
├─ Buffer B (320x160x2 = 100KB)
├─ UDP Buffer (max 64KB)
└─ JPEG work area (internal to TJpg_Decoder)
```

## Data Flow

```
UDP Packet (JPEG)
    ↓
UDP Buffer (PSRAM)
    ↓
TJpg_Decoder
    ↓
Decode Buffer (Buffer A or B)
    ↓ swapBuffers()
Draw Buffer (Buffer B or A)
    ↓ getPixel()
LED Mapping
```

## UDP Image Protocol

### Single-Packet Method (recommended)
```
[Header 4 bytes][JPEG Data N bytes]
Header:
  - Magic: 0x4A50 (JP)
  - Width: uint16_t
  - Height: uint16_t (ignored in the implementation, obtained from config.json)
```

### Multi-Packet Method (future extension)
Split large images into multiple packets for transmission
```
[Header][Sequence][Fragment][JPEG Data]
```

## TJpg_Decoder Integration

### platformio.ini
```ini
lib_deps = 
    bodmer/TJpg_Decoder @ ^1.0.8
```

### Initialization
```cpp
TJpgDec.setJpgScale(1);  // 1, 2, 4, 8
TJpgDec.setSwapBytes(true);  // RGB565 byte order
TJpgDec.setCallback(jpegOutput);
```

### Decode
```cpp
TJpgDec.drawJpg(0, 0, _udpBuffer, _udpDataSize);
```

## Performance Targets

- Decode speed: 30fps or more (320x160 JPEG)
- Buffer swap: < 1ms (pointer swap only)
- Pixel access: < 1μs

## Implementation Phases

### Phase 1: Basic Implementation
- Allocate double buffer
- Set up TJpg_Decoder
- Basic flow from UDP reception through decoding
- Implement getPixel()

### Phase 2: Optimization
- Frame drop detection
- FPS measurement
- Error handling

### Phase 3: Extended Features
- Load JPEGs from the file system
- Multi-packet JPEG support
- Image rotation / scaling

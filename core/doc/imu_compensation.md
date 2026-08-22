> **English** · [日本語](imu_compensation.ja.md)

# IMU Orientation Compensation

## Overview

Even when the LED sphere physically rotates, this feature ensures the image is always displayed in the correct orientation by using the IMU quaternion to inverse-rotate each LED's coordinates.

## How It Works

### The Problem

When the sphere rotates, the fixed LED coordinates rotate along with it, causing the image to follow the sphere's rotation.

```
Example: rotating the sphere by 90 degrees
- Physical position of LED #0: (0.261, 0.804, -0.533) → after tilting
- But the image is sent in untilted spatial coordinates
→ Result: the image appears tilted
```

### The Solution: Quaternion Inverse Rotation

1. **Get quaternion from IMU**: the sphere's current orientation `q = (w, x, y, z)`
2. **Compute the conjugate quaternion**: the inverse rotation `q^-1 = (w, -x, -y, -z)`
3. **Inverse-rotate the LED coordinates**: `LED coordinate' = q^-1 * LED coordinate * q`
4. **UV mapping using the inverse-rotated coordinates**: determines which part of the image to display

As a result, no matter how the sphere is tilted, the LED coordinates are "returned to their original orientation," so the correct image is displayed.

## Implementation Details

### LEDManager::rotateByQuaternion()

```cpp
void LEDManager::rotateByQuaternion(float& x, float& y, float& z, 
                                     float qw, float qx, float qy, float qz) {
    // Optimized quaternion vector rotation
    // v' = v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v)
    
    float cross1_x = qy * z - qz * y;
    float cross1_y = qz * x - qx * z;
    float cross1_z = qx * y - qy * x;
    
    cross1_x += qw * x;
    cross1_y += qw * y;
    cross1_z += qw * z;
    
    float cross2_x = qy * cross1_z - qz * cross1_y;
    float cross2_y = qz * cross1_x - qx * cross1_z;
    float cross2_z = qx * cross1_y - qy * cross1_x;
    
    x += 2.0f * cross2_x;
    y += 2.0f * cross2_y;
    z += 2.0f * cross2_z;
}
```

### updateLEDBuffer() Flow

```cpp
void LEDManager::updateLEDBuffer() {
    // 1. Get quaternion from IMU
    float qw, qx, qy, qz;
    if (_imuManager->getQuaternion(qw, qx, qy, qz)) {
        // Compute conjugate (inverse rotation)
        qx = -qx;
        qy = -qy;
        qz = -qz;
    }
    
    for (uint16_t i = 0; i < _numLEDs; i++) {
        // 2. Get LED coordinates
        float x = ledLayout[i].x;
        float y = ledLayout[i].y;
        float z = ledLayout[i].z;
        
        // 3. Inverse-rotation compensation
        rotateByQuaternion(x, y, z, qw, qx, qy, qz);
        
        // 4. UV mapping
        float u, v;
        sphereToUV(x, y, z, u, v);
        
        // 5. Get image pixel
        imageManager->getPixel(u * width, v * height, r, g, b);
        
        // 6. Set the LED
        ledBuffer[i] = CRGB(r, g, b);
    }
}
```

## Enabling / Disabling

### Automatic Enabling

If the IMUManager is initialized, orientation compensation is enabled automatically.

```cpp
// main.cpp
IMUManager* imuPtr = imuSensor.isInitialized() ? &imuSensor : nullptr;
ledManager.begin(config, imageManager, imuPtr);
```

### Manual Control

```cpp
// Disable orientation compensation (for debugging)
ledManager.setIMUCompensation(false);

// Enable it again
ledManager.setIMUCompensation(true);
```

## Performance

### Processing Time

| Process | Time (estimated) | Before optimization | After optimization |
|------|-------------|----------|----------|
| Quaternion retrieval | ~0.1 ms | 0.1 ms | 0.1 ms |
| Conjugate computation | < 0.01 ms | < 0.01 ms | < 0.01 ms |
| Inverse rotation × 800 | ~8-12 ms | 10-15 ms | 8-12 ms |
| UV mapping × 800 | ~10-15 ms | 15-20 ms | 10-12 ms |
| **Total** | **~18-27 ms** | **25-35 ms** | **18-25 ms** |

### Optimizations

#### 1. Use of Fast Approximation Functions in common.h

**_sqrt() - Fast square root**
- Before: `sqrt()` from the standard library
- Optimized: `_sqrt()` approximation using a 4th-order convergence method
- **Effect**: about 2-3x faster

**_atan2() - Fast arctangent**
- Before: `atan2f()` from the standard library  
- Optimized: `_atan2()` 4th-order polynomial approximation
- **Effect**: about 3-5x faster

```cpp
// Before optimization (standard library)
float len = sqrt(x*x + y*y + z*z);
u = 0.5f + atan2f(nx, ny) / (2.0f * PI);              // longitude -> width
v = atan2f(sqrt(nx*nx + ny*ny), nz) / PI;             // polar angle (from +Z) -> height

// After optimization (FastMath.h; _atan2 returns "degrees/180", so no /PI is needed)
float len = _sqrt(x*x + y*y + z*z);
u = (_atan2(nx, ny) + 1.0f) / 2.0f;                   // longitude -180..180 -> [0,1]
v = _atan2(_sqrt(nx*nx + ny*ny), nz);                 // polar angle 0..180 -> [0,1]
```

> Standard Z-polar equirectangular. For `v`, the first argument is ≥0 so `_atan2 ∈ [0,1]` already —
> do **not** apply `(+1)/2`. [`LEDManager::sphereToUV()`](../src/LEDManager.cpp) is canonical, and the
> WebUI twin `server/frontend/src/components/sphere/HoloSphere.jsx` uses the identical formulas and
> the identical quantization.

#### 2. Quaternion Rotation Optimization

The previous implementation was already optimized:
- Cross-product-based computation: 12 multiply-accumulate operations
- Avoids matrix computation: reduced memory access

For 800 LEDs, the processing time is about 18-25 ms per frame.
This leaves **ample headroom** relative to the target of 30 fps (33 ms/frame).

## Coordinate Systems

### BNO055 IMU Coordinate System

```
      +Z (zenith)
       |
       |
       +------ +X (forward)
      /
     /
   +Y (right)
```

### LED Sphere Coordinate System

This assumes that the coordinate system defined in `led_layouts-5strip.csv` matches the IMU coordinate system.

If the coordinate systems differ, a coordinate transformation is required before `rotateByQuaternion()`.

## Debugging

### Example Serial Output

```
[LEDManager] Initializing...
[LEDManager] IMU compensation enabled
[LEDManager] Loaded 800 LED coordinates
[LEDManager] LEDs per strip: [200, 200, 200, 200]
[LEDManager] Initialization complete

[LED_Render] Task started on core 1
```

### How to Verify Orientation Compensation

1. **Stationary**: keep the sphere still → the image displays normally
2. **Rotating**: tilt the sphere → the image always stays upright
3. **Compensation disabled**: `setIMUCompensation(false)` → the image rotates together with the sphere

## Limitations

1. **IMU update rate**: 100Hz (10ms interval)
   - Faster than the 30fps LED update, so precision is sufficient

2. **Quaternion normalization**:
   - The BNO055 outputs already-normalized quaternions
   - Normalization is required when using other IMUs

3. **Coordinate system alignment**:
   - Assumes the LED coordinate system and the IMU coordinate system are aligned
   - Calibration may be required

## Future Improvements

- [ ] **Calibration feature**: coordinate-system offset compensation
- [ ] **Smoothing**: quaternion interpolation for smooth motion
- [x] **Fast approximation functions**: 3-5x speedup by using `_sqrt()` and `_atan2()` from common.h
- [ ] **Lookup table**: further speedup (trade-off against accuracy)
- [ ] **Coordinate transformation UI**: support for different coordinate systems

## References

- [Building the Isolation sphere - elchika](https://elchika.com/article/9b97a7b7-561f-4795-9e16-3e011c045913/)
  - Implementation of virtual LED rotation using the IMU
  - Principles of orientation compensation using quaternions
- [common.h fast approximation functions](../src/common.h)
  - `_sqrt()`: square-root approximation using a 4th-order convergence method
  - `_atan2()`: arctangent approximation using a 4th-order polynomial

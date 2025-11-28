#pragma once

// This file defines the hardware configuration for the M5Atom S3R.
// It is assumed that the M5Unified library will be used for this board.

#include <M5Unified.h>

// --- Display ---
#define BOARD_HAS_LCD 1

// --- I2C (for IMU, etc.) ---
// On the M5Atom S3R, SDA/SCL are internally wired and initialized by M5.begin().
// Explicit pin numbers are generally not needed for components on the internal I2C bus.
// Referring to M5Unified's definitions (G2, G1) if needed.
#define I2C_SDA_PIN 2
#define I2C_SCL_PIN 1

// --- Speaker ---
// The speaker is also initialized by M5.begin().
#define SPEAKER_PIN 12

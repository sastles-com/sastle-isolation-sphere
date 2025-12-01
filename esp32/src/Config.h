#pragma once

// WiFi Settings
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"
#define SERVER_HOST "192.168.1.100"
#define SERVER_PORT 8000
#define SERVER_PATH "/ws"

// Hardware Pins for M5AtomS3
#define PIN_SPEAKER 39 // G39
#define PIN_IMU_SDA 2  // G2 (Grove SDA)
#define PIN_IMU_SCL 1  // G1 (Grove SCL)

// LED Pins (M5AtomS3 Bottom Header)
#define PIN_LED_1 5 // G5
#define PIN_LED_2 6 // G6
#define PIN_LED_3 7 // G7
#define PIN_LED_4 8 // G8

#define NUM_LEDS_PER_STRIP 10 // Example value, adjust as needed

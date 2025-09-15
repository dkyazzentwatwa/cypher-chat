#pragma once

#include <NimBLEDevice.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// -- ROLE SELECTION --
// Uncomment to compile as SERVER
// #define ROLE_SERVER

#define SERVER_NAME "BLEEP-SERVER"
#ifndef ROLE_SERVER
  #define UNIT_NAME "ALPHA"
#endif

// -- HARDWARE PINS & SETTINGS --
#define I2C_SDA_PIN   21
#define I2C_SCL_PIN   22
#define OLED_ADDR     0x3C
#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_RESET_PIN -1

// Button pins (active-low)
extern const int BUTTON_PINS[];
extern const int NUM_BUTTONS;

// Buzzer pin (-1 to disable)
#define BUZZER_PIN    27

// Button labels
extern const char* BUTTON_LABELS[];

// BLE settings
#define BLE_TX_POWER  ESP_PWR_LVL_P9

// Scan retry & sleep settings
#define MAX_SCAN_RETRIES 5
#define SLEEP_DURATION_USEC (5ULL * 60ULL * 1000000ULL) // 5 minutes

// BLE UUIDs
#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define RX_CHARACTERISTIC_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define TX_CHARACTERISTIC_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// Security
#define PASSKEY 123456

// Display object
extern Adafruit_SSD1306 display;

// Button debounce
extern unsigned long lastButtonPressMillis;
extern const int DEBOUNCE_DELAY;

// Shared utilities
void beep();
void oledPrint(const char* line1, const char* line2 = "");


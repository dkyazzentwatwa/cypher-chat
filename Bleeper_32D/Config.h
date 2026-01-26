#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// -- RUNTIME CONFIGURATION --
#define DEFAULT_UNIT_NAME "CYPHER"
#define DEFAULT_PASSKEY 123456

extern String unitName;
extern uint32_t currentPasskey;
extern bool isServer;

// -- SECURITY & VALIDATION CONSTANTS --
#define MAX_MESSAGE_SIZE 128
#define MIN_MESSAGE_SIZE 5
#define MAX_UNIT_NAME_LEN 16
#define MIN_PASSKEY 100000
#define MAX_PASSKEY 999999
#define PASSKEY_DIGITS 6
#define HMAC_SIZE 8  // Use first 8 bytes of HMAC-SHA256
#define HMAC_HEX_SIZE (HMAC_SIZE * 2)  // 16 hex characters

// -- CONNECTION PARAMETERS --
#define SCAN_TIMEOUT_MS 10000
#define CONNECT_TIMEOUT_MS 15000
#define BLEEPER_MAX_CONN_FAILURES 5

// -- HARDWARE PINS & SETTINGS --
#define I2C_SDA_PIN   21
#define I2C_SCL_PIN   22
#define OLED_ADDR     0x3C
#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_RESET_PIN -1

// Button pins (active-low)
#define NUM_BUTTONS 4
extern const int BUTTON_PINS[];

// Buzzer pin (-1 to disable)
#define BUZZER_PIN    -1

// RGB LED pins (-1 to disable)
#define LED_PIN_R     25
#define LED_PIN_G     26
#define LED_PIN_B     27
#define LED_ENABLED   true
#define LED_PWM_FREQ  5000
#define LED_PWM_RES   8  // 8-bit resolution (0-255)

// Terminal interface configuration
#define TERMINAL_ENABLED true
#define TERMINAL_DEFAULT_MODE TERM_NORMAL
#define TERMINAL_UPDATE_INTERVAL_MS 1000
#define TERMINAL_BANNER_ENABLED true

// Button labels
extern const char* BUTTON_LABELS[];

// BLE settings
#define BLE_TX_POWER  ESP_PWR_LVL_P9

// BLE UUIDs
#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define RX_CHARACTERISTIC_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define TX_CHARACTERISTIC_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// -- MESH NETWORKING (ESP-NOW) --
#define MESH_ENABLED true         // Enable mesh networking layer
#define MESH_CHANNEL 1            // WiFi channel for ESP-NOW (1-14)
#define MESH_MAX_PEERS 20         // Maximum tracked mesh peers
#define MESH_DEFAULT_TTL 3        // Default hops for messages
#define MESH_MAX_TTL 5            // Maximum hops before discard
#define MESH_HEARTBEAT_MS 15000   // Peer discovery interval
#define MESH_PEER_TIMEOUT_MS 60000 // Peer offline threshold
#define MESH_STORE_QUEUE_SIZE 32  // Store-and-forward queue
#define MESH_MSG_EXPIRE_MS 300000 // Message expiry (5 minutes)

// GPS Support (optional TinyGPS++ compatible module)
#define GPS_ENABLED false         // Set true if GPS module connected
#define GPS_RX_PIN 16             // GPS module TX -> ESP32 RX
#define GPS_TX_PIN 17             // GPS module RX -> ESP32 TX (if needed)
#define GPS_BAUD 9600             // GPS module baud rate

// Message history
void addToHistory(const char* msg);

// Display object
extern Adafruit_SSD1306 display;

// Button debounce
extern unsigned long lastButtonPressMillis;
extern const int DEBOUNCE_DELAY;

// Shared utilities
void beep();
void oledPrint(const char* line1, const char* line2 = "");


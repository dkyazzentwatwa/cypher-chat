#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/**
 * CYPHER-CHAT - ESP32 DevKit C V4 FULL CONFIGURATION
 * Optimized for multi-module RF gateway (NRF24L01 + CC1101)
 */

// -- PROJECT IDENTIFICATION --
#define PROJECT_NAME "CYPHER-CHAT"
#define DEFAULT_UNIT_NAME "CYPHER_NODE"
#define DEFAULT_PASSPHRASE "123456"

// -- RUNTIME CONFIGURATION --
extern String unitName;
extern char currentPassphrase[65];
extern bool isServer;

// -- SECURITY & VALIDATION CONSTANTS --
#define MAX_MESSAGE_SIZE 128
#define MIN_MESSAGE_SIZE 5
#define MAX_UNIT_NAME_LEN 16
#define MAX_PASSPHRASE_LEN 64
#define MIN_PASSPHRASE_LEN 4

// -- MESH CRYPTO PROTOCOL --
#define MESH_PROTOCOL_VERSION 0x02

// -- NRF24L01+PA+LNA Modules --
#define NRF_U2_MISO 19
#define NRF_U2_MOSI 18
#define NRF_U2_SCK 23
#define NRF_U2_CSN 33
#define NRF_U2_CE 26

#define NRF_U3_MISO 19
#define NRF_U3_MOSI 18
#define NRF_U3_SCK 23
#define NRF_U3_CSN 5
#define NRF_U3_CE 25

#define NRF_U4_MISO 19
#define NRF_U4_MOSI 18
#define NRF_U4_SCK 23
#define NRF_U4_CSN 5
#define NRF_U4_CE 25

// -- CC1101 Modules --
#define CC_MISO_GDO1 14
#define CC_MOSI 13
#define CC_SCK 12
#define CC_CSN 17
#define CC_GDO0 32
#define CC_GDO2 35

// -- DISPLAY (I2C) --
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDR 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET_PIN -1
#define OLED_ENABLED true

// -- USER INTERFACE (Buttons) --
#define NUM_BUTTONS 3
#define KEY1_PIN 39
#define KEY2_PIN 34
#define KEY3_PIN 36
extern const int BUTTON_PINS[];
extern const char* BUTTON_LABELS[];

// -- INDICATORS (Discrete LEDs) --
#define LED1_PIN 0
#define LED2_PIN -1 
#define LED3_PIN 3  // Status/RX
#define LED4_PIN 1  // Alarm/TX
#define LED_ENABLED true

// Pin mapping for LEDManager
#define LED_IDLE_PIN    LED1_PIN
#define LED_STATUS_PIN  LED3_PIN
#define LED_ALARM_PIN   LED4_PIN

// RGB LED pins (for LEDManager color control)
#define LED_PIN_R       LED1_PIN  // Red channel
#define LED_PIN_G       LED3_PIN  // Green channel
#define LED_PIN_B       LED4_PIN  // Blue channel

// LED PWM configuration
#define LED_PWM_FREQ    5000      // 5kHz PWM frequency
#define LED_PWM_RES     8         // 8-bit resolution (0-255)

// -- BUZZER --
#define BUZZER_PIN      -1        // Buzzer disabled (-1 = no buzzer)

// -- SERIAL COMMUNICATION --
#define UART_RXD 3
#define UART_TXD 1
#define TERMINAL_BAUD 115200
#define TERMINAL_ENABLED true
#define TERMINAL_DEFAULT_MODE TERM_NORMAL
#define TERMINAL_UPDATE_INTERVAL_MS 1000
#define TERMINAL_BANNER_ENABLED true

// -- BLE UART (Nordic UART Service) --
#define BLE_UART_ENABLED true         // Enable BLE UART for wireless terminal access (iPhone compatible)
                                      // Uses Nordic UART Service - works with nRF Connect, Bluefruit apps

// -- MESH NETWORKING (ESP-NOW) --
#define MESH_ENABLED true         
#define MESH_CHANNEL 1            
#define MESH_MAX_PEERS 20         
#define MESH_DEFAULT_TTL 3        
#define MESH_MAX_TTL 5            
#define MESH_HEARTBEAT_MS 15000   
#define MESH_PEER_TIMEOUT_MS 60000 
#define MESH_STORE_QUEUE_SIZE 32  
#define MESH_MSG_EXPIRE_MS 300000 

// -- BLE SETTINGS --
#define BLE_ENABLED false 

// -- GPS Support (Optional) --
#define GPS_ENABLED false
#define GPS_RX_PIN 16            // GPS TX -> ESP32 RX
#define GPS_TX_PIN 17            // GPS RX -> ESP32 TX
#define GPS_BAUD 9600            // Standard GPS baud rate

// -- BATTERY MONITORING --
#define BATTERY_ENABLED true
#define BATTERY_ADC_PIN 35       // ADC1_CH7 - voltage divider input
#define BATTERY_ADC_SAMPLES 10   // Number of samples to average
#define BATTERY_MIN_VOLTAGE 3.0  // Minimum battery voltage (empty)
#define BATTERY_MAX_VOLTAGE 4.2  // Maximum battery voltage (full)
#define BATTERY_DIVIDER_RATIO 2.0 // Voltage divider ratio (R1=R2=100K)

// -- FILE SYSTEM (SPIFFS/LittleFS) --
#define FILESYSTEM_ENABLED true
#define FILESYSTEM_SIZE_MB 1.5   // Partition size
#define FILESYSTEM_MAX_FILES 50  // Maximum file count

// -- TIME MANAGEMENT --
#define TIME_ENABLED true
#define TIME_DEFAULT_TZ 0        // UTC offset in hours (-12 to +14)
#define TIME_AUTO_SYNC true      // Auto-sync from GPS if available

// -- LOGGING --
#define LOG_ENABLED true
#define LOG_BUFFER_SIZE 200      // Number of log entries to keep in RAM
#define LOG_DEFAULT_LEVEL 2      // 0=NONE, 1=ERROR, 2=INFO, 3=WARN, 4=DEBUG
#define LOG_TO_SERIAL true       // Echo logs to serial
#define LOG_TO_FILE false        // Save logs to filesystem

// -- SECURITY & ACCESS CONTROL --
#define SECURITY_BLOCKLIST_SIZE 20    // Maximum blocked peers
#define SECURITY_TRUSTLIST_SIZE 50    // Maximum trusted peers
#define SECURITY_PERSIST_NVS true     // Save to NVS

// -- POWER MANAGEMENT --
#define POWER_MANAGEMENT_ENABLED true
#define POWER_TX_POWER_DEFAULT 20     // WiFi TX power in dBm (0-20)
#define POWER_SLEEP_WAKEUP_PIN KEY1_PIN // Button to wake from deep sleep

// -- SHARED UTILITIES & OBJECTS --
void addToHistory(const char* msg);
void oledPrint(const char* line1, const char* line2 = "");
void beep();

extern Adafruit_SSD1306 display;
extern unsigned long lastButtonPressMillis;
extern const int DEBOUNCE_DELAY;
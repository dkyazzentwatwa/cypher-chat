#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/**
 * STARBEAM - ESP32 DevKit C V4 FULL CONFIGURATION
 * Optimized for multi-module RF gateway (NRF24L01 + CC1101)
 */

// -- PROJECT IDENTIFICATION --
#define PROJECT_NAME "STARBEAM"
#define DEFAULT_UNIT_NAME "STARBEAM_NODE"
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

// -- SHARED UTILITIES & OBJECTS --
void addToHistory(const char* msg);
void oledPrint(const char* line1, const char* line2 = "");
void beep();

extern Adafruit_SSD1306 display;
extern unsigned long lastButtonPressMillis;
extern const int DEBOUNCE_DELAY;
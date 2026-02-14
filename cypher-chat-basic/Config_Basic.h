#pragma once

#include <Arduino.h>

/**
 * CYPHER-CHAT BASIC - Minimal ESP32 Configuration
 *
 * Designed for bare ESP32 boards ($3-5) with no additional hardware.
 * Connects via USB Serial or Bluetooth Serial (BLE UART).
 * Participates in ESP-NOW mesh network.
 *
 * No buttons, LEDs, display, or buzzer required.
 */

// -- PROJECT IDENTIFICATION --
#define PROJECT_NAME "CYPHER-CHAT_BASIC"
#define DEFAULT_UNIT_NAME "BASIC_NODE"
#define DEFAULT_PASSKEY 123456

// -- RUNTIME CONFIGURATION --
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
#define HMAC_SIZE 8
#define HMAC_HEX_SIZE (HMAC_SIZE * 2)

// -- SERIAL COMMUNICATION --
#define UART_RXD 3
#define UART_TXD 1
#define TERMINAL_BAUD 115200
#define TERMINAL_ENABLED true
#define TERMINAL_DEFAULT_MODE TERM_NORMAL
#define TERMINAL_UPDATE_INTERVAL_MS 1000
#define TERMINAL_BANNER_ENABLED true

// -- BLE UART (Nordic UART Service) --
#define BLE_UART_ENABLED true         // Enable BLE UART for wireless terminal access

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

// -- DISABLED FEATURES --
#define BLE_ENABLED false         // Old NimBLE server/client (not used)
#define GPS_ENABLED false         // No GPS module
#define GPS_RX_PIN -1
#define GPS_TX_PIN -1
#define GPS_BAUD 9600

// -- HARDWARE DISABLED (Barebones version) --
// No display
#define OLED_ENABLED false

// No buttons
#define NUM_BUTTONS 0
#define BUTTONS_ENABLED false

// No LEDs
#define LED_ENABLED false
#define LED_PIN_R -1
#define LED_PIN_G -1
#define LED_PIN_B -1

// No buzzer
#define BUZZER_PIN -1

// -- BUTTON LABELS (for terminal send command compatibility) --
// These are the message types that can be sent via "send 1-4" command
extern const char* BUTTON_LABELS[];

// -- STUB FUNCTIONS (for code compatibility) --
void addToHistory(const char* msg);
void oledPrint(const char* line1, const char* line2 = "");
void beep();

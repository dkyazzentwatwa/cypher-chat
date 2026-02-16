#pragma once

#include <Arduino.h>

/**
 * CYPHER-CHAT 8266 - ESP8266 Serial Terminal Configuration
 *
 * Designed for cheap ESP8266 boards ($2-4) with no additional hardware.
 * Serial terminal only - no BLE, no display, no buttons.
 * Participates in ESP-NOW mesh network alongside ESP32 nodes.
 *
 * Compatible with ESP32 cypher-chat nodes using same passphrase.
 */

// -- PROJECT IDENTIFICATION --
#define PROJECT_NAME "CYPHER-CHAT_8266"
#define DEFAULT_UNIT_NAME "8266_NODE"
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
// Must match ESP32 version for cross-compatibility
#define MESH_PROTOCOL_VERSION 0x02

// -- SERIAL COMMUNICATION --
#define TERMINAL_BAUD 115200
#define TERMINAL_ENABLED true
#define TERMINAL_DEFAULT_MODE TERM_NORMAL
#define TERMINAL_UPDATE_INTERVAL_MS 1000
#define TERMINAL_BANNER_ENABLED true

// -- BLE UART (NOT AVAILABLE ON ESP8266) --
#define BLE_UART_ENABLED false

// -- MESH NETWORKING (ESP-NOW) --
#define MESH_ENABLED true
#define MESH_CHANNEL 1
#define MESH_MAX_PEERS 10           // Reduced from 20 for ESP8266 RAM
#define MESH_DEFAULT_TTL 3
#define MESH_MAX_TTL 5
#define MESH_HEARTBEAT_MS 15000
#define MESH_PEER_TIMEOUT_MS 60000
#define MESH_STORE_QUEUE_SIZE 8     // Reduced from 32 for ESP8266 RAM
#define MESH_MSG_EXPIRE_MS 300000

// -- DISABLED FEATURES --
#define BLE_ENABLED false
#define GPS_ENABLED false
#define GPS_RX_PIN -1
#define GPS_TX_PIN -1
#define GPS_BAUD 9600

// -- HARDWARE DISABLED (Serial terminal only) --
#define OLED_ENABLED false
#define NUM_BUTTONS 0
#define BUTTONS_ENABLED false
#define LED_ENABLED false
#define LED_PIN_R -1
#define LED_PIN_G -1
#define LED_PIN_B -1
#define BUZZER_PIN -1

// -- ESP8266 MEMORY TUNING --
// ESP8266 has ~80KB usable RAM vs ESP32's 520KB
#define MESH_MSG_CACHE_SIZE 32      // Reduced from 64
#define TERM_HISTORY_SIZE 5         // Reduced from 10
#define MESH_REPLAY_MAX_PEERS 10    // Reduced from 20

// -- EEPROM LAYOUT (persistent storage) --
// ESP8266 uses EEPROM instead of ESP32's NVS (Preferences)
#define EEPROM_SIZE 512
#define EEPROM_MAGIC 0xCC
#define EEPROM_ADDR_MAGIC 0x000
#define EEPROM_ADDR_PP_LEN 0x001
#define EEPROM_ADDR_PP_DATA 0x002    // 65 bytes (passphrase + null)
#define EEPROM_ADDR_RC_COUNT 0x043
#define EEPROM_ADDR_RC_DATA 0x044    // 10 entries * 10 bytes = 100 bytes
#define EEPROM_ADDR_TERM_MODE 0x0AC
#define EEPROM_ADDR_ANSI 0x0AD
#define EEPROM_ADDR_MENU 0x0AE

// -- BUTTON LABELS (for terminal send command compatibility) --
extern const char* BUTTON_LABELS[];

// -- STUB FUNCTIONS (for code compatibility) --
void addToHistory(const char* msg);
void oledPrint(const char* line1, const char* line2 = "");
void beep();

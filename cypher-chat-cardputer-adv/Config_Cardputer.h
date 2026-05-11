#pragma once

#include <Arduino.h>

/*
 * CYPHER CHAT - M5Stack Cardputer-Adv configuration
 *
 * This target uses the built-in ESP32-S3, ST7789 display, keyboard, speaker,
 * battery monitor, USB serial, BLE UART, and ESP-NOW mesh radio.
 */

// -- PROJECT IDENTIFICATION --
#define PROJECT_NAME "CYPHER_CARDPUTER_ADV"
#define DEFAULT_UNIT_NAME "CARDPUTER_NODE"
#define DEFAULT_PASSKEY 123456
#define FIRMWARE_VERSION "0.2.0-field-console"

// -- RUNTIME CONFIGURATION --
extern String unitName;
extern uint32_t currentPasskey;
extern bool isServer;

// -- SECURITY & VALIDATION CONSTANTS --
#define MAX_MESSAGE_SIZE 128
#define MIN_MESSAGE_SIZE 1
#define MAX_UNIT_NAME_LEN 16
#define MIN_PASSKEY 100000
#define MAX_PASSKEY 999999
#define PASSKEY_DIGITS 6
#define HMAC_SIZE 8
#define HMAC_HEX_SIZE (HMAC_SIZE * 2)

// -- SERIAL COMMUNICATION --
#define TERMINAL_BAUD 115200
#define TERMINAL_ENABLED true
#define TERMINAL_DEFAULT_MODE TERM_NORMAL
#define TERMINAL_UPDATE_INTERVAL_MS 1000
#define TERMINAL_BANNER_ENABLED true

// -- BLE UART (Nordic UART Service) --
#define BLE_UART_ENABLED true

// -- ESP-NOW MESH --
#define MESH_ENABLED true
#define MESH_CHANNEL 1
#define MESH_MAX_PEERS 20
#define MESH_DEFAULT_TTL 3
#define MESH_MAX_TTL 5
#define MESH_HEARTBEAT_MS 15000
#define MESH_PEER_TIMEOUT_MS 60000
#define MESH_STORE_QUEUE_SIZE 32
#define MESH_MSG_EXPIRE_MS 300000

// -- LEGACY BLE CLIENT/SERVER AND GPS --
#define BLE_ENABLED false
#define GPS_ENABLED false
#define GPS_RX_PIN -1
#define GPS_TX_PIN -1
#define GPS_BAUD 9600

// -- CARDPUTER UI --
#define CARDPUTER_SPEAKER_ENABLED true
#define CARDPUTER_HISTORY_SIZE 16
#define FIELD_MESSAGE_STORE_SIZE 32
#define FIELD_PEER_DIRECTORY_SIZE 12
#define CARDPUTER_INPUT_MAX 96

// Terminal "send" compatibility. These are quick messages, not GPIO buttons.
#define NUM_BUTTONS 4
extern const char* BUTTON_LABELS[];

// Shared hooks expected by copied core modules.
void addToHistory(const char* msg);
void oledPrint(const char* line1, const char* line2 = "");
void beep();

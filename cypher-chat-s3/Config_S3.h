#pragma once

#include <Arduino.h>
#include "BoardConfig.h"

#define PROJECT_NAME "CYPHER_CHAT_S3"
#define DEFAULT_UNIT_NAME "CYPHER_S3"
#define DEFAULT_PASSKEY 123456
#define FIRMWARE_VERSION "0.2.0-s3-profiles"

extern String unitName;
extern uint32_t currentPasskey;
extern bool isServer;

#define MAX_MESSAGE_SIZE 128
#define MIN_MESSAGE_SIZE 1
#define MAX_UNIT_NAME_LEN 16
#define MIN_PASSKEY 100000
#define MAX_PASSKEY 999999
#define PASSKEY_DIGITS 6
#define HMAC_SIZE 8
#define HMAC_HEX_SIZE (HMAC_SIZE * 2)

#define TERMINAL_BAUD SERIAL_BAUD
#define TERMINAL_ENABLED true
#define TERMINAL_DEFAULT_MODE TERM_NORMAL
#define TERMINAL_UPDATE_INTERVAL_MS 1000
#define TERMINAL_BANNER_ENABLED true

#define BLE_UART_ENABLED true

#define MESH_ENABLED true
#define MESH_CHANNEL 1
#define MESH_MAX_PEERS 20
#define MESH_DEFAULT_TTL 3
#define MESH_MAX_TTL 5
#define MESH_HEARTBEAT_MS 15000
#define MESH_PEER_TIMEOUT_MS 60000
#define MESH_STORE_QUEUE_SIZE 32
#define MESH_MSG_EXPIRE_MS 300000

#define BLE_ENABLED false
#define GPS_ENABLED false
#define GPS_RX_PIN -1
#define GPS_TX_PIN -1
#define GPS_BAUD 9600

#define CARDPUTER_SPEAKER_ENABLED BOARD_HAS_M5_POWER
#define CARDPUTER_HISTORY_SIZE 12
#define CARDPUTER_INPUT_MAX 96

#define NUM_BUTTONS 4
extern const char* BUTTON_LABELS[];

void addToHistory(const char* msg);
void oledPrint(const char* line1, const char* line2 = "");
void beep();


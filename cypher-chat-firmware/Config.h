#pragma once

#include <Arduino.h>
#include "BoardProfiles.h"

#define PROJECT_NAME "CYPHER_CHAT_FIRMWARE"
#define DEFAULT_UNIT_NAME DEFAULT_PROFILE_UNIT_NAME
#define DEFAULT_PASSPHRASE "01234567"
#define FIRMWARE_VERSION "0.4.0-secure-mesh"

extern String unitName;
extern char currentPassphrase[];
extern bool isServer;

#define MAX_MESSAGE_SIZE 128
#define MIN_MESSAGE_SIZE 1
#define MAX_UNIT_NAME_LEN 16
#define MIN_PASSPHRASE_LEN 8
#define MAX_PASSPHRASE_LEN 64

#define MESH_PROTOCOL_VERSION 0x02

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
#define GPS_ENABLED BOARD_HAS_GPS
#if !GPS_ENABLED
constexpr int GPS_RX_PIN = -1;
constexpr int GPS_TX_PIN = -1;
constexpr uint32_t GPS_BAUD = 9600;
#endif

#define CARDPUTER_SPEAKER_ENABLED BOARD_HAS_M5_POWER
#define CARDPUTER_HISTORY_SIZE 12
#define CARDPUTER_INPUT_MAX 96

#define NUM_BUTTONS 4
extern const char* BUTTON_LABELS[];

void addToHistory(const char* msg);
void oledPrint(const char* line1, const char* line2 = "");
void beep();

/**
 * CYPHER-CHAT 8266 - ESP8266 Serial Terminal for Mesh Network
 *
 * A serial-terminal-only port of Cypher-Chat for ESP8266 boards.
 * Communicates with ESP32 cypher-chat nodes via ESP-NOW mesh.
 *
 * Perfect for:
 * - Cheap $2-4 ESP8266 boards (NodeMCU, Wemos D1 Mini, etc.)
 * - Adding more nodes to an existing ESP32 mesh network
 * - Terminal-only operation via USB serial
 *
 * Features:
 * - USB Serial terminal (115200 baud)
 * - ESP-NOW mesh networking (~250m range, multi-hop relay)
 * - AES-256-GCM AEAD encryption (BearSSL) + HKDF-SHA256 key derivation
 * - Wire-compatible with ESP32 cypher-chat nodes
 *
 * What's different from ESP32 version:
 * - No Bluetooth (ESP8266 has no BLE)
 * - No display, buttons, LEDs, buzzer, GPS
 * - BearSSL crypto instead of mbedTLS (same AES-256-GCM output)
 * - EEPROM storage instead of NVS/Preferences
 * - Reduced buffer sizes for ~80KB RAM
 * - Simplified single-threaded state management (no FreeRTOS)
 *
 * Build with ESP8266 Arduino core:
 *   Board: NodeMCU 1.0 (or Generic ESP8266 Module)
 *   Flash: 4MB (FS:1MB OTA:~1019KB)
 *   CPU: 80MHz or 160MHz
 */

#include "Config_8266.h"
#include "MeshCrypto.h"
#include "OutputManager.h"
#include "TerminalManager.h"
#include "MeshManager.h"
#include "StateManager.h"

// Global runtime state
bool isServer = false;
String unitName = DEFAULT_UNIT_NAME;
char currentPassphrase[65] = DEFAULT_PASSPHRASE;
String messageHistory[10];
int historyCount = 0;

// Button labels for terminal "send" command compatibility
const char* BUTTON_LABELS[] = { "ACK", "ENROUTE", "HELP", "OK" };

// Global connection state
StateManager connectionState;

// Emergency broadcast state
bool emergencyActive = false;
unsigned long emergencyStartTime = 0;
const unsigned long EMERGENCY_DURATION = 30000;

// ============================================================================
// Mesh networking callbacks
// ============================================================================

#if MESH_ENABLED
void onMeshMessage(const MeshPacket* packet, const uint8_t* senderMac, int8_t rssi) {
  char msg[128];
  size_t len = (packet->header.payloadLen < sizeof(msg) - 1) ?
               packet->header.payloadLen : sizeof(msg) - 1;
  memcpy(msg, packet->payload, len);
  msg[len] = '\0';

  char senderStr[32];
  snprintf(senderStr, sizeof(senderStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           packet->header.originMac[0], packet->header.originMac[1],
           packet->header.originMac[2], packet->header.originMac[3],
           packet->header.originMac[4], packet->header.originMac[5]);

  if (packet->header.type == MESH_MSG_EMERGENCY) {
    uint8_t flags = packet->payload[0];
    size_t offset = 1;
    GPSCoordinates gps = {0, 0, 0, false};

    if (flags & 0x01 && packet->header.payloadLen >= 13) {
      memcpy(&gps.latitude, &packet->payload[offset], 4);
      offset += 4;
      memcpy(&gps.longitude, &packet->payload[offset], 4);
      offset += 4;
      memcpy(&gps.altitude, &packet->payload[offset], 4);
      offset += 4;
      gps.valid = true;
    }

    size_t msgLen = packet->header.payloadLen - offset;
    if (msgLen > sizeof(msg) - 1) msgLen = sizeof(msg) - 1;
    memcpy(msg, &packet->payload[offset], msgLen);
    msg[msgLen] = '\0';

    output.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    output.printf("EMERGENCY from %s\n", senderStr);
    output.printf("   Message: %s\n", msg);
    if (gps.valid) {
      output.printf("   GPS: %.6f, %.6f (%.1fm)\n", gps.latitude, gps.longitude, gps.altitude);
    }
    output.printf("   Signal: %d dBm | Hops: %d\n", rssi, packet->header.hopCount);
    output.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    addToHistory(msg);
  } else {
    output.printf("Mesh RX: %s\n", msg);
    addToHistory(msg);
  }
}

void onMeshPeerUpdate(const MeshPeerInfo* peer, bool isNew) {
  if (isNew) {
    if (strlen(peer->unitName) > 0) {
      output.printf("+ Mesh peer: %s (RSSI: %d dBm)\n", peer->unitName, peer->rssi);
    } else {
      output.printf("+ Mesh peer: %02X:%02X (RSSI: %d dBm)\n",
                    peer->mac[4], peer->mac[5], peer->rssi);
    }
  } else if (!peer->isOnline) {
    if (strlen(peer->unitName) > 0) {
      output.printf("- Mesh peer offline: %s\n", peer->unitName);
    }
  }
}
#endif

// ============================================================================
// Stub functions for code compatibility
// ============================================================================

void beep() {
  // No buzzer on ESP8266 serial terminal
}

void oledPrint(const char* line1, const char* line2) {
  if (line1 && strlen(line1) > 0) {
    output.print("[STATUS] ");
    output.print(line1);
    if (line2 && strlen(line2) > 0) {
      output.print(" - ");
      output.print(line2);
    }
    output.println();
  }
}

void addToHistory(const char* msg) {
  messageHistory[historyCount % 10] = String(msg);
  historyCount++;
}

void broadcastEmergency() {
  emergencyActive = true;
  emergencyStartTime = millis();

  output.println("\n[EMERGENCY] Broadcasting emergency signal...");

  char baseMsg[64];
  snprintf(baseMsg, sizeof(baseMsg), "%s:0:EMERGENCY", unitName.c_str());

#if MESH_ENABLED
  uint32_t meshMsgId = meshMgr.sendEmergency(baseMsg, nullptr);
  if (meshMsgId > 0) {
    output.printf("[EMERGENCY] Broadcast sent (TTL: %d hops)\n", MESH_MAX_TTL);
  }
#endif

  addToHistory("EMERGENCY");
}

void cancelEmergency() {
  emergencyActive = false;
  output.println("[EMERGENCY] Emergency canceled");
}

void handleEmergency() {
  if (!emergencyActive) return;

  if (millis() - emergencyStartTime >= EMERGENCY_DURATION) {
    output.println("[EMERGENCY] Auto-canceled after 30 seconds");
    cancelEmergency();
    return;
  }
}

void simulateButtonPress(int buttonIndex) {
  if (buttonIndex < 0 || buttonIndex >= 4) return;

  char baseMsg[64];
  snprintf(baseMsg, sizeof(baseMsg), "%s:%d:%s",
           unitName.c_str(), buttonIndex + 1, BUTTON_LABELS[buttonIndex]);

#if MESH_ENABLED
  uint32_t msgId = meshMgr.broadcast((uint8_t*)baseMsg, strlen(baseMsg), MESH_MSG_DATA, MESH_DEFAULT_TTL);
  if (msgId > 0) {
    output.printf("Sent: %s (mesh id: %08X)\n", baseMsg, msgId);
    addToHistory(baseMsg);
  } else {
    output.println("Failed to send message");
  }
#else
  output.printf("Sent (local only): %s\n", baseMsg);
  addToHistory(baseMsg);
#endif
}

void configurePassphrase() {
  // Try to load saved passphrase from EEPROM first
  char saved[65];
  if (MeshCrypto::loadPassphrase(saved, sizeof(saved))) {
    strncpy(currentPassphrase, saved, sizeof(currentPassphrase) - 1);
    currentPassphrase[sizeof(currentPassphrase) - 1] = '\0';
    output.print("Loaded saved passphrase (");
    output.print(strlen(currentPassphrase));
    output.println(" chars)");
    return;
  }

  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║       CYPHER-CHAT 8266 - Initial Setup            ║");
  output.println("╚════════════════════════════════════════════════╝");
  output.println("\nEnter passphrase (min 4 chars, or press Enter for default):");
  output.println("Timeout: 15 seconds\n");

  unsigned long start = millis();
  String input = "";

  while (millis() - start < 15000) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') break;
      if (isPrintable(c) && input.length() < MAX_PASSPHRASE_LEN) {
        input += c;
        output.print('*');
      }
    }
  }
  output.println();

  if (input.length() >= MIN_PASSPHRASE_LEN) {
    strncpy(currentPassphrase, input.c_str(), sizeof(currentPassphrase) - 1);
    currentPassphrase[sizeof(currentPassphrase) - 1] = '\0';
    MeshCrypto::savePassphrase(currentPassphrase);
    output.print("Passphrase set (");
    output.print(strlen(currentPassphrase));
    output.println(" chars) - saved to EEPROM");
    delay(1000);
    return;
  }

  output.println("Using default passphrase");
  output.println("(Change with 'passphrase' command)");
  delay(1000);
}

// ============================================================================
// Arduino setup and loop
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize output manager (Serial only on ESP8266)
  output.begin();

  // Initialize terminal interface
  terminalMgr.begin();

  // Configure passphrase
  configurePassphrase();

  // No BLE on ESP8266
  output.println("\nBluetooth: Not available (ESP8266)");

  // Initialize mesh networking
#if MESH_ENABLED
  output.println("\nStarting Mesh Network (ESP-NOW)...");
  if (meshMgr.begin(unitName.c_str(), currentPassphrase)) {
    output.println("Mesh networking enabled");
    output.printf("  - Range: ~250m per hop\n");
    output.printf("  - TTL: %d hops max\n", MESH_DEFAULT_TTL);
    output.printf("  - Store-forward: %d messages\n", MESH_STORE_QUEUE_SIZE);
    output.println("  - ESP32 compatible: YES");

    meshMgr.onMessage(onMeshMessage);
    meshMgr.onPeerUpdate(onMeshPeerUpdate);
  } else {
    output.println("WARNING: Mesh init failed!");
  }
#endif

  // Ready
  output.println("\n════════════════════════════════════════════════");
  output.println("  CYPHER-CHAT 8266 Ready");
  output.println("════════════════════════════════════════════════");
  output.printf("Unit: %s\n", unitName.c_str());
  output.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  output.println("Type 'help' for commands\n");
}

void loop() {
  // Poll terminal for commands
  terminalMgr.poll();

  // Handle emergency state
  handleEmergency();

  // Update terminal display (monitor mode)
  terminalMgr.update();

#if MESH_ENABLED
  // Process mesh networking events
  meshMgr.update();
#endif

  delay(10);
}

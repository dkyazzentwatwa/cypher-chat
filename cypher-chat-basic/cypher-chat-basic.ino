/**
 * CYPHER-CHAT BASIC - Minimal ESP32 Mesh Terminal
 *
 * A barebones version of Cypher-Chat for ESP32 boards without
 * additional hardware. Just USB Serial + Bluetooth Serial + Mesh.
 *
 * Perfect for:
 * - Cheap $3-5 ESP32 boards (ESP32 DevKit, NodeMCU, etc.)
 * - Testing mesh networks without full hardware
 * - Terminal-only operation via phone or computer
 *
 * Features:
 * - USB Serial terminal (115200 baud)
 * - Bluetooth Serial terminal (BLE UART - works with phone apps)
 * - ESP-NOW mesh networking (~250m range, multi-hop relay)
 * - ChaCha20-Poly1305 AEAD encryption + HKDF-SHA256 key derivation
 *
 * Compatible phone apps:
 * - nRF Connect (iOS/Android)
 * - Serial Bluetooth Terminal (Android, BLE mode)
 * - Bluefruit Connect (iOS)
 */

#include "Config_Basic.h"
#include "MeshCrypto.h"
#include "OutputManager.h"
#if BLE_UART_ENABLED
#include "BLEUARTManager.h"
#endif
#include "TerminalManager.h"
#include "MeshManager.h"
#include "StateManager.h"

// Global runtime state
bool isServer = false;  // Role not used in basic version
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
const unsigned long EMERGENCY_DURATION = 30000; // 30 seconds

// Mesh networking callbacks
#if MESH_ENABLED
void onMeshMessage(const MeshPacket* packet, const uint8_t* senderMac, int8_t rssi) {
  // Extract message from payload
  char msg[128];
  size_t len = (packet->header.payloadLen < sizeof(msg) - 1) ?
               packet->header.payloadLen : sizeof(msg) - 1;
  memcpy(msg, packet->payload, len);
  msg[len] = '\0';

  // Format sender MAC for display
  char senderStr[32];
  snprintf(senderStr, sizeof(senderStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           packet->header.originMac[0], packet->header.originMac[1],
           packet->header.originMac[2], packet->header.originMac[3],
           packet->header.originMac[4], packet->header.originMac[5]);

  // Handle by message type
  if (packet->header.type == MESH_MSG_EMERGENCY) {
    // Parse emergency payload: [flags:1][gps:12?][msg:variable]
    uint8_t flags = packet->payload[0];
    size_t offset = 1;
    GPSCoordinates gps = {0, 0, 0, false};

    if (flags & 0x01 && packet->header.payloadLen >= 13) {
      // Has GPS data
      memcpy(&gps.latitude, &packet->payload[offset], 4);
      offset += 4;
      memcpy(&gps.longitude, &packet->payload[offset], 4);
      offset += 4;
      memcpy(&gps.altitude, &packet->payload[offset], 4);
      offset += 4;
      gps.valid = true;
    }

    // Extract message
    size_t msgLen = packet->header.payloadLen - offset;
    if (msgLen > sizeof(msg) - 1) msgLen = sizeof(msg) - 1;
    memcpy(msg, &packet->payload[offset], msgLen);
    msg[msgLen] = '\0';

    // Display emergency alert (terminal only - no LED/buzzer in basic version)
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
    // Regular data message
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

// Stub functions for code compatibility
void beep() {
  // No buzzer in basic version - silent operation
}

void oledPrint(const char* line1, const char* line2) {
  // No display in basic version - output to terminal instead
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
  // Store in circular buffer
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
  // Broadcast via mesh network for maximum range
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

  // Auto-cancel after 30 seconds
  if (millis() - emergencyStartTime >= EMERGENCY_DURATION) {
    output.println("[EMERGENCY] Auto-canceled after 30 seconds");
    cancelEmergency();
    return;
  }
}

void simulateButtonPress(int buttonIndex) {
  if (buttonIndex < 0 || buttonIndex >= 4) return;

  // Build message
  char baseMsg[64];
  snprintf(baseMsg, sizeof(baseMsg), "%s:%d:%s",
           unitName.c_str(), buttonIndex + 1, BUTTON_LABELS[buttonIndex]);

#if MESH_ENABLED
  // Send via mesh
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

// Helper to validate passphrase security
bool isPassphraseSecure(const char* passphrase) {
  if (!passphrase) return false;
  size_t len = strlen(passphrase);

  // Reject if too short
  if (len < MIN_PASSPHRASE_LEN) return false;

  // Reject known insecure passphrases
  if (strcmp(passphrase, INSECURE_DEFAULT_PASSPHRASE) == 0) return false;
  if (strcmp(passphrase, "password") == 0) return false;
  if (strcmp(passphrase, "12345678") == 0) return false;
  if (strcmp(passphrase, "00000000") == 0) return false;

  return true;
}

void configurePassphrase() {
  // Try to load saved passphrase from NVS first
  char saved[65];
  if (MeshCrypto::loadPassphrase(saved, sizeof(saved)) && isPassphraseSecure(saved)) {
    strncpy(currentPassphrase, saved, sizeof(currentPassphrase) - 1);
    currentPassphrase[sizeof(currentPassphrase) - 1] = '\0';
    output.print("Loaded saved passphrase (");
    output.print(strlen(currentPassphrase));
    output.println(" chars)");
    return;
  }

  // SECURITY: Require user to set a passphrase - no insecure defaults
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║      SECURITY: Passphrase Required             ║");
  output.println("╚════════════════════════════════════════════════╝");
  output.printf("\nEnter mesh passphrase (min %d characters):\n", MIN_PASSPHRASE_LEN);
  output.println("NOTE: '123456' and common passwords are BLOCKED");
  output.printf("Timeout: %d seconds\n\n", PASSPHRASE_INPUT_TIMEOUT_MS / 1000);

  unsigned long start = millis();
  unsigned long lastReminder = start;
  String input = "";
  bool validPassphrase = false;

  while (!validPassphrase && millis() - start < PASSPHRASE_INPUT_TIMEOUT_MS) {
    // Periodic reminder
    if (millis() - lastReminder >= PASSPHRASE_REMINDER_INTERVAL_MS) {
      int remaining = (PASSPHRASE_INPUT_TIMEOUT_MS - (millis() - start)) / 1000;
      output.printf("\n[%ds remaining] Enter passphrase: ", remaining);
      lastReminder = millis();
    }

    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        output.println();

        // Validate input
        if (input.length() < MIN_PASSPHRASE_LEN) {
          output.printf("Too short (min %d chars). Try again: ", MIN_PASSPHRASE_LEN);
          input = "";
          continue;
        }

        if (!isPassphraseSecure(input.c_str())) {
          output.println("That passphrase is too common/weak. Try a stronger one: ");
          input = "";
          continue;
        }

        validPassphrase = true;
      } else if (isPrintable(c) && input.length() < MAX_PASSPHRASE_LEN) {
        input += c;
        output.print('*');
      }
    }
  }

  if (validPassphrase) {
    strncpy(currentPassphrase, input.c_str(), sizeof(currentPassphrase) - 1);
    currentPassphrase[sizeof(currentPassphrase) - 1] = '\0';
    MeshCrypto::savePassphrase(currentPassphrase);
    output.print("Passphrase set (");
    output.print(strlen(currentPassphrase));
    output.println(" chars) - saved to NVS");
    delay(1000);
    return;
  }

  // Timeout reached without valid passphrase - CRITICAL: Do not use insecure default
  output.println("\n");
  output.println("╔════════════════════════════════════════════════╗");
  output.println("║  ERROR: Passphrase required for mesh security  ║");
  output.println("╚════════════════════════════════════════════════╝");
  output.println("\nDevice will restart in 5 seconds...");
  output.println("Please enter a secure passphrase on restart.\n");
  delay(5000);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize output manager (broadcasts to USB + BT)
  output.begin();

  // Initialize terminal interface
  terminalMgr.begin();

  // Configure passphrase
  configurePassphrase();

  // Initialize BLE UART service for phone connectivity
#if BLE_UART_ENABLED
  output.println("\nStarting Bluetooth (BLE UART)...");
  bleUARTMgr.begin(unitName.c_str());
  output.println("BLE UART ready - connect with nRF Connect or similar app");
#endif

  // Initialize mesh networking
#if MESH_ENABLED
  output.println("\nStarting Mesh Network (ESP-NOW)...");
  if (meshMgr.begin(unitName.c_str(), currentPassphrase)) {
    output.println("Mesh networking enabled");
    output.printf("  - Range: ~250m per hop\n");
    output.printf("  - TTL: %d hops max\n", MESH_DEFAULT_TTL);
    output.printf("  - Store-forward: %d messages\n", MESH_STORE_QUEUE_SIZE);

    meshMgr.onMessage(onMeshMessage);
    meshMgr.onPeerUpdate(onMeshPeerUpdate);
  } else {
    output.println("WARNING: Mesh init failed!");
  }
#endif

  // Ready
  output.println("\n════════════════════════════════════════════════");
  output.println("  CYPHER-CHAT BASIC Ready");
  output.println("════════════════════════════════════════════════");
  output.printf("Unit: %s\n", unitName.c_str());
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

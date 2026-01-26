#include "Config.h"
#include "MessageAuth.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "LEDManager.h"
#include "BuzzerManager.h"
#include "TerminalManager.h"
#include "Server.h"
#include "Client.h"
#include "MeshManager.h"
#include "GPSManager.h"

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);
unsigned long lastButtonPressMillis = 0;
const int DEBOUNCE_DELAY = 200;
const int BUTTON_PINS[] = { 17, 5, 18, 19 };
const char* BUTTON_LABELS[] = { "ACK", "ENROUTE", "NEED HELP", "ALL GOOD" };

bool isServer = false;
String unitName = DEFAULT_UNIT_NAME;
uint32_t currentPasskey = DEFAULT_PASSKEY;
String messageHistory[10];
int historyCount = 0;

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

    // Display emergency with high priority
    Serial.printf("\n!!! MESH EMERGENCY from %s (hops: %d, rssi: %d) !!!\n",
                  senderStr, packet->header.hopCount, rssi);
    Serial.printf("Message: %s\n", msg);
    if (gps.valid) {
      Serial.printf("GPS: %.6f, %.6f (alt: %.1fm)\n", gps.latitude, gps.longitude, gps.altitude);
    }

    // Trigger emergency indicators
    ledMgr.flash(LED_EMERGENCY, 2000);
    buzzerMgr.play(BUZZ_EMERGENCY);

    char displayMsg[32];
    snprintf(displayMsg, sizeof(displayMsg), "MESH EMERGENCY!");
    oledPrint(displayMsg, msg);
    addToHistory(msg);

  } else {
    // Regular data message
    Serial.printf("[MESH RX] From %s (hops: %d, rssi: %d): %s\n",
                  senderStr, packet->header.hopCount, rssi, msg);

    // Display and store
    char incomingMsg[32];
    snprintf(incomingMsg, sizeof(incomingMsg), "[Mesh] %s", msg);
    oledPrint("Mesh Message", msg);
    addToHistory(msg);

    ledMgr.flash(LED_MESSAGE_RX, 200);
    buzzerMgr.play(BUZZ_MESSAGE_RX);
  }
}

void onMeshPeerUpdate(const MeshPeer* peer, bool isNew) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           peer->mac[0], peer->mac[1], peer->mac[2],
           peer->mac[3], peer->mac[4], peer->mac[5]);

  if (isNew) {
    Serial.printf("[MESH] New peer discovered: %s", macStr);
    if (strlen(peer->unitName) > 0) {
      Serial.printf(" (%s)", peer->unitName);
    }
    Serial.printf(" rssi=%d\n", peer->rssi);
  } else if (!peer->isOnline) {
    Serial.printf("[MESH] Peer offline: %s\n", macStr);
  }
}
#endif

void beep() {
  // Use BuzzerManager for button feedback
  buzzerMgr.play(BUZZ_BUTTON);
}

void oledPrint(const char* line1, const char* line2) {
  // Redirect to DisplayManager for modern UI
  displayMgr.updateStatus(line1, line2);
}

void addToHistory(const char* msg) {
  // Legacy array for backward compatibility
  messageHistory[historyCount % 10] = String(msg);
  historyCount++;

  // Add to DisplayManager (detect direction - simple heuristic)
  bool isOutgoing = (strstr(msg, unitName.c_str()) == msg); // Message starts with our unit name
  displayMgr.addMessage(msg, isOutgoing);
}

void broadcastEmergency() {
  emergencyActive = true;
  emergencyStartTime = millis();

  // Set emergency LED and buzzer patterns
  ledMgr.setPattern(LED_EMERGENCY);
  buzzerMgr.play(BUZZ_EMERGENCY);

  oledPrint("EMERGENCY ACTIVE", "Broadcasting...");

  char baseMsg[64];
  snprintf(baseMsg, sizeof(baseMsg), "%s:0:EMERGENCY", unitName.c_str());

#if MESH_ENABLED
  // PRIMARY: Broadcast via mesh network for maximum range (~250m+ per hop)
  // Mesh will relay message up to MESH_MAX_TTL hops (potentially kilometers)
  GPSCoordinates* gpsPtr = nullptr;
  GPSCoordinates gpsCoords;

#if GPS_ENABLED
  // Include GPS coordinates if available
  if (gpsMgr.hasFix()) {
    gpsCoords = gpsMgr.getCoordinates();
    gpsPtr = &gpsCoords;

    char coordStr[64];
    gpsMgr.formatCoordinates(coordStr, sizeof(coordStr));
    Serial.printf("EMERGENCY includes GPS: %s\n", coordStr);

    char mapsUrl[80];
    gpsMgr.formatMapsURL(mapsUrl, sizeof(mapsUrl));
    Serial.printf("  Location: %s\n", mapsUrl);
  }
#endif

  uint32_t meshMsgId = meshMgr.sendEmergency(baseMsg, gpsPtr);
  if (meshMsgId > 0) {
    Serial.printf("EMERGENCY sent via MESH (id: %08X, TTL: %d hops)\n",
                  meshMsgId, MESH_MAX_TTL);
  }
#endif

  // BACKUP: Also send via BLE for close-range reliability
  // Send 3 times for redundancy on BLE channel
  for (int i = 0; i < 3; i++) {
    // Generate HMAC
    uint8_t hmac[HMAC_SIZE];
    char passkeyStr[16];
    snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", (unsigned long)currentPasskey);

    if (MessageAuth::generateHMAC(baseMsg, passkeyStr, hmac, HMAC_SIZE)) {
      char hmacHex[HMAC_HEX_SIZE + 1];
      MessageAuth::toHex(hmac, HMAC_SIZE, hmacHex, sizeof(hmacHex));

      char authenticatedMsg[MAX_MESSAGE_SIZE];
      snprintf(authenticatedMsg, sizeof(authenticatedMsg), "%s:%s", baseMsg, hmacHex);

      // Broadcast via appropriate BLE channel
      if (isServer) {
        // Server broadcasts to all clients
        extern NimBLECharacteristic* pTxCharacteristic;
        extern NimBLEServer* pServer;
        if (pTxCharacteristic && pServer && pServer->getConnectedCount() > 0) {
          pTxCharacteristic->setValue(std::string(authenticatedMsg));
          pTxCharacteristic->notify(true);
        }
      } else {
        // Client sends to server
        extern NimBLERemoteCharacteristic* pRemoteRxCharacteristic;
        if (pRemoteRxCharacteristic && pRemoteRxCharacteristic->canWrite()) {
          pRemoteRxCharacteristic->writeValue(std::string(authenticatedMsg), false);
        }
      }

      delay(100); // Small delay between transmissions
    }
  }

  addToHistory("EMERGENCY");
  Serial.println("EMERGENCY BROADCAST SENT (Mesh + BLE 3x redundancy)");
}

void cancelEmergency() {
  emergencyActive = false;
  ledMgr.setPattern(LED_IDLE);
  buzzerMgr.stop();
  oledPrint("Emergency canceled", "");
  Serial.println("Emergency canceled");
}

void handleEmergency() {
  if (!emergencyActive) return;

  // Check for auto-cancel after 30 seconds
  if (millis() - emergencyStartTime >= EMERGENCY_DURATION) {
    cancelEmergency();
    return;
  }

  // Display countdown
  unsigned long remaining = (EMERGENCY_DURATION - (millis() - emergencyStartTime)) / 1000;
  char line[32];
  snprintf(line, sizeof(line), "Auto-cancel: %lus", remaining);
  oledPrint("EMERGENCY ACTIVE", line);
}

void handleButtons() {
  int buttonIndex;
  ButtonEvent event = buttonMgr.poll(buttonIndex);

  if (event == BUTTON_NONE) return;

  if (event == BUTTON_LONG_PRESS) {
    if (buttonIndex == 2) {
      // Button 2 long-press: Trigger emergency
      if (!emergencyActive) {
        Serial.println("Emergency broadcast triggered (Button 2 long-press)");
        broadcastEmergency();
      }
    } else if (buttonIndex == 0) {
      // Button 0 long-press: Cancel emergency
      if (emergencyActive) {
        Serial.println("Emergency canceled (Button 0 long-press)");
        cancelEmergency();
      }
    }
  }
  // Regular button presses handled by Client/Server loops
}

void configurePasskey() {
  oledPrint("Enter 6-digit PIN", "Serial (10s timeout)");
  Serial.println("Enter 6-digit passkey (or press Enter for default 123456):");

  unsigned long start = millis();
  String input = "";

  while (millis() - start < 10000) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') break;
      if (isdigit(c) && input.length() < PASSKEY_DIGITS) {
        input += c;
        Serial.print('*');  // Echo asterisk for feedback
      }
    }
  }
  Serial.println();

  // Validate passkey
  if (input.length() == PASSKEY_DIGITS) {
    uint32_t newPasskey = input.toInt();
    if (newPasskey >= MIN_PASSKEY && newPasskey <= MAX_PASSKEY) {
      currentPasskey = newPasskey;
      Serial.print("Passkey set to: ");
      Serial.println(currentPasskey);
      oledPrint("PIN configured", "Secure connection");
      delay(1000);
      return;
    } else {
      Serial.println("ERROR: Passkey out of range (100000-999999)");
    }
  } else if (input.length() > 0) {
    Serial.print("ERROR: Passkey must be exactly ");
    Serial.print(PASSKEY_DIGITS);
    Serial.println(" digits");
  }

  // Use default passkey
  Serial.println("Using default passkey: 123456");
  Serial.println("WARNING: Default passkey is insecure! Change it for production use.");
  oledPrint("Using default PIN", "⚠ INSECURE ⚠");
  delay(2000);
}

void detectRole() {
  delay(500);
  if (digitalRead(BUTTON_PINS[0]) == LOW) {
    isServer = true;
    unitName = "CYPHER_Server";
  } else {
    unitName = DEFAULT_UNIT_NAME;
  }
}

void simulateButtonPress(int buttonIndex) {
  if (buttonIndex < 0 || buttonIndex >= NUM_BUTTONS) return;

  // Use the same logic as physical button press
  lastButtonPressMillis = millis();

  if (isServer) {
    // Server sends to all clients (same as loopServer)
    char baseMsg[64];
    snprintf(baseMsg, sizeof(baseMsg), "%s:%d:%s",
             unitName.c_str(), buttonIndex + 1, BUTTON_LABELS[buttonIndex]);

    // Generate HMAC
    uint8_t hmac[HMAC_SIZE];
    char passkeyStr[16];
    snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", (unsigned long)currentPasskey);

    if (MessageAuth::generateHMAC(baseMsg, passkeyStr, hmac, HMAC_SIZE)) {
      char hmacHex[HMAC_HEX_SIZE + 1];
      MessageAuth::toHex(hmac, HMAC_SIZE, hmacHex, sizeof(hmacHex));

      char authenticatedMsg[MAX_MESSAGE_SIZE];
      snprintf(authenticatedMsg, sizeof(authenticatedMsg), "%s:%s", baseMsg, hmacHex);

      extern NimBLECharacteristic* pTxCharacteristic;
      extern NimBLEServer* pServer;
      if (pTxCharacteristic && pServer && pServer->getConnectedCount() > 0) {
        pTxCharacteristic->setValue(std::string(authenticatedMsg));
        pTxCharacteristic->notify(true);
      }

      addToHistory(baseMsg);
      char sentMsg[32];
      snprintf(sentMsg, sizeof(sentMsg), "Sent: %s", baseMsg);
      oledPrint(sentMsg, "");
      beep();
    }
  } else {
    // Client sends to server (same as Client.cpp handleClientButtons)
    char baseMsg[64];
    snprintf(baseMsg, sizeof(baseMsg), "%s:%d:%s",
             unitName.c_str(), buttonIndex + 1, BUTTON_LABELS[buttonIndex]);

    uint8_t hmac[HMAC_SIZE];
    char passkeyStr[16];
    snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", (unsigned long)currentPasskey);

    if (MessageAuth::generateHMAC(baseMsg, passkeyStr, hmac, HMAC_SIZE)) {
      char hmacHex[HMAC_HEX_SIZE + 1];
      MessageAuth::toHex(hmac, HMAC_SIZE, hmacHex, sizeof(hmacHex));

      char authenticatedMsg[MAX_MESSAGE_SIZE];
      snprintf(authenticatedMsg, sizeof(authenticatedMsg), "%s:%s", baseMsg, hmacHex);

      extern NimBLERemoteCharacteristic* pRemoteRxCharacteristic;
      if (pRemoteRxCharacteristic && pRemoteRxCharacteristic->canWrite()) {
        pRemoteRxCharacteristic->writeValue(std::string(authenticatedMsg), false);
      }

      addToHistory(baseMsg);
      char sentMsg[32];
      snprintf(sentMsg, sizeof(sentMsg), "Sent: %s", baseMsg);
      oledPrint(sentMsg, "");
      beep();
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);  // Let serial stabilize

  // Initialize terminal interface first
  terminalMgr.begin();

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.clearDisplay();
  display.display();

  for (int i = 0; i < NUM_BUTTONS; i++) {
#if defined(ARDUINO_ARCH_ESP32)
    if (digitalPinCanOutput(BUTTON_PINS[i])) {
      pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    } else {
      pinMode(BUTTON_PINS[i], INPUT);
    }
#else
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
#endif
  }

#if BUZZER_PIN != -1
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#endif

  // Initialize managers
  displayMgr.begin();
  buttonMgr.begin();
  ledMgr.begin();
  buzzerMgr.begin();

  detectRole();
  configurePasskey();

  // Print configuration to terminal
  terminalMgr.printConfiguration();

  if (isServer) {
    setupServer();
  } else {
    setupClient();
  }

  // Initialize mesh networking layer (ESP-NOW)
  // This runs alongside BLE for extended range and true mesh relay
#if MESH_ENABLED
  oledPrint("Starting Mesh...", "ESP-NOW");
  if (meshMgr.begin(unitName.c_str(), currentPasskey)) {
    Serial.println("Mesh networking enabled (ESP-NOW)");
    Serial.printf("  - Range: ~250m (vs BLE ~30m)\n");
    Serial.printf("  - TTL: %d hops max\n", MESH_DEFAULT_TTL);
    Serial.printf("  - Store-forward queue: %d messages\n", MESH_STORE_QUEUE_SIZE);

    // Set up mesh message callback
    meshMgr.onMessage(onMeshMessage);
    meshMgr.onPeerUpdate(onMeshPeerUpdate);
  } else {
    Serial.println("WARNING: Mesh init failed, BLE-only mode");
  }
#endif

  // Initialize GPS module (optional)
#if GPS_ENABLED
  oledPrint("Starting GPS...", "Waiting for fix");
  if (gpsMgr.begin()) {
    Serial.println("GPS module initialized");
    Serial.printf("  Waiting for satellite fix...\n");
  } else {
    Serial.println("GPS module not detected");
  }
#endif
}

void loop() {
  // Poll terminal for commands (non-blocking)
  terminalMgr.poll();

  // Handle emergency state
  handleEmergency();

  // Handle button events (long-press for emergency)
  handleButtons();

  if (isServer) {
    loopServer();
  } else {
    clientLoop();
  }

  // Update managers
  displayMgr.refresh();
  ledMgr.update();
  buzzerMgr.update();
  terminalMgr.update();  // Update terminal display (monitor mode)

#if MESH_ENABLED
  // Process mesh networking events
  meshMgr.update();
#endif

#if GPS_ENABLED
  // Process GPS data
  gpsMgr.update();
#endif

  delay(10);
}

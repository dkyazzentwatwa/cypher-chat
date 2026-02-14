#include "Config.h"
#include "MeshCrypto.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "LEDManager.h"
#include "BuzzerManager.h"
#include "OutputManager.h"
#if BLE_UART_ENABLED
#include "BLEUARTManager.h"
#endif
#include "TerminalManager.h"
#include "MeshManager.h"
#include "GPSManager.h"
#include "PowerManager.h"
#include "SecurityManager.h"
#include "FileSystemManager.h"
#include "TimeManager.h"
#include "LogManager.h"

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);
unsigned long lastButtonPressMillis = 0;
const int DEBOUNCE_DELAY = 200;
const int BUTTON_PINS[] = { KEY1_PIN, KEY2_PIN, KEY3_PIN };
const char* BUTTON_LABELS[] = { "KEY1", "KEY2", "KEY3" };

bool isServer = false;
String unitName = DEFAULT_UNIT_NAME;
char currentPassphrase[65] = DEFAULT_PASSPHRASE;
String messageHistory[10];
int historyCount = 0;

// Global connection state
#include "StateManager.h"
StateManager connectionState;

// BLE Global objects (referenced by Server.cpp, Client.cpp, and loops)
#if BLE_ENABLED
#include <NimBLEDevice.h>
extern NimBLEServer* pServer;
extern NimBLECharacteristic* pTxCharacteristic;
extern NimBLERemoteCharacteristic* pRemoteRxCharacteristic;
#endif

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
    output.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    output.printf("🚨 EMERGENCY from %s\n", senderStr);
    output.printf("   Message: %s\n", msg);
    if (gps.valid) {
      output.printf("   GPS: %.6f, %.6f (%.1fm)\n", gps.latitude, gps.longitude, gps.altitude);
    }
    output.printf("   Signal: %d dBm | Hops: %d\n", rssi, packet->header.hopCount);
    output.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    // Trigger emergency indicators
    ledMgr.flash(LED_EMERGENCY, 2000);
    buzzerMgr.play(BUZZ_EMERGENCY);

    char displayMsg[32];
    snprintf(displayMsg, sizeof(displayMsg), "MESH EMERGENCY!");
    oledPrint(displayMsg, msg);
    addToHistory(msg);

  } else {
    // Regular data message
    output.printf("Mesh RX: %s\n", msg);

    // Display and store
    char incomingMsg[32];
    snprintf(incomingMsg, sizeof(incomingMsg), "[Mesh] %s", msg);
    oledPrint("Mesh Message", msg);
    addToHistory(msg);

    ledMgr.flash(LED_MESSAGE_RX, 200);
    buzzerMgr.play(BUZZ_MESSAGE_RX);
  }
}

void onMeshPeerUpdate(const MeshPeerInfo* peer, bool isNew) {
  if (isNew) {
    if (strlen(peer->unitName) > 0) {
      output.printf("✓ Mesh peer: %s (RSSI: %d dBm)\n", peer->unitName, peer->rssi);
    } else {
      output.printf("✓ Mesh peer: %02X:%02X (RSSI: %d dBm)\n",
                    peer->mac[4], peer->mac[5], peer->rssi);
    }
  } else if (!peer->isOnline) {
    if (strlen(peer->unitName) > 0) {
      output.printf("✗ Mesh peer offline: %s\n", peer->unitName);
    }
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
    output.printf("EMERGENCY includes GPS: %s\n", coordStr);

    char mapsUrl[80];
    gpsMgr.formatMapsURL(mapsUrl, sizeof(mapsUrl));
    output.printf("  Location: %s\n", mapsUrl);
  }
#endif

  uint32_t meshMsgId = meshMgr.sendEmergency(baseMsg, gpsPtr);
  if (meshMsgId > 0) {
    output.printf("🚨 Emergency broadcast sent (TTL: %d hops)\n", MESH_MAX_TTL);
  }
#endif

  // BACKUP: Also send via BLE for close-range reliability
  // Send 3 times for redundancy on BLE channel
  for (int i = 0; i < 3; i++) {
    // Broadcast via appropriate BLE channel
    if (isServer) {
#if BLE_ENABLED
      if (pTxCharacteristic && pServer && pServer->getConnectedCount() > 0) {
        pTxCharacteristic->setValue(std::string(baseMsg));
        pTxCharacteristic->notify(true);
      }
#endif
    } else {
#if BLE_ENABLED
      if (pRemoteRxCharacteristic && pRemoteRxCharacteristic->canWrite()) {
        pRemoteRxCharacteristic->writeValue(std::string(baseMsg), false);
      }
#endif
    }

    delay(100); // Small delay between transmissions
  }

  addToHistory("EMERGENCY");
}

void cancelEmergency() {
  emergencyActive = false;
  ledMgr.setPattern(LED_IDLE);
  buzzerMgr.stop();
  oledPrint("Emergency canceled", "");
  output.println("Emergency canceled");
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

  if (event == BUTTON_PRESS) {
    if (buttonIndex == 2) {
      // Button 3 (index 2): Cycle OLED screens
      DisplayScreen current = displayMgr.getCurrentScreen();
      if (current == SCREEN_STATUS) displayMgr.setScreen(SCREEN_MESSAGES);
      else if (current == SCREEN_MESSAGES) displayMgr.setScreen(SCREEN_HISTORY);
      else displayMgr.setScreen(SCREEN_STATUS);
      
      output.println("Screen cycled via Button 3");
      beep();
      return;
    }
  }

  if (event == BUTTON_LONG_PRESS) {
    if (buttonIndex == 2) {
      // Button 3 long-press: Trigger emergency
      if (!emergencyActive) {
        output.println("Emergency broadcast triggered (Button 3 long-press)");
        broadcastEmergency();
      }
    } else if (buttonIndex == 0) {
      // Button 0 long-press: Cancel emergency
      if (emergencyActive) {
        output.println("Emergency canceled (Button 0 long-press)");
        cancelEmergency();
      }
    }
  }
  // Regular button presses handled by Client/Server loops
}

void configurePassphrase() {
  // Check NVS for saved passphrase first
  char saved[65] = {0};
  if (MeshCrypto::loadPassphrase(saved, sizeof(saved)) && strlen(saved) >= MIN_PASSPHRASE_LEN) {
    strncpy(currentPassphrase, saved, sizeof(currentPassphrase) - 1);
    currentPassphrase[sizeof(currentPassphrase) - 1] = '\0';
    output.println("Loaded saved passphrase from NVS");
    oledPrint("Passphrase loaded", "From NVS");
    delay(500);
    return;
  }

  oledPrint("Enter passphrase", "Serial (15s timeout)");
  output.println("Enter mesh passphrase (min 4 chars, or Enter for default):");

  unsigned long start = millis();
  String input = "";

  while (millis() - start < 15000) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') break;
      if (c >= 0x20 && c < 0x7F && input.length() < MAX_PASSPHRASE_LEN) {
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
    output.println("Passphrase set and saved to NVS");
    oledPrint("Passphrase set", "Saved to device");
    delay(1000);
    return;
  } else if (input.length() > 0) {
    output.printf("Too short (min %d chars). Using default.\n", MIN_PASSPHRASE_LEN);
  }

  // Use default passphrase
  strncpy(currentPassphrase, DEFAULT_PASSPHRASE, sizeof(currentPassphrase) - 1);
  output.println("Using default passphrase");
  output.println("WARNING: Default passphrase is insecure! Use 'passphrase' command to change.");
  oledPrint("Default passphrase", "INSECURE");
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

  // Build message for button press
  char baseMsg[64];
  snprintf(baseMsg, sizeof(baseMsg), "%s:%d:%s",
           unitName.c_str(), buttonIndex + 1, BUTTON_LABELS[buttonIndex]);

  // Send via mesh (encrypted by MeshManager)
#if MESH_ENABLED
  meshMgr.broadcast((uint8_t*)baseMsg, strlen(baseMsg));
#endif

  // Also send via BLE if available
  if (isServer) {
#if BLE_ENABLED
    if (pTxCharacteristic && pServer && pServer->getConnectedCount() > 0) {
      pTxCharacteristic->setValue(std::string(baseMsg));
      pTxCharacteristic->notify(true);
    }
#endif
  } else {
#if BLE_ENABLED
    if (pRemoteRxCharacteristic && pRemoteRxCharacteristic->canWrite()) {
      pRemoteRxCharacteristic->writeValue(std::string(baseMsg), false);
    }
#endif
  }

  addToHistory(baseMsg);
  char sentMsg[32];
  snprintf(sentMsg, sizeof(sentMsg), "Sent: %s", baseMsg);
  oledPrint(sentMsg, "");
  beep();
}

void setup() {
  Serial.begin(115200);
  delay(100);  // Let serial stabilize

  // Initialize output manager
  output.begin();

  // Initialize terminal interface first
  terminalMgr.begin();

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    output.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.clearDisplay();
  display.display();

  for (int i = 0; i < NUM_BUTTONS; i++) {
    // cypher-chat pins 34, 36, 39 are input-only and use external resistors
    pinMode(BUTTON_PINS[i], INPUT);
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

  // Initialize new managers
  powerMgr.begin();
  securityMgr.begin();
  fileSystemMgr.begin();
  timeMgr.begin();
  logMgr.begin();

  detectRole();
  configurePassphrase();

  // Initialize BLE UART service with unit name (after detectRole sets it)
#if BLE_UART_ENABLED
  bleUARTMgr.begin(unitName.c_str());
#endif

  // Print configuration to terminal
  terminalMgr.printConfiguration();

  // Note: Old BLE server/client code removed (BLE_ENABLED=false)
  // Mesh networking is now the primary transport

  // Initialize mesh networking layer (ESP-NOW)
  // This runs alongside BLE for extended range and true mesh relay
#if MESH_ENABLED
  oledPrint("Starting Mesh...", "ESP-NOW");
  if (meshMgr.begin(unitName.c_str(), currentPassphrase)) {
    output.println("Mesh networking enabled (ESP-NOW)");
    output.printf("  - Range: ~250m (vs BLE ~30m)\n");
    output.printf("  - TTL: %d hops max\n", MESH_DEFAULT_TTL);
    output.printf("  - Store-forward queue: %d messages\n", MESH_STORE_QUEUE_SIZE);

    // Set up mesh message callback
    meshMgr.onMessage(onMeshMessage);
    meshMgr.onPeerUpdate(onMeshPeerUpdate);
  } else {
    output.println("WARNING: Mesh init failed, BLE-only mode");
  }
#endif

  // Initialize GPS module (optional)
#if GPS_ENABLED
  oledPrint("Starting GPS...", "Waiting for fix");
  if (gpsMgr.begin()) {
    output.println("GPS module initialized");
    output.printf("  Waiting for satellite fix...\n");
  } else {
    output.println("GPS module not detected");
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

  // Note: Old BLE server/client loops removed (BLE_ENABLED=false)

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
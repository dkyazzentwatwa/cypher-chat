#include "Config.h"
#include "MessageAuth.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "LEDManager.h"
#include "BuzzerManager.h"
#include "TerminalManager.h"
#include "Server.h"
#include "Client.h"

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

  // Send emergency message 3 times for redundancy
  for (int i = 0; i < 3; i++) {
    char baseMsg[64];
    snprintf(baseMsg, sizeof(baseMsg), "%s:0:EMERGENCY", unitName.c_str());

    // Generate HMAC
    uint8_t hmac[HMAC_SIZE];
    char passkeyStr[16];
    snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", (unsigned long)currentPasskey);

    if (MessageAuth::generateHMAC(baseMsg, passkeyStr, hmac, HMAC_SIZE)) {
      char hmacHex[HMAC_HEX_SIZE + 1];
      MessageAuth::toHex(hmac, HMAC_SIZE, hmacHex, sizeof(hmacHex));

      char authenticatedMsg[MAX_MESSAGE_SIZE];
      snprintf(authenticatedMsg, sizeof(authenticatedMsg), "%s:%s", baseMsg, hmacHex);

      // Broadcast via appropriate channel
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
  Serial.println("EMERGENCY BROADCAST SENT (3x redundancy)");
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

void sendCustomMessage(const char* message) {
  if (message == nullptr || strlen(message) == 0) return;

  lastButtonPressMillis = millis();

  // Build base message: UNIT_NAME:0:CUSTOM_MESSAGE (0 indicates custom msg)
  char baseMsg[MAX_MESSAGE_SIZE];
  snprintf(baseMsg, sizeof(baseMsg), "%s:0:%s", unitName.c_str(), message);

  // Generate HMAC
  uint8_t hmac[HMAC_SIZE];
  char passkeyStr[16];
  snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", (unsigned long)currentPasskey);

  if (!MessageAuth::generateHMAC(baseMsg, passkeyStr, hmac, HMAC_SIZE)) {
    Serial.println("HMAC generation failed");
    return;
  }

  char hmacHex[HMAC_HEX_SIZE + 1];
  MessageAuth::toHex(hmac, HMAC_SIZE, hmacHex, sizeof(hmacHex));

  char authenticatedMsg[MAX_MESSAGE_SIZE];
  snprintf(authenticatedMsg, sizeof(authenticatedMsg), "%s:%s", baseMsg, hmacHex);

  if (isServer) {
    extern NimBLECharacteristic* pTxCharacteristic;
    extern NimBLEServer* pServer;
    if (pTxCharacteristic && pServer && pServer->getConnectedCount() > 0) {
      pTxCharacteristic->setValue(std::string(authenticatedMsg));
      pTxCharacteristic->notify(true);
    }
  } else {
    extern NimBLERemoteCharacteristic* pRemoteRxCharacteristic;
    if (pRemoteRxCharacteristic && pRemoteRxCharacteristic->canWrite()) {
      pRemoteRxCharacteristic->writeValue(std::string(authenticatedMsg), false);
    }
  }

  addToHistory(baseMsg);
  Serial.print("Sent: ");
  Serial.println(message);
  oledPrint("Sent:", message);
  beep();
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

  delay(10);
}

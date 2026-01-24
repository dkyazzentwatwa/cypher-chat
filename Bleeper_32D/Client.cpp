#include "Client.h"
#include "MessageAuth.h"
#include "StateManager.h"
#include "DisplayManager.h"
#include "LEDManager.h"
#include "BuzzerManager.h"

String lastSentMsg = "";

NimBLEScan* pScan = nullptr;
NimBLERemoteCharacteristic* pRemoteTxCharacteristic = nullptr;
NimBLERemoteCharacteristic* pRemoteRxCharacteristic = nullptr;
static NimBLEAdvertisedDevice* pServerDevice = nullptr;

// Thread-safe state management (replaces volatile flags)
StateManager connectionState;

unsigned long lastScanAttempt = 0;
unsigned long lastConnectionAttempt = 0;

// Message retry system
#define MAX_RETRY_ATTEMPTS 5
#define RETRY_INTERVAL_MS 5000

struct PendingMessage {
  char message[64];
  unsigned long lastRetryTime;
  int retryCount;
  bool active;
};

PendingMessage pendingMessage = {"", 0, 0, false};

static void freeServerDevice() {
    if (pServerDevice) {
        delete pServerDevice;
        pServerDevice = nullptr;
    }
}

void notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    // Validate message length
    if (length == 0 || length >= MAX_MESSAGE_SIZE) {
        Serial.print("Invalid message length: ");
        Serial.println(length);
        return;
    }

    // Use fixed-size buffer instead of VLA
    char msg[MAX_MESSAGE_SIZE];
    size_t copyLen = (length < MAX_MESSAGE_SIZE - 1) ? length : (MAX_MESSAGE_SIZE - 1);
    memcpy(msg, pData, copyLen);
    msg[copyLen] = '\0';

    // Sanitize input to prevent control characters
    MessageAuth::sanitizeInput(msg, MAX_MESSAGE_SIZE);

    String msgStr = String(msg);

    // Handle ACK messages
    if (msgStr.startsWith("ACK:")) {
        // Expected format: ACK:UNIT_NAME:HMAC
        int lastColon = msgStr.lastIndexOf(':');
        if (lastColon > 4) {
            String target = msgStr.substring(4, lastColon);
            String hmacHexReceived = msgStr.substring(lastColon + 1);

            // Verify ACK HMAC
            char baseAck[64];
            snprintf(baseAck, sizeof(baseAck), "ACK:%s", target.c_str());

            uint8_t hmacExpected[HMAC_SIZE];
            char passkeyStr[16];
            snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", (unsigned long)currentPasskey);

            if (MessageAuth::generateHMAC(baseAck, passkeyStr, hmacExpected, HMAC_SIZE)) {
                char hmacHexExpected[HMAC_HEX_SIZE + 1];
                MessageAuth::toHex(hmacExpected, HMAC_SIZE, hmacHexExpected, sizeof(hmacHexExpected));

                if (hmacHexReceived == String(hmacHexExpected) && target == unitName) {
                    // Mark message as delivered in DisplayManager
                    if (lastSentMsg.length() > 0) {
                        displayMgr.markDelivered(lastSentMsg.c_str());
                    }

                    // Clear pending retry
                    pendingMessage.active = false;

                    oledPrint("Delivered", "Connected to Server");
                    buzzerMgr.play(BUZZ_BUTTON);
                } else {
                    Serial.println("ACK HMAC verification failed!");
                }
            }
        }
        return;
    }

    // Regular message - extract HMAC
    int lastColon = msgStr.lastIndexOf(':');
    if (lastColon == -1 || lastColon + HMAC_HEX_SIZE + 1 != (int)msgStr.length()) {
        Serial.println("Invalid message format (no HMAC)");
        oledPrint("SPOOFED MESSAGE", "HMAC missing");
        buzzerMgr.play(BUZZ_ERROR);
        return;
    }

    String baseMsg = msgStr.substring(0, lastColon);
    String hmacHexReceived = msgStr.substring(lastColon + 1);

    // Verify message HMAC
    uint8_t hmacExpected[HMAC_SIZE];
    char passkeyStr[16];
    snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", (unsigned long)currentPasskey);

    if (!MessageAuth::generateHMAC(baseMsg.c_str(), passkeyStr, hmacExpected, HMAC_SIZE)) {
        Serial.println("HMAC generation failed");
        oledPrint("HMAC error", "Cannot verify");
        buzzerMgr.play(BUZZ_ERROR);
        return;
    }

    char hmacHexExpected[HMAC_HEX_SIZE + 1];
    MessageAuth::toHex(hmacExpected, HMAC_SIZE, hmacHexExpected, sizeof(hmacHexExpected));

    if (hmacHexReceived != String(hmacHexExpected)) {
        Serial.println("HMAC verification FAILED - spoofed message detected!");
        oledPrint("SPOOFED MESSAGE", "HMAC mismatch");
        ledMgr.flash(LED_ERROR, 500);
        buzzerMgr.play(BUZZ_ERROR);
        return;
    }

    // HMAC verified - process message
    addToHistory(baseMsg.c_str());
    char incomingMsg[32];
    int written = snprintf(incomingMsg, sizeof(incomingMsg), "Incoming: %s", baseMsg.c_str());
    if (written < 0 || written >= (int)sizeof(incomingMsg)) {
        strcpy(incomingMsg, "Incoming: [msg]");
    }
    oledPrint(incomingMsg, "Connected to Server");

    // Trigger RX indicators
    ledMgr.flash(LED_MESSAGE_RX, 200);
    buzzerMgr.play(BUZZ_MESSAGE_RX);
}

class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* /*pClient*/) override {
    connectionState.setState(STATE_CONNECTING);
    oledPrint("Connected to Server", "Discovering services...");
  }

  void onDisconnect(NimBLEClient* /*pClient*/, int reason) override {
    connectionState.setState(STATE_DISCONNECTED);
    freeServerDevice();
    pRemoteTxCharacteristic = nullptr;
    pRemoteRxCharacteristic = nullptr;
    connectionState.incrementRetry();

    char line[32];
    int retries = connectionState.getRetryCount();
    snprintf(line, sizeof(line), "Reason: %d (%d/%d)", reason, retries, BLEEPER_MAX_CONN_FAILURES);
    oledPrint("Disconnected", line);

    if (retries >= BLEEPER_MAX_CONN_FAILURES) {
      connectionState.setState(STATE_ERROR);
      oledPrint("Too many failures", "Restarting ESP32...");
    }
  }

  void onConnectFail(NimBLEClient* /*pClient*/, int reason) override {
    connectionState.setState(STATE_DISCONNECTED);
    connectionState.incrementRetry();

    char line[32];
    int retries = connectionState.getRetryCount();
    snprintf(line, sizeof(line), "Fail: %d (%d/%d)", reason, retries, BLEEPER_MAX_CONN_FAILURES);
    oledPrint("Connection Failed", line);

    if (retries >= BLEEPER_MAX_CONN_FAILURES) {
      connectionState.setState(STATE_ERROR);
      oledPrint("Too many failures", "Restarting ESP32...");
    }
  }

  void onPassKeyEntry(NimBLEConnInfo& connInfo) override {
    NimBLEDevice::injectPassKey(connInfo, currentPasskey);
    char line[32];
    snprintf(line, sizeof(line), "PIN %06lu", (unsigned long)currentPasskey);
    oledPrint("Sending passkey", line);
  }

  void onConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t pin) override {
    NimBLEDevice::injectConfirmPasskey(connInfo, pin == currentPasskey);
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
    if (connInfo.isEncrypted()) {
      connectionState.setState(STATE_CONNECTED);
      connectionState.resetRetry();
      oledPrint("Secure link", "Authenticated");
      buzzerMgr.play(BUZZ_CONNECTED);
    } else {
      connectionState.setState(STATE_DISCONNECTED);
      connectionState.incrementRetry();
      oledPrint("Auth failed", "Retrying...");
      buzzerMgr.play(BUZZ_ERROR);
    }
  }
};

static ClientCallbacks clientCallbacks;

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
    if (advertisedDevice->isAdvertisingService(NimBLEUUID(SERVICE_UUID))) {
      pScan->stop();
      freeServerDevice();
      pServerDevice = new NimBLEAdvertisedDevice(*advertisedDevice);
      connectionState.setState(STATE_IDLE);  // Ready to connect
    }
  }

  void onScanEnd(const NimBLEScanResults& /*results*/, int /*reason*/) override {
    ConnectionState currentState = connectionState.getState();
    if (currentState == STATE_SCANNING) {
      // Scan ended without finding server
      connectionState.setState(STATE_DISCONNECTED);
      connectionState.incrementRetry();

      unsigned long backoffDelay = connectionState.getBackoffDelay();
      char line[32];
      snprintf(line, sizeof(line), "Retry in %lus", backoffDelay / 1000);
      oledPrint("Server not found", line);
    }
  }
};

static ScanCallbacks scanCallbacks;

bool connectToServer() {
  // Check exponential backoff delay
  unsigned long backoffDelay = connectionState.getBackoffDelay();
  if (millis() - lastConnectionAttempt < backoffDelay) {
    return false;
  }
  lastConnectionAttempt = millis();

  if (pServerDevice == nullptr) {
    oledPrint("No server device", "Scan first");
    return false;
  }

  NimBLEClient* pClient = NimBLEDevice::getDisconnectedClient();
  bool isNewClient = false;

  if (pClient == nullptr) {
    // Check if we've hit the maximum number of client connections (typically 3-4 on ESP32)
    if (NimBLEDevice::getCreatedClientCount() >= 3) {
      oledPrint("Max connections!", "Waiting...");
      return false;
    }

    pClient = NimBLEDevice::createClient();
    if (pClient == nullptr) {
      oledPrint("Client alloc fail", "Memory issue");
      connectionState.incrementRetry();
      return false;
    }
    isNewClient = true;
  }

  pClient->setClientCallbacks(&clientCallbacks, false);
  if (isNewClient) {
    pClient->setConnectionParams(12, 12, 0, 51);
    pClient->setConnectTimeout(5);
  }

  connectionState.setState(STATE_CONNECTING);
  oledPrint("Connecting...", "Please wait");

  if (!pClient->connect(pServerDevice)) {
    if (isNewClient) {
      NimBLEDevice::deleteClient(pClient);
    }
    connectionState.setState(STATE_DISCONNECTED);
    connectionState.incrementRetry();
    oledPrint("Connect failed", "Will retry");
    return false;
  }

  NimBLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    oledPrint("Service not found", "Wrong server?");
    pClient->disconnect();
    connectionState.setState(STATE_DISCONNECTED);
    connectionState.incrementRetry();
    return false;
  }

  pRemoteTxCharacteristic = pRemoteService->getCharacteristic(TX_CHARACTERISTIC_UUID);
  pRemoteRxCharacteristic = pRemoteService->getCharacteristic(RX_CHARACTERISTIC_UUID);

  if (pRemoteTxCharacteristic == nullptr || pRemoteRxCharacteristic == nullptr) {
    oledPrint("Characteristic fail", "Wrong version?");
    pClient->disconnect();
    connectionState.setState(STATE_DISCONNECTED);
    connectionState.incrementRetry();
    return false;
  }

  if (pRemoteTxCharacteristic->canNotify()) {
    if (!pRemoteTxCharacteristic->subscribe(true, notifyCallback)) {
      oledPrint("Subscribe failed", "Will continue");
    }
  }

  return true;
}

void startScan() {
  ConnectionState currentState = connectionState.getState();
  if (currentState == STATE_SCANNING) return;

  // Check exponential backoff delay
  unsigned long backoffDelay = connectionState.getBackoffDelay();
  if (millis() - lastScanAttempt < backoffDelay) {
    return;
  }
  lastScanAttempt = millis();

  connectionState.setState(STATE_SCANNING);

  char statusLine[32];
  int retries = connectionState.getRetryCount();
  if (retries > 0) {
    snprintf(statusLine, sizeof(statusLine), "Retry %d/%d", retries, BLEEPER_MAX_CONN_FAILURES);
    oledPrint("Scanning for server", statusLine);
  } else {
    oledPrint("Scanning for server", unitName.c_str());
  }

  if (pScan == nullptr) {
    oledPrint("Scan object null", "Init failed");
    connectionState.setState(STATE_ERROR);
    return;
  }

  if (!pScan->start(5, false)) {
    connectionState.setState(STATE_DISCONNECTED);
    connectionState.incrementRetry();
    oledPrint("Scan start failed", "BLE issue");
  }
}

void setupClient() {
    char splash[32];
    snprintf(splash, sizeof(splash), "Starting Client %s", unitName.c_str());
    oledPrint(splash, "Initializing...");

    delay(1000);

    if (!NimBLEDevice::init(unitName.c_str())) {
        oledPrint("BLE init failed", "Hardware issue");
        ESP.restart();
        return;
    }

    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityPasskey(currentPasskey);
    NimBLEDevice::setPower(BLE_TX_POWER);

    pScan = NimBLEDevice::getScan();
    if (pScan == nullptr) {
        oledPrint("Scan init failed", "Restarting...");
        delay(1000);
        ESP.restart();
        return;
    }

    pScan->setScanCallbacks(&scanCallbacks, false);
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);

    // Initialize state manager
    connectionState.setState(STATE_IDLE);
    connectionState.resetRetry();

    oledPrint("Client ready", "Starting scan...");
    startScan();
}

void handleClientButtons() {
    if (connectionState.getState() != STATE_CONNECTED || millis() - lastButtonPressMillis < DEBOUNCE_DELAY) {
        return;
    }
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (digitalRead(BUTTON_PINS[i]) == LOW) {
            lastButtonPressMillis = millis();
            if (pRemoteRxCharacteristic && pRemoteRxCharacteristic->canWrite()) {
                // Create base message
                char baseMsg[64];
                snprintf(baseMsg, sizeof(baseMsg), "%s:%d:%s", unitName.c_str(), i + 1, BUTTON_LABELS[i]);

                // Generate HMAC
                uint8_t hmac[HMAC_SIZE];
                char passkeyStr[16];
                snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", (unsigned long)currentPasskey);

                if (MessageAuth::generateHMAC(baseMsg, passkeyStr, hmac, HMAC_SIZE)) {
                    // Convert HMAC to hex and append
                    char hmacHex[HMAC_HEX_SIZE + 1];
                    MessageAuth::toHex(hmac, HMAC_SIZE, hmacHex, sizeof(hmacHex));

                    char authenticatedMsg[MAX_MESSAGE_SIZE];
                    snprintf(authenticatedMsg, sizeof(authenticatedMsg), "%s:%s", baseMsg, hmacHex);

                    // Send authenticated message
                    pRemoteRxCharacteristic->writeValue(std::string(authenticatedMsg), false);
                    addToHistory(baseMsg);  // Store base message in history

                    char sentMsg[32];
                    snprintf(sentMsg, sizeof(sentMsg), "Sent: %s", baseMsg);
                    oledPrint(sentMsg, "Connected to Server");
                    lastSentMsg = String(baseMsg);

                    // Store for potential retry
                    strncpy(pendingMessage.message, baseMsg, sizeof(pendingMessage.message) - 1);
                    pendingMessage.message[sizeof(pendingMessage.message) - 1] = '\0';
                    pendingMessage.lastRetryTime = millis();
                    pendingMessage.retryCount = 0;
                    pendingMessage.active = true;

                    // Trigger TX indicators
                    ledMgr.flash(LED_MESSAGE_TX, 200);
                    buzzerMgr.play(BUZZ_BUTTON);
                } else {
                    oledPrint("HMAC failed", "Cannot send");
                    buzzerMgr.play(BUZZ_ERROR);
                }
            }
            break;
        }
    }
}

void handleMessageRetry() {
    if (!pendingMessage.active) return;
    if (connectionState.getState() != STATE_CONNECTED) return;

    unsigned long now = millis();
    if (now - pendingMessage.lastRetryTime < RETRY_INTERVAL_MS) return;

    pendingMessage.retryCount++;

    if (pendingMessage.retryCount > MAX_RETRY_ATTEMPTS) {
        // Max retries exceeded
        Serial.print("Message delivery failed after ");
        Serial.print(MAX_RETRY_ATTEMPTS);
        Serial.println(" retries");

        displayMgr.updateRetryCount(pendingMessage.message, pendingMessage.retryCount);
        oledPrint("Delivery FAILED", "Max retries");
        buzzerMgr.play(BUZZ_ERROR);

        pendingMessage.active = false;
        return;
    }

    // Retry sending message
    Serial.print("Retrying message (attempt ");
    Serial.print(pendingMessage.retryCount);
    Serial.print("/");
    Serial.print(MAX_RETRY_ATTEMPTS);
    Serial.println(")");

    if (pRemoteRxCharacteristic && pRemoteRxCharacteristic->canWrite()) {
        // Generate HMAC for retry
        uint8_t hmac[HMAC_SIZE];
        char passkeyStr[16];
        snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", (unsigned long)currentPasskey);

        if (MessageAuth::generateHMAC(pendingMessage.message, passkeyStr, hmac, HMAC_SIZE)) {
            char hmacHex[HMAC_HEX_SIZE + 1];
            MessageAuth::toHex(hmac, HMAC_SIZE, hmacHex, sizeof(hmacHex));

            char authenticatedMsg[MAX_MESSAGE_SIZE];
            snprintf(authenticatedMsg, sizeof(authenticatedMsg), "%s:%s", pendingMessage.message, hmacHex);

            pRemoteRxCharacteristic->writeValue(std::string(authenticatedMsg), false);

            pendingMessage.lastRetryTime = now;

            char statusMsg[32];
            snprintf(statusMsg, sizeof(statusMsg), "Retry %d/%d", pendingMessage.retryCount, MAX_RETRY_ATTEMPTS);
            oledPrint("Resending...", statusMsg);

            displayMgr.updateRetryCount(pendingMessage.message, pendingMessage.retryCount);
        }
    }
}

void clientLoop() {
    // Check watchdog for stale states
    connectionState.checkWatchdog();

    ConnectionState currentState = connectionState.getState();

    // Sync state to DisplayManager
    displayMgr.setConnectionState(currentState);
    displayMgr.setRetryCount(connectionState.getRetryCount());

    // Update LED pattern based on state
    switch (currentState) {
        case STATE_IDLE:
            ledMgr.setPattern(LED_IDLE);
            break;
        case STATE_SCANNING:
            ledMgr.setPattern(LED_SCANNING);
            break;
        case STATE_CONNECTING:
            ledMgr.setPattern(LED_CONNECTING);
            break;
        case STATE_CONNECTED:
            ledMgr.setPattern(LED_CONNECTED);
            break;
        case STATE_DISCONNECTED:
            ledMgr.setPattern(LED_IDLE);
            break;
        case STATE_ERROR:
            ledMgr.setPattern(LED_ERROR);
            break;
    }

    switch (currentState) {
        case STATE_IDLE:
            // Ready to attempt connection if we have a server device
            if (pServerDevice != nullptr) {
                if (connectToServer()) {
                    // Connection initiated, callbacks will handle state transitions
                } else {
                    // Connection attempt failed, start scanning again
                    freeServerDevice();
                    startScan();
                }
            } else {
                // No server device, need to scan
                startScan();
            }
            break;

        case STATE_SCANNING:
            // Waiting for scan to complete (callbacks will update state)
            // Do nothing, callbacks handle this
            break;

        case STATE_CONNECTING:
            // Waiting for connection to establish (callbacks will update state)
            // Do nothing, callbacks handle this
            break;

        case STATE_CONNECTED:
            // Handle button presses when connected
            handleClientButtons();

            // Handle message retry for undelivered messages
            handleMessageRetry();
            break;

        case STATE_DISCONNECTED:
            // Connection lost, wait for backoff delay then retry
            if (millis() - connectionState.getStateTime() >= connectionState.getBackoffDelay()) {
                // Clean up and start scanning again
                freeServerDevice();
                startScan();
            }
            break;

        case STATE_ERROR:
            // Too many failures, restart device
            oledPrint("Fatal error", "Restarting...");
            delay(2000);
            ESP.restart();
            break;
    }

    yield();
}

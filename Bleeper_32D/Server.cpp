#include "Server.h"
#include "MessageAuth.h"

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pTxCharacteristic = nullptr;
bool deviceConnected = false;
char statusLine[32];

// Static callback instances to prevent memory leaks
static class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& /*connInfo*/) override {
    deviceConnected = true;
    int written = snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
    if (written >= 0 && written < (int)sizeof(statusLine)) {
      oledPrint("Client Connected", statusLine);
    }
    beep();
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& /*connInfo*/, int /*reason*/) override {
    deviceConnected = false;
    NimBLEDevice::startAdvertising();
    int written = snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
    if (written >= 0 && written < (int)sizeof(statusLine)) {
      oledPrint("Client Disconnected", statusLine);
    }
  }

  uint32_t onPassKeyDisplay() override {
    char line[32];
    int written = snprintf(line, sizeof(line), "PIN %06lu", (unsigned long)currentPasskey);
    if (written >= 0 && written < (int)sizeof(line)) {
      oledPrint("Share passkey", line);
    }
    return currentPasskey;
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
    if (connInfo.isEncrypted()) {
      int written = snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
      if (written >= 0 && written < (int)sizeof(statusLine)) {
        oledPrint("Secure link", statusLine);
      }
    }
  }
} serverCallbacks;

static class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() == 0 || rxValue.length() >= MAX_MESSAGE_SIZE) {
            return;
        }

        // Extract HMAC from message (last HMAC_HEX_SIZE characters after last colon)
        size_t lastColon = rxValue.rfind(':');
        if (lastColon == std::string::npos || lastColon + HMAC_HEX_SIZE + 1 != rxValue.length()) {
            Serial.println("Server: Invalid message format (no HMAC)");
            oledPrint("SPOOFED MESSAGE", "HMAC missing");
            return;
        }

        std::string baseMsg = rxValue.substr(0, lastColon);
        std::string hmacHexReceived = rxValue.substr(lastColon + 1);

        // Verify HMAC
        uint8_t hmacExpected[HMAC_SIZE];
        char passkeyStr[16];
        snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", (unsigned long)currentPasskey);

        if (!MessageAuth::generateHMAC(baseMsg.c_str(), passkeyStr, hmacExpected, HMAC_SIZE)) {
            Serial.println("Server: HMAC generation failed");
            oledPrint("HMAC error", "Cannot verify");
            return;
        }

        char hmacHexExpected[HMAC_HEX_SIZE + 1];
        MessageAuth::toHex(hmacExpected, HMAC_SIZE, hmacHexExpected, sizeof(hmacHexExpected));

        if (hmacHexReceived != std::string(hmacHexExpected)) {
            Serial.println("Server: HMAC verification FAILED - spoofed message!");
            oledPrint("SPOOFED MESSAGE", "HMAC mismatch");
            return;
        }

        // HMAC verified - broadcast message to all clients
        if (pTxCharacteristic && pServer->getConnectedCount() > 0) {
            pTxCharacteristic->setValue(rxValue);
            pTxCharacteristic->notify(true);

            // Generate authenticated ACK
            size_t firstColon = baseMsg.find(':');
            if (firstColon != std::string::npos) {
                std::string unit = baseMsg.substr(0, firstColon);
                std::string baseAck = std::string("ACK:") + unit;

                // Generate HMAC for ACK
                uint8_t ackHmac[HMAC_SIZE];
                if (MessageAuth::generateHMAC(baseAck.c_str(), passkeyStr, ackHmac, HMAC_SIZE)) {
                    char ackHmacHex[HMAC_HEX_SIZE + 1];
                    MessageAuth::toHex(ackHmac, HMAC_SIZE, ackHmacHex, sizeof(ackHmacHex));

                    std::string authenticatedAck = baseAck + ":" + std::string(ackHmacHex);
                    pTxCharacteristic->setValue(authenticatedAck);
                    pTxCharacteristic->notify(true);
                }
            }
        }

        // Display message (base message without HMAC)
        char incomingMsg[32];
        int written = snprintf(incomingMsg, sizeof(incomingMsg), "Incoming: %s", baseMsg.c_str());
        if (written < 0 || written >= (int)sizeof(incomingMsg)) {
            strcpy(incomingMsg, "Incoming: [msg]");
        }
        written = snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
        if (written >= 0 && written < (int)sizeof(statusLine)) {
            addToHistory(baseMsg.c_str());
            oledPrint(incomingMsg, statusLine);
            beep();
        }
    }
} rxCallbacks;

void setupServer() {
    oledPrint("Starting Server...", unitName.c_str());

    NimBLEDevice::init(unitName.c_str());
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityPasskey(currentPasskey);
    NimBLEDevice::setPower(BLE_TX_POWER);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);  // Use static callback reference

    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        TX_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    NimBLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
        RX_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pRxCharacteristic->setCallbacks(&rxCallbacks);  // Use static callback reference

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->enableScanResponse(true);
    pAdvertising->start();

    snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
    oledPrint("Server Ready", statusLine);
}

void loopServer() {
    if (millis() - lastButtonPressMillis < DEBOUNCE_DELAY) {
        return;
    }
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (digitalRead(BUTTON_PINS[i]) == LOW) {
            lastButtonPressMillis = millis();

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
                if (pTxCharacteristic && pServer->getConnectedCount() > 0) {
                    pTxCharacteristic->setValue(std::string(authenticatedMsg));
                    pTxCharacteristic->notify(true);
                }

                char sentMsg[32];
                snprintf(sentMsg, sizeof(sentMsg), "Sent: %s", baseMsg);
                snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
                addToHistory(baseMsg);
                oledPrint(sentMsg, statusLine);
                beep();
            }
            break;
        }
    }
}

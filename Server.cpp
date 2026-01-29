#include "Server.h"

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pTxCharacteristic = nullptr;
bool deviceConnected = false;
char statusLine[32];

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
        deviceConnected = true;
        snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
        oledPrint("Client Connected", statusLine);
        beep();
    }
    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        NimBLEDevice::startAdvertising();
        snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
        oledPrint("Client Disconnected", statusLine);
    }
};

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            if (pTxCharacteristic && pServer->getConnectedCount() > 0) {
                pTxCharacteristic->setValue(rxValue);
                pTxCharacteristic->notify(true);
                size_t colon = rxValue.find(':');
                if (colon != std::string::npos) {
                    std::string unit = rxValue.substr(0, colon);
                    std::string ack = std::string("ACK:") + unit;
                    pTxCharacteristic->setValue(ack);
                    pTxCharacteristic->notify(true);
                }
            }
            char incomingMsg[32];
            snprintf(incomingMsg, sizeof(incomingMsg), "Incoming: %s", rxValue.c_str());
            snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
            addToHistory(rxValue.c_str());
            oledPrint(incomingMsg, statusLine);
            beep();
        }
    }
};

void setupServer() {
    oledPrint("Starting Server...", unitName.c_str());

    NimBLEDevice::init(unitName.c_str());
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityPasskey(currentPasskey);
    NimBLEDevice::setPower(BLE_TX_POWER);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        TX_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    NimBLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
        RX_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new CharacteristicCallbacks());

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
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

            char msg[32];
            snprintf(msg, sizeof(msg), "%s:%d:%s", unitName.c_str(), i + 1, BUTTON_LABELS[i]);

            if (pTxCharacteristic && pServer->getConnectedCount() > 0) {
                pTxCharacteristic->setValue(std::string(msg));
                pTxCharacteristic->notify(true);
            }

            char sentMsg[32];
            snprintf(sentMsg, sizeof(sentMsg), "Sent: %s", msg);
            snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
            addToHistory(msg);
            oledPrint(sentMsg, statusLine);
            beep();
            break;
        }
    }
}

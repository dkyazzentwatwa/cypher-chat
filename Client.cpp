#include "Client.h"

#ifndef ROLE_SERVER

NimBLEScan* pScan = nullptr;
NimBLERemoteCharacteristic* pTxCharacteristic = nullptr;
NimBLERemoteCharacteristic* pRxCharacteristic = nullptr;
static NimBLEAdvertisedDevice* pServerDevice = nullptr;

volatile bool connected = false;
volatile bool doConnect = false;
volatile bool isScanning = false;

static void freeServerDevice() {
    if (pServerDevice) {
        delete pServerDevice;
        pServerDevice = nullptr;
    }
}

void notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    char msg[length + 1];
    memcpy(msg, pData, length);
    msg[length] = '\0';
    char incomingMsg[32];
    snprintf(incomingMsg, sizeof(incomingMsg), "Incoming: %s", msg);
    oledPrint(incomingMsg, "Connected to Server");
    beep();
}

class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) {
        connected = true;
        oledPrint("Connected to Server", "Discovering services...");
    }
    void onDisconnect(NimBLEClient* pClient) {
        connected = false;
        freeServerDevice();
        pTxCharacteristic = nullptr;
        pRxCharacteristic = nullptr;
        oledPrint("Disconnected", "Will restart scan...");
    }
};

class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        if (advertisedDevice->isAdvertisingService(NimBLEUUID(SERVICE_UUID))) {
            pScan->stop();
            freeServerDevice();
            pServerDevice = new NimBLEAdvertisedDevice(*advertisedDevice);
            doConnect = true;
        }
    }
};

void scanEnded(NimBLEScanResults results) {
    isScanning = false;
    if (!connected && !doConnect) {
        oledPrint("Server not found", "Retrying...");
    }
}

bool connectToServer() {
    NimBLEClient* pClient = nullptr;

    if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
        oledPrint("Max connections!", "Rebooting...");
        delay(1000);
        ESP.restart();
    }

    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCallbacks(), false);
    pClient->setConnectionParams(12, 12, 0, 51);
    pClient->setConnectTimeout(5);

    if (!pClient->connect(pServerDevice)) {
        NimBLEDevice::deleteClient(pClient);
        return false;
    }

    NimBLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
        pClient->disconnect();
        return false;
    }

    pTxCharacteristic = pRemoteService->getCharacteristic(TX_CHARACTERISTIC_UUID);
    pRxCharacteristic = pRemoteService->getCharacteristic(RX_CHARACTERISTIC_UUID);

    if (pTxCharacteristic == nullptr || pRxCharacteristic == nullptr) {
        pClient->disconnect();
        return false;
    }

    if (pTxCharacteristic->canNotify()) {
        pTxCharacteristic->subscribe(true, notifyCallback);
    }

    return true;
}

void startScan() {
    if (isScanning) return;
    isScanning = true;
    oledPrint("Scanning for server", UNIT_NAME);
    pScan->start(5, scanEnded, false);
}

void setupClient() {
    char splash[32];
    snprintf(splash, sizeof(splash), "Starting Client %s", UNIT_NAME);
    oledPrint(splash);

    NimBLEDevice::init(UNIT_NAME);
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityPasskey(PASSKEY);
    NimBLEDevice::setPower(BLE_TX_POWER);
    pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);

    startScan();
}

void handleClientButtons() {
    if (!connected || millis() - lastButtonPressMillis < DEBOUNCE_DELAY) {
        return;
    }
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (digitalRead(BUTTON_PINS[i]) == LOW) {
            lastButtonPressMillis = millis();
            if (pRxCharacteristic && pRxCharacteristic->canWrite()) {
                char msg[32];
                snprintf(msg, sizeof(msg), "%s:%d:%s", UNIT_NAME, i + 1, BUTTON_LABELS[i]);
                pRxCharacteristic->writeValue(std::string(msg), false);
                char sentMsg[32];
                snprintf(sentMsg, sizeof(sentMsg), "Sent: %s", msg);
                oledPrint(sentMsg, "Connected to Server");
                beep();
            }
            break;
        }
    }
}

void clientLoop() {
    if (doConnect) {
        doConnect = false;
        if (connectToServer()) {
            // success handled by callback
        } else {
            oledPrint("Connection Failed", "Restarting scan...");
            freeServerDevice();
            startScan();
        }
    } else if (!connected && !isScanning) {
        startScan();
    }

    if (connected) {
        handleClientButtons();
    }
}

#endif // !ROLE_SERVER

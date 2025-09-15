/********************************************************************************
 * BLE Beeper/Messaging System for ESP32
 *
 * A single-file Arduino sketch that implements a simple "beeper" messaging
 * system over Bluetooth Low Energy (BLE). The device can be compiled in one of
 * two roles:
 *
 * 1. SERVER (Hub): A GATT server that accepts connections from multiple clients.
 *    It receives messages from any client and broadcasts them to all other
 *    connected clients. It can also send messages from its own buttons.
 *
 * 2. CLIENT (Unit): A GATT client that scans for and connects to the SERVER.
 *    It can send messages via its buttons and receives/displays all broadcast
 *    messages from the server.
 *
 * This sketch is designed to be production-ready and compiles for ESP32 in the
 * Arduino IDE with no manual refactoring, provided the required libraries are
 * installed.
 *
 * HARDWARE WIRING (Default Pins):
 * - Board: ESP32 Dev Module
 * - Display: SSD1306 128x64 I2C
 *   - SDA: GPIO 21
 *   - SCL: GPIO 22
 *   - I2C Address: 0x3C
 * - Buttons: 4x momentary buttons, active-low (connected to GND)
 *   - BTN1: GPIO 32
 *   - BTN2: GPIO 33
 *   - BTN3: GPIO 25
 *   - BTN4: GPIO 26
 * - Buzzer (Optional): Piezo buzzer, active-high
 *   - BUZZER_PIN: GPIO 27 (connect through a 100-220 Ohm resistor to 3.3V)
 *
 * REQUIRED ARDUINO LIBRARIES:
 * - NimBLE-Arduino by h2zero (https://github.com/h2zero/NimBLE-Arduino)
 * - Adafruit SSD1306 by Adafruit
 * - Adafruit GFX Library by Adafruit
 *
 ********************************************************************************/

//==============================================================================
// CONFIGURATION BLOCK
//==============================================================================

// -- ROLE SELECTION --
// Uncomment the following line to compile this device as the SERVER (hub).
// By default, it compiles as a CLIENT (unit).
// #define ROLE_SERVER

// -- DEVICE-SPECIFIC SETTINGS --
#ifndef ROLE_SERVER
  #define UNIT_NAME "ALPHA" // Unique name for this client unit
#endif

// -- HARDWARE PINS & SETTINGS --
#define I2C_SDA_PIN   21
#define I2C_SCL_PIN   22
#define OLED_ADDR     0x3C
#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_RESET_PIN -1 // -1 if sharing Arduino reset pin

// Button pins (active-low, uses internal pull-ups)
const int BUTTON_PINS[] = {32, 33, 25, 26};
const int NUM_BUTTONS = sizeof(BUTTON_PINS) / sizeof(int);

// Buzzer pin (set to -1 to disable)
#define BUZZER_PIN    27

// -- BUTTON LABELS --
// Keep these short to fit in the BLE payload
const char* BUTTON_LABELS[] = {"ACK", "ENROUTE", "NEED HELP", "ALL GOOD"};

// -- BLE SETTINGS --
#define BLE_TX_POWER  ESP_PWR_LVL_P9 // TX power level (e.g., ESP_PWR_LVL_P9 is +9dBm)

//==============================================================================
// INCLUDES & LIBRARIES
//==============================================================================
#include <NimBLEDevice.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//==============================================================================
// SHARED DEFINITIONS & GLOBALS
//==============================================================================

// BLE Service and Characteristic UUIDs (Nordic UART Service)
#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define RX_CHARACTERISTIC_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e" // Client -> Server (Write)
#define TX_CHARACTERISTIC_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e" // Server -> Client (Notify)

// Display object
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);

// Button debounce timer
unsigned long lastButtonPressMillis = 0;
const int DEBOUNCE_DELAY = 200; // ms

//==============================================================================
// UTILITY FUNCTIONS
//==============================================================================

// Simple buzzer chirp
void beep() {
  #if BUZZER_PIN != -1
    digitalWrite(BUZZER_PIN, HIGH);
    delay(20);
    digitalWrite(BUZZER_PIN, LOW);
  #endif
}

// Print two lines of text to the OLED, clearing it first
void oledPrint(const char* line1, const char* line2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  if (strlen(line2) > 0) {
    display.setCursor(0, 10);
    display.println(line2);
  }
  display.display();
}

//==============================================================================
// ROLE-SPECIFIC CODE: SERVER
//==============================================================================
#ifdef ROLE_SERVER

#define DEVICE_NAME "SERVER"
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pTxCharacteristic = nullptr;
bool deviceConnected = false;
char statusLine[32];

// Callback class for server events (connect/disconnect)
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
        deviceConnected = true;
        snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
        oledPrint("Client Connected", statusLine);
        beep();
    };

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        // Keep advertising after a client disconnects
        NimBLEDevice::startAdvertising();
        snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
        oledPrint("Client Disconnected", statusLine);
    }
};

// Callback class for characteristic events (receiving data)
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();

        if (rxValue.length() > 0) {
            // Echo the received message to all connected clients
            if (pTxCharacteristic && pServer->getConnectedCount() > 0) {
                pTxCharacteristic->setValue(rxValue);
                pTxCharacteristic->notify(true); // Use notify for all clients
            }

            // Display on server's OLED
            char incomingMsg[32];
            snprintf(incomingMsg, sizeof(incomingMsg), "Incoming: %s", rxValue.c_str());
            snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
            oledPrint(incomingMsg, statusLine);
            beep();
        }
    }
};

void setupServer() {
    oledPrint("Starting Server...", "BLEEP-SERVER");

    NimBLEDevice::init("BLEEP-SERVER");
    NimBLEDevice::setPower(BLE_TX_POWER);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    // Create TX Characteristic (Server -> Client)
    pTxCharacteristic = pService->createCharacteristic(
        TX_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // Create RX Characteristic (Client -> Server)
    NimBLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
        RX_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new CharacteristicCallbacks());

    pService->start();

    // Start advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
    oledPrint("Server Ready", statusLine);
}

void handleServerButtons() {
    if (millis() - lastButtonPressMillis < DEBOUNCE_DELAY) {
        return;
    }

    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (digitalRead(BUTTON_PINS[i]) == LOW) {
            lastButtonPressMillis = millis();

            char msg[32];
            snprintf(msg, sizeof(msg), "%s:%d:%s", DEVICE_NAME, i + 1, BUTTON_LABELS[i]);

            // Send to all connected clients
            if (pTxCharacteristic && pServer->getConnectedCount() > 0) {
                pTxCharacteristic->setValue(std::string(msg));
                pTxCharacteristic->notify(true);
            }

            char sentMsg[32];
            snprintf(sentMsg, sizeof(sentMsg), "Sent: %s", msg);
            snprintf(statusLine, sizeof(statusLine), "%d client(s)", pServer->getConnectedCount());
            oledPrint(sentMsg, statusLine);
            beep();
            break; // Handle one button at a time
        }
    }
}

#endif // ROLE_SERVER

//==============================================================================
// ROLE-SPECIFIC CODE: CLIENT
//==============================================================================
#ifndef ROLE_SERVER

NimBLEScan* pScan = nullptr;
NimBLERemoteCharacteristic* pTxCharacteristic = nullptr;
NimBLERemoteCharacteristic* pRxCharacteristic = nullptr;
static NimBLEAdvertisedDevice* pServerDevice = nullptr;

volatile bool connected = false;
volatile bool doConnect = false;
volatile bool isScanning = false;

// Notification callback for when the server sends a message
void notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    char msg[length + 1];
    memcpy(msg, pData, length);
    msg[length] = '\0';

    char incomingMsg[32];
    snprintf(incomingMsg, sizeof(incomingMsg), "Incoming: %s", msg);
    oledPrint(incomingMsg, "Connected to Server");
    beep();
}

// Callback class for client events (connect/disconnect)
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) {
        connected = true;
        oledPrint("Connected to Server", "Discovering services...");
    }

    void onDisconnect(NimBLEClient* pClient) {
        connected = false;
        pServerDevice = nullptr;
        pTxCharacteristic = nullptr;
        pRxCharacteristic = nullptr;
        oledPrint("Disconnected", "Will restart scan...");
        // The main loop will handle restarting the scan
    }
};

// Find the server via its advertised service UUID
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        if (advertisedDevice->isAdvertisingService(NimBLEUUID(SERVICE_UUID))) {
            pScan->stop();
            pServerDevice = new NimBLEAdvertisedDevice(*advertisedDevice);
            doConnect = true;
            isScanning = false;
        }
    }
};

// Main connection logic
bool connectToServer() {
    NimBLEClient* pClient = nullptr;

    if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
        oledPrint("Max connections!", "Rebooting...");
        delay(1000);
        ESP.restart();
    }

    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCallbacks(), false);
    pClient->setConnectionParams(12, 12, 0, 51); // interval, latency, timeout
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
    pScan->start(0, nullptr, false); // Scan indefinitely, no duplicate filtering
}

void setupClient() {
    char splash[32];
    snprintf(splash, sizeof(splash), "Starting Client...", UNIT_NAME);
    oledPrint(splash);

    NimBLEDevice::init(UNIT_NAME);
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

                pRxCharacteristic->writeValue(std::string(msg), false); // Write No Response

                char sentMsg[32];
                snprintf(sentMsg, sizeof(sentMsg), "Sent: %s", msg);
                oledPrint(sentMsg, "Connected to Server");
                beep();
            }
            break; // Handle one button at a time
        }
    }
}

#endif // !ROLE_SERVER

//==============================================================================
// MAIN SETUP & LOOP
//==============================================================================

void setup() {
    Serial.begin(115200);

    // Initialize I2C and Display
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Don't proceed, loop forever
    }
    display.clearDisplay();
    display.display();

    // Initialize Buttons
    for (int i = 0; i < NUM_BUTTONS; i++) {
        pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    }

    // Initialize Buzzer
    #if BUZZER_PIN != -1
      pinMode(BUZZER_PIN, OUTPUT);
      digitalWrite(BUZZER_PIN, LOW);
    #endif

    // Role-specific setup
    #ifdef ROLE_SERVER
      setupServer();
    #else
      setupClient();
    #endif
}

void loop() {
    #ifdef ROLE_SERVER
        handleServerButtons();
    #else
        // Handle connection logic
        if (doConnect) {
            doConnect = false;
            if (connectToServer()) {
                // Success handled by callback
            } else {
                oledPrint("Connection Failed", "Restarting scan...");
                pServerDevice = nullptr; // Clear device to allow re-scanning
                startScan();
            }
        } else if (!connected && !isScanning) {
            // If we disconnected, start scanning again
            startScan();
        }

        // Handle button presses only when connected
        if (connected) {
            handleClientButtons();
        }
    #endif

    delay(10); // Low-noise loop
}

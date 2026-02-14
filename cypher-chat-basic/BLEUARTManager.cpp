#include "BLEUARTManager.h"

#if BLE_UART_ENABLED

// Nordic UART Service (NUS) UUIDs - standard for BLE serial
#define BLE_UART_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_UART_RX_CHARACTERISTIC   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // Write from client
#define BLE_UART_TX_CHARACTERISTIC   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // Notify to client

// Global instance
BLEUARTManager bleUARTMgr;

BLEUARTManager::BLEUARTManager()
  : _pServer(nullptr)
  , _pService(nullptr)
  , _pTxCharacteristic(nullptr)
  , _pRxCharacteristic(nullptr)
  , _pAdvertising(nullptr)
  , _connected(false)
  , _rxHead(0)
  , _rxTail(0) {
  _deviceName[0] = '\0';
  _clientName[0] = '\0';
}

bool BLEUARTManager::begin(const char* deviceName) {
  if (!deviceName || strlen(deviceName) == 0) {
    return false;
  }

  strncpy(_deviceName, deviceName, sizeof(_deviceName) - 1);
  _deviceName[sizeof(_deviceName) - 1] = '\0';

  // Initialize NimBLE device
  NimBLEDevice::init(_deviceName);

  // Enable BLE security: bonding + MITM protection + secure connections
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityPasskey(123456);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

  // Create BLE Server
  _pServer = NimBLEDevice::createServer();
  if (!_pServer) {
    return false;
  }

  _pServer->setCallbacks(new ServerCallbacks(this));

  // Create UART Service
  _pService = _pServer->createService(BLE_UART_SERVICE_UUID);
  if (!_pService) {
    return false;
  }

  // Create TX Characteristic (notify to client)
  _pTxCharacteristic = _pService->createCharacteristic(
    BLE_UART_TX_CHARACTERISTIC,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::NOTIFY
  );

  // Create RX Characteristic (write from client)
  _pRxCharacteristic = _pService->createCharacteristic(
    BLE_UART_RX_CHARACTERISTIC,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_NR
  );

  _pRxCharacteristic->setCallbacks(new RxCallbacks(this));

  // Start the service
  _pService->start();

  // Start advertising
  _pAdvertising = NimBLEDevice::getAdvertising();
  _pAdvertising->addServiceUUID(BLE_UART_SERVICE_UUID);

  NimBLEDevice::startAdvertising();

  Serial.printf("BLE UART initialized: %s\n", _deviceName);
  Serial.println("  - Compatible with iPhone (nRF Connect, Bluefruit apps)");
  Serial.println("  - Nordic UART Service (NUS) active");

  return true;
}

void BLEUARTManager::end() {
  if (_pAdvertising) {
    _pAdvertising->stop();
  }

  NimBLEDevice::deinit(true);

  _pServer = nullptr;
  _pService = nullptr;
  _pTxCharacteristic = nullptr;
  _pRxCharacteristic = nullptr;
  _pAdvertising = nullptr;
  _connected = false;
}

bool BLEUARTManager::isConnected() const {
  return _connected;
}

int BLEUARTManager::available() {
  if (_rxHead >= _rxTail) {
    return _rxHead - _rxTail;
  } else {
    return sizeof(_rxBuffer) - _rxTail + _rxHead;
  }
}

int BLEUARTManager::read() {
  if (_rxHead == _rxTail) {
    return -1;  // Buffer empty
  }

  uint8_t c = _rxBuffer[_rxTail];
  _rxTail = (_rxTail + 1) % sizeof(_rxBuffer);
  return c;
}

size_t BLEUARTManager::write(uint8_t c) {
  return write(&c, 1);
}

size_t BLEUARTManager::write(const uint8_t* buffer, size_t size) {
  if (!_connected || !_pTxCharacteristic || !buffer || size == 0) {
    return 0;
  }

  // BLE has a max packet size, send in chunks if needed
  const size_t MAX_CHUNK = 64;  // Balanced for compatibility and performance
  size_t sent = 0;

  while (sent < size) {
    size_t chunkSize = min(MAX_CHUNK, size - sent);

    _pTxCharacteristic->setValue(buffer + sent, chunkSize);
    _pTxCharacteristic->notify();

    sent += chunkSize;

    // Small delay between chunks to avoid overwhelming client
    if (sent < size) {
      delay(10);
    }
  }

  return sent;
}

const char* BLEUARTManager::getClientName() const {
  return _clientName;
}

void BLEUARTManager::onConnect(const char* clientAddr) {
  _connected = true;
  strncpy(_clientName, clientAddr, sizeof(_clientName) - 1);
  _clientName[sizeof(_clientName) - 1] = '\0';

  Serial.printf("+ BLE UART client connected: %s\n", clientAddr);

  // Send welcome message
  const char* welcome = "\r\nConnected to ";
  write((const uint8_t*)welcome, strlen(welcome));
  write((const uint8_t*)_deviceName, strlen(_deviceName));
  write((const uint8_t*)"\r\n", 2);
}

void BLEUARTManager::onDisconnect() {
  _connected = false;

  Serial.printf("- BLE UART client disconnected: %s\n", _clientName);

  _clientName[0] = '\0';

  // Clear RX buffer
  _rxHead = 0;
  _rxTail = 0;

  // Restart advertising
  NimBLEDevice::startAdvertising();
}

void BLEUARTManager::onReceive(const uint8_t* data, size_t len) {
  Serial.printf("[BLE] onReceive: %d bytes -> buffer\n", len);
  for (size_t i = 0; i < len; i++) {
    addToRxBuffer(data[i]);
  }
}

void BLEUARTManager::addToRxBuffer(uint8_t byte) {
  size_t nextHead = (_rxHead + 1) % sizeof(_rxBuffer);

  if (nextHead == _rxTail) {
    // Buffer full, drop oldest byte
    _rxTail = (_rxTail + 1) % sizeof(_rxBuffer);
  }

  _rxBuffer[_rxHead] = byte;
  _rxHead = nextHead;
}

// ServerCallbacks implementation
void BLEUARTManager::ServerCallbacks::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
  char addr[18];
  snprintf(addr, sizeof(addr), "%s", connInfo.getAddress().toString().c_str());
  _manager->onConnect(addr);
}

void BLEUARTManager::ServerCallbacks::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
  _manager->onDisconnect();
}

// RxCallbacks implementation
void BLEUARTManager::RxCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
  std::string value = pCharacteristic->getValue();
  Serial.printf("[BLE] RX callback: %d bytes\n", value.length());
  if (value.length() > 0) {
    _manager->onReceive((const uint8_t*)value.data(), value.length());
  }
}

#endif // BLE_UART_ENABLED

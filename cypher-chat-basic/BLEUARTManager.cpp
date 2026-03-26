#include "BLEUARTManager.h"
#include <Preferences.h>
#include <esp32-hal-bt.h>

#if BLE_UART_ENABLED

// Nordic UART Service (NUS) UUIDs - standard for BLE serial
#define BLE_UART_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_UART_RX_CHARACTERISTIC   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // Write from client
#define BLE_UART_TX_CHARACTERISTIC   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // Notify to client

// Global instance
BLEUARTManager bleUARTMgr;
static uint32_t g_blePIN = 0;
static bool isValidPin(uint32_t pin) {
  return pin >= BLE_PIN_MIN && pin <= BLE_PIN_MAX;
}

BLEUARTManager::BLEUARTManager()
  : _pServer(nullptr)
  , _pService(nullptr)
  , _pTxCharacteristic(nullptr)
  , _pRxCharacteristic(nullptr)
  , _pAdvertising(nullptr)
  , _connected(false)
  , _cleanLineMode(BLE_TERMINAL_CLEAN_LINE_MODE)
  , _rxHead(0)
  , _rxTail(0)
  , _txHead(0)
  , _txTail(0)
  , _txDroppedBytes(0)
  , _lastTxFlushMs(0) {
  _deviceName[0] = '\0';
  _clientName[0] = '\0';
}

bool BLEUARTManager::begin(const char* deviceName) {
  if (!deviceName || strlen(deviceName) == 0) {
    return false;
  }

  strncpy(_deviceName, deviceName, sizeof(_deviceName) - 1);
  _deviceName[sizeof(_deviceName) - 1] = '\0';

  // Ensure any previously started BT stack is stopped before NimBLE init.
  if (btStarted()) {
    btStop();
    delay(100);
  }

  // Initialize NimBLE device with a short retry loop.
  bool bleInitOk = false;
  for (int attempt = 1; attempt <= 3; attempt++) {
    if (NimBLEDevice::init(_deviceName)) {
      bleInitOk = true;
      break;
    }
    Serial.printf("BLE UART init attempt %d/3 failed\n", attempt);
    delay(200);
  }
  if (!bleInitOk) {
    Serial.println("BLE UART init failed: NimBLEDevice::init() returned false");
    return false;
  }

  // Load persisted BLE PIN (fallback to transitional default)
  Preferences prefs;
  prefs.begin("bleuart", true);
  uint32_t savedPin = prefs.getULong("pin", BLE_PIN_DEFAULT);
  prefs.end();
  g_blePIN = isValidPin(savedPin) ? savedPin : BLE_PIN_DEFAULT;

  // Enable BLE security: bonding + MITM protection + secure connections
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityPasskey(g_blePIN);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

  Serial.printf("BLE Security: PIN pairing enabled (PIN: %06lu)\n", (unsigned long)g_blePIN);
  if (g_blePIN == BLE_PIN_DEFAULT) {
    Serial.println("BLE Security: default PIN in use, change with `blepin <6digits>`");
  }

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
  // NOTE: WRITE_ENC removed - requires pairing before any data can be received.
  // Most BLE serial apps don't handle pairing well, so we accept unencrypted writes.
  _pRxCharacteristic = _pService->createCharacteristic(
    BLE_UART_RX_CHARACTERISTIC,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
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

  _lastTxFlushMs = millis();

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
  _txHead = 0;
  _txTail = 0;
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

  return queueTx(buffer, size);
}

void BLEUARTManager::pollTx() {
  if (!_connected || !_pTxCharacteristic) {
    return;
  }

  size_t queued = getTxQueuedBytes();
  if (queued == 0) {
    return;
  }

  unsigned long now = millis();
  if (queued < BLE_TX_BURST_MAX && (now - _lastTxFlushMs) < BLE_TX_FLUSH_INTERVAL_MS) {
    return;
  }

  uint16_t mtu = NimBLEDevice::getMTU();
  if (mtu < 23) mtu = 23;
  size_t maxChunk = mtu - 3;
  if (maxChunk < 20) maxChunk = 20;
  if (maxChunk > BLE_TX_BURST_MAX) maxChunk = BLE_TX_BURST_MAX;

  uint8_t chunk[BLE_TX_BURST_MAX];
  size_t flushed = 0;
  int notificationsSent = 0;
  while (flushed < BLE_TX_BURST_MAX) {
    size_t chunkSize = min(maxChunk, BLE_TX_BURST_MAX - flushed);
    if (chunkSize == 0) break;

    size_t copied = 0;
    portENTER_CRITICAL(&_txMux);
    while (copied < chunkSize && _txTail != _txHead) {
      chunk[copied++] = _txBuffer[_txTail];
      _txTail = (_txTail + 1) % sizeof(_txBuffer);
    }
    portEXIT_CRITICAL(&_txMux);

    if (copied == 0) {
      break;
    }

    _pTxCharacteristic->setValue(chunk, copied);
    bool ok = _pTxCharacteristic->notify();
    if (!ok) {
      // NimBLE queue full - stop and retry next poll
#if BLE_RX_DEBUG
      Serial.printf("[BLE] notify failed (queue full), retry later\n");
#endif
      break;
    }
    flushed += copied;
    notificationsSent++;

    // Rate limit: iOS CoreBluetooth needs ~20ms between notifications
    // Send max 3 notifications per poll cycle to avoid overwhelming stack
    if (notificationsSent >= 3) {
      break;
    }
    delay(5);  // Small delay between notifications
  }

  _lastTxFlushMs = now;
}

void BLEUARTManager::setCleanLineMode(bool enabled) {
  _cleanLineMode = enabled;
}

size_t BLEUARTManager::getTxQueuedBytes() const {
  size_t queued = 0;
  portENTER_CRITICAL((portMUX_TYPE*)&_txMux);
  queued = txQueuedUnsafe();
  portEXIT_CRITICAL((portMUX_TYPE*)&_txMux);
  return queued;
}

size_t BLEUARTManager::getTxDroppedBytes() const {
  size_t dropped = 0;
  portENTER_CRITICAL((portMUX_TYPE*)&_txMux);
  dropped = _txDroppedBytes;
  portEXIT_CRITICAL((portMUX_TYPE*)&_txMux);
  return dropped;
}

const char* BLEUARTManager::getClientName() const {
  return _clientName;
}

uint32_t BLEUARTManager::getPairingPIN() const {
  return g_blePIN;
}

bool BLEUARTManager::setPairingPIN(uint32_t pin) {
  if (!isValidPin(pin)) {
    return false;
  }

  Preferences prefs;
  prefs.begin("bleuart", false);
  prefs.putULong("pin", pin);
  prefs.end();

  g_blePIN = pin;
  if (NimBLEDevice::isInitialized()) {
    NimBLEDevice::setSecurityPasskey(g_blePIN);
  }
  return true;
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
  _txHead = 0;
  _txTail = 0;

  // Restart advertising
  NimBLEDevice::startAdvertising();
}

void BLEUARTManager::onReceive(const uint8_t* data, size_t len) {
#if BLE_RX_DEBUG
  Serial.printf("[BLE] onReceive: %d bytes -> buffer\n", len);
#endif
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
  Serial.printf("[BLE] onConnect: addr=%s sec=%d\n", addr, connInfo.isEncrypted());

  // Update connection parameters for iOS stability
  // iOS works best with: min=15ms, max=30ms, latency=0, timeout=400ms
  // These are in 1.25ms units: 12=15ms, 24=30ms
  pServer->updateConnParams(connInfo.getConnHandle(), 12, 24, 0, 400);

  _manager->onConnect(addr);
}

void BLEUARTManager::ServerCallbacks::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
  _manager->onDisconnect();
}

// RxCallbacks implementation
void BLEUARTManager::RxCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
  std::string value = pCharacteristic->getValue();
#if BLE_RX_DEBUG
  Serial.printf("[BLE] RX callback: %d bytes\n", value.length());
#endif
  if (value.length() > 0) {
    _manager->onReceive((const uint8_t*)value.data(), value.length());
  }
}

size_t BLEUARTManager::queueTx(const uint8_t* data, size_t len) {
  if (!data || len == 0) {
    return 0;
  }

  size_t queued = 0;
  portENTER_CRITICAL(&_txMux);
  for (size_t i = 0; i < len; i++) {
    size_t nextHead = (_txHead + 1) % sizeof(_txBuffer);
    if (nextHead == _txTail) {
      _txDroppedBytes += (len - i);
      break;
    }
    _txBuffer[_txHead] = data[i];
    _txHead = nextHead;
    queued++;
  }
  portEXIT_CRITICAL(&_txMux);
  return queued;
}

size_t BLEUARTManager::txQueuedUnsafe() const {
  if (_txHead >= _txTail) {
    return _txHead - _txTail;
  }
  return sizeof(_txBuffer) - _txTail + _txHead;
}

#endif // BLE_UART_ENABLED

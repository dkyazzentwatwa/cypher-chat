#include "BluetoothSerialManager.h"

#if BT_SERIAL_ENABLED

// Global instance
BluetoothSerialManager btSerialMgr;

// Static member initialization
BluetoothSerialManager* BluetoothSerialManager::_instance = nullptr;

BluetoothSerialManager::BluetoothSerialManager()
  : _connected(false) {
  _deviceName[0] = '\0';
  _clientName[0] = '\0';
  _instance = this;
}

bool BluetoothSerialManager::begin(const char* deviceName) {
  if (!deviceName || strlen(deviceName) == 0) {
    Serial.println("ERROR: BT device name cannot be empty");
    return false;
  }

  // Store device name
  strncpy(_deviceName, deviceName, sizeof(_deviceName) - 1);
  _deviceName[sizeof(_deviceName) - 1] = '\0';

  // Initialize BluetoothSerial
  if (!_bt.begin(_deviceName)) {
    Serial.println("ERROR: Bluetooth Serial init failed");
    return false;
  }

  // Register callback for connection events
  _bt.register_callback(btCallback);

  Serial.printf("Bluetooth Serial started: %s\n", _deviceName);
  Serial.println("Ready for pairing (no PIN required)");

  return true;
}

void BluetoothSerialManager::end() {
  _bt.end();
  _connected = false;
  Serial.println("Bluetooth Serial stopped");
}

int BluetoothSerialManager::available() {
  return _bt.available();
}

int BluetoothSerialManager::read() {
  return _bt.read();
}

size_t BluetoothSerialManager::write(uint8_t c) {
  if (!_connected) {
    return 0;  // Silently drop if not connected
  }
  return _bt.write(c);
}

size_t BluetoothSerialManager::write(const uint8_t* buffer, size_t size) {
  if (!_connected || !buffer || size == 0) {
    return 0;  // Silently drop if not connected or invalid
  }
  return _bt.write(buffer, size);
}

bool BluetoothSerialManager::isConnected() const {
  return _connected;
}

const char* BluetoothSerialManager::getClientName() const {
  return _clientName;
}

const char* BluetoothSerialManager::getDeviceName() const {
  return _deviceName;
}

void BluetoothSerialManager::onConnect(const uint8_t* addr) {
  _connected = true;

  // Format client address as name (since BluetoothSerial doesn't provide name)
  if (addr) {
    snprintf(_clientName, sizeof(_clientName),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
  } else {
    strncpy(_clientName, "Unknown", sizeof(_clientName) - 1);
  }

  Serial.printf("✓ Bluetooth client connected: %s\n", _clientName);

  // Send welcome message to BT client
  _bt.println();
  _bt.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  _bt.printf("Connected to %s\n", _deviceName);
  _bt.println("Type 'help' for commands");
  _bt.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  _bt.println();
  _bt.print("> ");
}

void BluetoothSerialManager::onDisconnect() {
  _connected = false;
  Serial.printf("✗ Bluetooth client disconnected: %s\n", _clientName);
  _clientName[0] = '\0';
}

void BluetoothSerialManager::btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
  if (!_instance) return;

  switch (event) {
    case ESP_SPP_OPEN_EVT:
      // Client connected
      if (param && param->open.rem_bda) {
        _instance->onConnect(param->open.rem_bda);
      } else {
        _instance->onConnect(nullptr);
      }
      break;

    case ESP_SPP_CLOSE_EVT:
      // Client disconnected
      _instance->onDisconnect();
      break;

    case ESP_SPP_DATA_IND_EVT:
      // Data received (handled by available()/read())
      break;

    default:
      // Other events (ignore)
      break;
  }
}

#endif // BT_SERIAL_ENABLED

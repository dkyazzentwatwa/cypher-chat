#pragma once

#include <Arduino.h>
#include "Config.h"

#if BLE_UART_ENABLED
#include <NimBLEDevice.h>

/**
 * BLEUARTManager - BLE UART service for iPhone/Android terminal access
 *
 * Provides Nordic UART Service (NUS) for wireless serial communication.
 * Compatible with iPhone apps: nRF Connect, Bluefruit Connect
 * Compatible with Android apps: nRF Connect, Serial Bluetooth Terminal (BLE mode)
 *
 * UUIDs (Nordic UART Service standard):
 * - Service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 * - RX Char: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E (write from client)
 * - TX Char: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E (notify to client)
 */
class BLEUARTManager {
public:
  BLEUARTManager();

  /**
   * Initialize BLE UART service
   * @param deviceName BLE advertised name (e.g., "CYPHER_Server")
   * @return true if successful
   */
  bool begin(const char* deviceName);

  /**
   * Stop BLE UART service
   */
  void end();

  /**
   * Check if client is connected
   * @return true if connected
   */
  bool isConnected() const;

  /**
   * Get number of bytes available to read
   * @return bytes available
   */
  int available();

  /**
   * Read one byte from RX buffer
   * @return byte read, or -1 if none available
   */
  int read();

  /**
   * Write single byte to TX characteristic
   * @param c byte to write
   * @return 1 if successful
   */
  size_t write(uint8_t c);

  /**
   * Write buffer to TX characteristic
   * @param buffer data to write
   * @param size buffer size
   * @return number of bytes written
   */
  size_t write(const uint8_t* buffer, size_t size);

  /**
   * Flush queued BLE TX data (call from main loop)
   */
  void pollTx();

  /**
   * Enable/disable BLE clean line mode (filters control sequences)
   */
  void setCleanLineMode(bool enabled);

  /**
   * Get client device name (if available)
   * @return client name or empty string
   */
  const char* getClientName() const;

  /**
   * Get current BLE pairing PIN
   * @return configured 6-digit PIN
   */
  uint32_t getPairingPIN() const;

  /**
   * Set BLE pairing PIN and persist to NVS
   * @param pin 6-digit PIN (100000-999999)
   * @return true if valid and saved
   */
  bool setPairingPIN(uint32_t pin);

  /**
   * TX queue telemetry
   */
  size_t getTxQueuedBytes() const;
  size_t getTxDroppedBytes() const;

private:
  NimBLEServer* _pServer;
  NimBLEService* _pService;
  NimBLECharacteristic* _pTxCharacteristic;
  NimBLECharacteristic* _pRxCharacteristic;
  NimBLEAdvertising* _pAdvertising;

  bool _connected;
  bool _cleanLineMode;
  char _deviceName[32];
  char _clientName[32];

  // RX buffer for incoming data
  uint8_t _rxBuffer[256];
  size_t _rxHead;
  size_t _rxTail;

  // TX queue for buffered notifications (write can be called from multiple contexts)
  uint8_t _txBuffer[BLE_TX_QUEUE_BYTES];
  size_t _txHead;
  size_t _txTail;
  size_t _txDroppedBytes;
  unsigned long _lastTxFlushMs;
  portMUX_TYPE _txMux = portMUX_INITIALIZER_UNLOCKED;

  // Server callbacks
  class ServerCallbacks : public NimBLEServerCallbacks {
  public:
    ServerCallbacks(BLEUARTManager* manager) : _manager(manager) {}

    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;

  private:
    BLEUARTManager* _manager;
  };

  // RX characteristic callbacks (receive from client)
  class RxCallbacks : public NimBLECharacteristicCallbacks {
  public:
    RxCallbacks(BLEUARTManager* manager) : _manager(manager) {}

    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;

  private:
    BLEUARTManager* _manager;
  };

  // Connection state handlers
  void onConnect(const char* clientAddr);
  void onDisconnect();

  // Data handlers
  void onReceive(const uint8_t* data, size_t len);
  void addToRxBuffer(uint8_t byte);
  size_t queueTx(const uint8_t* data, size_t len);
  size_t txQueuedUnsafe() const;
};

// Global instance
extern BLEUARTManager bleUARTMgr;

#endif // BLE_UART_ENABLED

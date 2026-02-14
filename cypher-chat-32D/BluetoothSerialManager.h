#pragma once

#include <Arduino.h>
#include "Config.h"

#if BT_SERIAL_ENABLED
#include <BluetoothSerial.h>

/**
 * BluetoothSerialManager - Wrapper for ESP32 Bluetooth Serial
 *
 * Provides wireless terminal access via Bluetooth Classic (SPP profile).
 * Manages connection lifecycle and provides simple Serial-like API.
 */
class BluetoothSerialManager {
public:
  BluetoothSerialManager();

  /**
   * Initialize Bluetooth Serial with device name
   * @param deviceName Name visible during pairing (e.g., "CYPHER_Server")
   * @return true if initialization successful
   */
  bool begin(const char* deviceName);

  /**
   * Stop Bluetooth Serial and release resources
   */
  void end();

  /**
   * Check if data is available to read
   * @return Number of bytes available
   */
  int available();

  /**
   * Read a single byte from BT serial
   * @return Byte value or -1 if no data
   */
  int read();

  /**
   * Write a single byte to BT serial
   * @param c Byte to write
   * @return Number of bytes written (1 or 0)
   */
  size_t write(uint8_t c);

  /**
   * Write a buffer to BT serial
   * @param buffer Data to write
   * @param size Buffer size
   * @return Number of bytes written
   */
  size_t write(const uint8_t* buffer, size_t size);

  /**
   * Check if a BT client is connected
   * @return true if client connected
   */
  bool isConnected() const;

  /**
   * Get connected client's device name
   * @return Client name string (empty if not connected)
   */
  const char* getClientName() const;

  /**
   * Get this device's Bluetooth name
   * @return Device name string
   */
  const char* getDeviceName() const;

private:
  BluetoothSerial _bt;
  bool _connected;
  char _deviceName[32];
  char _clientName[32];

  // Connection callbacks
  void onConnect(const uint8_t* addr);
  void onDisconnect();

  // Static callback bridge (BluetoothSerial uses C-style callbacks)
  static void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param);
  static BluetoothSerialManager* _instance;
};

// Global instance
extern BluetoothSerialManager btSerialMgr;

#endif // BT_SERIAL_ENABLED

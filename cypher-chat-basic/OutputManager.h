#pragma once

#include <Arduino.h>
#include "Config_Basic.h"

/**
 * OutputManager - Broadcast output to multiple serial streams
 *
 * Provides a Serial-like API that mirrors output to both USB Serial
 * and Bluetooth Serial simultaneously. This allows users to connect
 * via either USB or Bluetooth and see the same terminal output.
 */
class OutputManager {
public:
  OutputManager();

  /**
   * Initialize output manager
   * Call after Serial.begin() and btSerialMgr.begin()
   */
  void begin();

  /**
   * Print string to all enabled streams
   * @param str String to print
   * @return Number of bytes written to USB stream
   */
  size_t print(const char* str);
  size_t print(char c);
  size_t print(int n);
  size_t print(unsigned int n);
  size_t print(long n);
  size_t print(unsigned long n);
  size_t print(const String& s);
  size_t print(const __FlashStringHelper* f);

  /**
   * Print string with newline to all enabled streams
   * @param str String to print
   * @return Number of bytes written to USB stream
   */
  size_t println(const char* str = "");
  size_t println(char c);
  size_t println(int n);
  size_t println(unsigned int n);
  size_t println(long n);
  size_t println(unsigned long n);
  size_t println(const String& s);
  size_t println(const __FlashStringHelper* f);

  /**
   * Print formatted string to all enabled streams (printf-style)
   * @param format Format string (printf compatible)
   * @param ... Format arguments
   * @return Number of bytes written to USB stream
   */
  size_t printf(const char* format, ...) __attribute__((format(printf, 2, 3)));

  /**
   * Write single byte to all enabled streams
   * @param c Byte to write
   * @return Number of bytes written to USB stream
   */
  size_t write(uint8_t c);

  /**
   * Write buffer to all enabled streams
   * @param buffer Data to write
   * @param size Buffer size
   * @return Number of bytes written to USB stream
   */
  size_t write(const uint8_t* buffer, size_t size);

  /**
   * Enable/disable USB serial output
   * @param enabled true to enable, false to disable
   */
  void setUSBEnabled(bool enabled);

  /**
   * Enable/disable Bluetooth serial output
   * @param enabled true to enable, false to disable
   */
  void setBTEnabled(bool enabled);

  /**
   * Check if USB serial is enabled
   * @return true if USB output enabled
   */
  bool isUSBEnabled() const;

  /**
   * Check if Bluetooth serial is enabled
   * @return true if BT output enabled
   */
  bool isBTEnabled() const;

private:
  bool _usbEnabled;
  bool _btEnabled;
  char _printBuffer[256];  // Buffer for printf formatting
};

// Global instance
extern OutputManager output;

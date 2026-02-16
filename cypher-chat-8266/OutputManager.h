#pragma once

#include <Arduino.h>
#include "Config_8266.h"

/**
 * OutputManager - Serial output for ESP8266
 *
 * Simplified version: USB Serial only (no BLE on ESP8266).
 * Provides the same API as the ESP32 version for code compatibility.
 */
class OutputManager {
public:
  OutputManager();

  void begin();

  size_t print(const char* str);
  size_t print(char c);
  size_t print(int n);
  size_t print(unsigned int n);
  size_t print(long n);
  size_t print(unsigned long n);
  size_t print(const String& s);
  size_t print(const __FlashStringHelper* f);

  size_t println(const char* str = "");
  size_t println(char c);
  size_t println(int n);
  size_t println(unsigned int n);
  size_t println(long n);
  size_t println(unsigned long n);
  size_t println(const String& s);
  size_t println(const __FlashStringHelper* f);

  size_t printf(const char* format, ...) __attribute__((format(printf, 2, 3)));

  size_t write(uint8_t c);
  size_t write(const uint8_t* buffer, size_t size);

  void setUSBEnabled(bool enabled);
  void setBTEnabled(bool enabled);
  bool isUSBEnabled() const;
  bool isBTEnabled() const;

private:
  bool _usbEnabled;
  char _printBuffer[256];
};

// Global instance
extern OutputManager output;

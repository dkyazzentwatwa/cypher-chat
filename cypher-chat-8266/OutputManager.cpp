#include "OutputManager.h"
#include <stdarg.h>

// Global instance
OutputManager output;

OutputManager::OutputManager()
  : _usbEnabled(true) {
  _printBuffer[0] = '\0';
}

void OutputManager::begin() {
  _usbEnabled = true;
}

size_t OutputManager::print(const char* str) {
  if (!str || !_usbEnabled) return 0;
  return Serial.print(str);
}

size_t OutputManager::print(char c) {
  if (_usbEnabled) return Serial.print(c);
  return 0;
}

size_t OutputManager::print(int n) {
  if (_usbEnabled) return Serial.print(n);
  return 0;
}

size_t OutputManager::print(unsigned int n) {
  if (_usbEnabled) return Serial.print(n);
  return 0;
}

size_t OutputManager::print(long n) {
  if (_usbEnabled) return Serial.print(n);
  return 0;
}

size_t OutputManager::print(unsigned long n) {
  if (_usbEnabled) return Serial.print(n);
  return 0;
}

size_t OutputManager::print(const String& s) {
  return print(s.c_str());
}

size_t OutputManager::print(const __FlashStringHelper* f) {
  if (_usbEnabled) return Serial.print(f);
  return 0;
}

size_t OutputManager::println(const char* str) {
  if (str && strlen(str) > 0) {
    print(str);
  }
  if (_usbEnabled) return Serial.println();
  return 0;
}

size_t OutputManager::println(char c) {
  print(c);
  return println();
}

size_t OutputManager::println(int n) {
  print(n);
  return println();
}

size_t OutputManager::println(unsigned int n) {
  print(n);
  return println();
}

size_t OutputManager::println(long n) {
  print(n);
  return println();
}

size_t OutputManager::println(unsigned long n) {
  print(n);
  return println();
}

size_t OutputManager::println(const String& s) {
  print(s);
  return println();
}

size_t OutputManager::println(const __FlashStringHelper* f) {
  print(f);
  return println();
}

size_t OutputManager::printf(const char* format, ...) {
  if (!format) return 0;

  va_list args;
  va_start(args, format);
  int len = vsnprintf(_printBuffer, sizeof(_printBuffer), format, args);
  va_end(args);

  if (len < 0) return 0;
  if ((size_t)len >= sizeof(_printBuffer)) {
    len = sizeof(_printBuffer) - 1;
  }

  return print(_printBuffer);
}

size_t OutputManager::write(uint8_t c) {
  if (_usbEnabled) return Serial.write(c);
  return 0;
}

size_t OutputManager::write(const uint8_t* buffer, size_t size) {
  if (!buffer || size == 0 || !_usbEnabled) return 0;
  return Serial.write(buffer, size);
}

void OutputManager::setUSBEnabled(bool enabled) {
  _usbEnabled = enabled;
}

void OutputManager::setBTEnabled(bool enabled) {
  // No BLE on ESP8266 - no-op
  (void)enabled;
}

bool OutputManager::isUSBEnabled() const {
  return _usbEnabled;
}

bool OutputManager::isBTEnabled() const {
  return false;  // No BLE on ESP8266
}

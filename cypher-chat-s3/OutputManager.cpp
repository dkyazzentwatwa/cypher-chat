#include "OutputManager.h"
#include <stdarg.h>

#if BLE_UART_ENABLED
#include "BLEUARTManager.h"
#endif

// Global instance
OutputManager output;

#if BLE_UART_ENABLED
static void bleWriteAnsiStripped(const char* str) {
  char bleBuffer[80];
  size_t bleLen = 0;

  auto flush = [&]() {
    if (bleLen > 0) {
      bleUARTMgr.write((const uint8_t*)bleBuffer, bleLen);
      bleLen = 0;
    }
  };

  const char* p = str;
  while (*p) {
    if (*p == '\033' || *p == '\x1b') {
      p++;
      if (*p == '[') p++;
      while (*p && !isalpha(*p)) p++;
      if (*p) p++;
      continue;
    }

    bleBuffer[bleLen++] = *p++;
    if (bleLen >= sizeof(bleBuffer)) {
      flush();
    }
  }

  flush();
}

static void bleWriteFlashStripped(const __FlashStringHelper* f) {
  char bleBuffer[80];
  size_t bleLen = 0;

  auto flush = [&]() {
    if (bleLen > 0) {
      bleUARTMgr.write((const uint8_t*)bleBuffer, bleLen);
      bleLen = 0;
    }
  };

  PGM_P p = reinterpret_cast<PGM_P>(f);
  char c;
  while ((c = pgm_read_byte(p++)) != 0) {
    if (c == '\033' || c == '\x1b') {
      c = pgm_read_byte(p++);
      if (c == '[') c = pgm_read_byte(p++);
      while (c && !isalpha(c)) c = pgm_read_byte(p++);
      continue;
    }

    bleBuffer[bleLen++] = c;
    if (bleLen >= sizeof(bleBuffer)) {
      flush();
    }
  }

  flush();
}
#endif

OutputManager::OutputManager()
  : _usbEnabled(true),   // USB always enabled by default (failsafe)
    _btEnabled(true) {    // BT enabled if connected
  _printBuffer[0] = '\0';
}

void OutputManager::begin() {
  // Nothing to initialize - just use Serial and bleUARTMgr directly
  _usbEnabled = true;
  _btEnabled = true;
}

size_t OutputManager::print(const char* str) {
  if (!str) return 0;

  size_t bytesWritten = 0;

  // Write to USB Serial
  if (_usbEnabled) {
    bytesWritten = Serial.print(str);
  }

  // Write to Bluetooth Serial (strip ANSI codes for better mobile compatibility)
#if BLE_UART_ENABLED
  if (_btEnabled && bleUARTMgr.isConnected()) {
    bleWriteAnsiStripped(str);
  }
#endif

  return bytesWritten;
}

size_t OutputManager::print(char c) {
  if (_usbEnabled) Serial.print(c);
#if BLE_UART_ENABLED
  if (_btEnabled && bleUARTMgr.isConnected()) bleUARTMgr.write(c);
#endif
  return 1;
}

size_t OutputManager::print(int n) {
  if (_usbEnabled) Serial.print(n);
#if BLE_UART_ENABLED
  if (_btEnabled && bleUARTMgr.isConnected()) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", n);
    bleUARTMgr.write((const uint8_t*)buf, strlen(buf));
  }
#endif
  return 1;
}

size_t OutputManager::print(unsigned int n) {
  if (_usbEnabled) Serial.print(n);
#if BLE_UART_ENABLED
  if (_btEnabled && bleUARTMgr.isConnected()) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", n);
    bleUARTMgr.write((const uint8_t*)buf, strlen(buf));
  }
#endif
  return 1;
}

size_t OutputManager::print(long n) {
  if (_usbEnabled) Serial.print(n);
#if BLE_UART_ENABLED
  if (_btEnabled && bleUARTMgr.isConnected()) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%ld", n);
    bleUARTMgr.write((const uint8_t*)buf, strlen(buf));
  }
#endif
  return 1;
}

size_t OutputManager::print(unsigned long n) {
  if (_usbEnabled) Serial.print(n);
#if BLE_UART_ENABLED
  if (_btEnabled && bleUARTMgr.isConnected()) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%lu", n);
    bleUARTMgr.write((const uint8_t*)buf, strlen(buf));
  }
#endif
  return 1;
}

size_t OutputManager::print(const String& s) {
  return print(s.c_str());
}

size_t OutputManager::print(const __FlashStringHelper* f) {
  if (_usbEnabled) Serial.print(f);
#if BLE_UART_ENABLED
  if (_btEnabled && bleUARTMgr.isConnected()) {
    bleWriteFlashStripped(f);
  }
#endif
  return 1;
}

size_t OutputManager::println(const char* str) {
  if (str && strlen(str) > 0) {
    print(str);
  }

  size_t bytesWritten = 0;

  // Write newline to USB Serial
  if (_usbEnabled) {
    bytesWritten = Serial.println();
  }

  // Write newline to Bluetooth Serial
#if BLE_UART_ENABLED
  if (_btEnabled && bleUARTMgr.isConnected()) {
    const uint8_t newline[] = {'\r', '\n'};
    bleUARTMgr.write(newline, sizeof(newline));
  }
#endif

  return bytesWritten;
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

  // Format into buffer
  int len = vsnprintf(_printBuffer, sizeof(_printBuffer), format, args);
  va_end(args);

  if (len < 0) {
    return 0;  // Formatting error
  }

  // Truncate if buffer too small
  if ((size_t)len >= sizeof(_printBuffer)) {
    len = sizeof(_printBuffer) - 1;
  }

  // Broadcast formatted string
  return print(_printBuffer);
}

size_t OutputManager::write(uint8_t c) {
  size_t bytesWritten = 0;

  // Write to USB Serial
  if (_usbEnabled) {
    bytesWritten = Serial.write(c);
  }

  // Write to Bluetooth Serial
#if BLE_UART_ENABLED
  if (_btEnabled && bleUARTMgr.isConnected()) {
    bleUARTMgr.write(c);
  }
#endif

  return bytesWritten;
}

size_t OutputManager::write(const uint8_t* buffer, size_t size) {
  if (!buffer || size == 0) return 0;

  size_t bytesWritten = 0;

  // Write to USB Serial
  if (_usbEnabled) {
    bytesWritten = Serial.write(buffer, size);
  }

  // Write to Bluetooth Serial
#if BLE_UART_ENABLED
  if (_btEnabled && bleUARTMgr.isConnected()) {
    bleUARTMgr.write(buffer, size);
  }
#endif

  return bytesWritten;
}

void OutputManager::setUSBEnabled(bool enabled) {
  _usbEnabled = enabled;
}

void OutputManager::setBTEnabled(bool enabled) {
  _btEnabled = enabled;
}

bool OutputManager::isUSBEnabled() const {
  return _usbEnabled;
}

bool OutputManager::isBTEnabled() const {
  return _btEnabled;
}

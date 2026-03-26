#include "OutputManager.h"
#include <stdarg.h>
#include <ctype.h>

#if BLE_UART_ENABLED
#include "BLEUARTManager.h"

static bool isAnsiTerminator(char c) {
  return (c >= '@' && c <= '~');
}

static size_t writeBLESanitizedBytes(const uint8_t* data, size_t len) {
  if (!data || len == 0 || !bleUARTMgr.isConnected()) return 0;
  char bleBuffer[256];
  size_t bleLen = 0;
  size_t written = 0;
  bool inAnsi = false;
  char prevOut = 0;

  for (size_t idx = 0; idx < len; ++idx) {
    char c = (char)data[idx];

    if (inAnsi) {
      if (isAnsiTerminator(c)) {
        inAnsi = false;
      }
      continue;
    }

    if (c == '\033' || c == '\x1b') {
      inAnsi = true;
      continue;
    }

    if (c == '\n') {
      if (prevOut != '\r' && bleLen < sizeof(bleBuffer) - 1) {
        bleBuffer[bleLen++] = '\r';
      }
      if (bleLen < sizeof(bleBuffer) - 1) {
        bleBuffer[bleLen++] = '\n';
        prevOut = '\n';
      }
    } else if (c == '\r' || c == '\t' || (unsigned char)c >= 0x20) {
      if (bleLen < sizeof(bleBuffer) - 1) {
        bleBuffer[bleLen++] = c;
        prevOut = c;
      }
    } else {
      continue;
    }

    if (bleLen >= sizeof(bleBuffer) - 2) {
      written += bleUARTMgr.write((const uint8_t*)bleBuffer, bleLen);
      bleLen = 0;
      prevOut = 0;
    }
  }

  if (bleLen > 0) {
    written += bleUARTMgr.write((const uint8_t*)bleBuffer, bleLen);
  }
  return written;
}

static size_t writeBLESanitized(const char* str) {
  if (!str) return 0;
  return writeBLESanitizedBytes((const uint8_t*)str, strlen(str));
}
#endif

// Global instance
OutputManager output;

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

  // Write to Bluetooth Serial (strip ANSI/control sequences for better mobile compatibility)
#if BLE_UART_ENABLED
  if (_btEnabled && bleUARTMgr.isConnected()) {
    writeBLESanitized(str);
  }
#endif

  return bytesWritten;
}

size_t OutputManager::print(char c) {
  if (_usbEnabled) Serial.print(c);
#if BLE_UART_ENABLED
  if (_btEnabled && bleUARTMgr.isConnected()) {
    char buf[2] = {c, '\0'};
    writeBLESanitized(buf);
  }
#endif
  return 1;
}

size_t OutputManager::print(int n) {
  if (_usbEnabled) Serial.print(n);
#if BLE_UART_ENABLED
  if (_btEnabled && bleUARTMgr.isConnected()) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", n);
    writeBLESanitized(buf);
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
    writeBLESanitized(buf);
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
    writeBLESanitized(buf);
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
    writeBLESanitized(buf);
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
    char bleBuffer[256];
    size_t bleLen = 0;

    PGM_P p = reinterpret_cast<PGM_P>(f);
    char c;
    while ((c = pgm_read_byte(p++)) != 0 && bleLen < sizeof(bleBuffer) - 1) {
      bleBuffer[bleLen++] = c;
    }

    if (bleLen > 0) {
      bleBuffer[bleLen] = '\0';
      writeBLESanitized(bleBuffer);
    }
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
    writeBLESanitized("\r\n");
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
    char buf[2] = {(char)c, '\0'};
    writeBLESanitized(buf);
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
    writeBLESanitizedBytes(buffer, size);
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

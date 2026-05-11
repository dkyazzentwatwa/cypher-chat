#pragma once

#include <Arduino.h>
#include "Config.h"

class UsbConsole {
public:
  void begin(uint32_t baud);

  int available();
  int read();
  bool connected() const;

  size_t write(uint8_t c);
  size_t write(const uint8_t* buffer, size_t size);

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

private:
  bool _started = false;
};

extern UsbConsole usbConsole;

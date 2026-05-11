#include "UsbConsole.h"
#include <stdarg.h>

#if defined(ESP32)
#include "soc/soc_caps.h"
#endif

#if defined(SOC_USB_SERIAL_JTAG_SUPPORTED) && SOC_USB_SERIAL_JTAG_SUPPORTED && \
    (BOARD_PROFILE == BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18 || BOARD_PROFILE == BOARD_PROFILE_WAVESHARE_TOUCH_LCD_147)
#include "HWCDC.h"
#define USB_CONSOLE_HAS_HWCDC 1
static HWCDC hwUsbConsole;
#else
#define USB_CONSOLE_HAS_HWCDC 0
#endif

UsbConsole usbConsole;

void UsbConsole::begin(uint32_t baud) {
  Serial.begin(baud);
#if USB_CONSOLE_HAS_HWCDC
  hwUsbConsole.setTxTimeoutMs(5);
  hwUsbConsole.begin(baud);
#endif
  _started = true;
}

int UsbConsole::available() {
  int count = Serial.available();
#if USB_CONSOLE_HAS_HWCDC
  int hwCount = hwUsbConsole.available();
  if (hwCount > 0) {
    count += hwCount;
  }
#endif
  return count;
}

int UsbConsole::read() {
  if (Serial.available()) {
    return Serial.read();
  }
#if USB_CONSOLE_HAS_HWCDC
  if (hwUsbConsole.available() > 0) {
    return hwUsbConsole.read();
  }
#endif
  return -1;
}

bool UsbConsole::connected() const {
  if (!_started) {
    return false;
  }
#if USB_CONSOLE_HAS_HWCDC
  if (static_cast<bool>(hwUsbConsole)) {
    return true;
  }
#endif
  return static_cast<bool>(Serial);
}

size_t UsbConsole::write(uint8_t c) {
  size_t written = Serial.write(c);
#if USB_CONSOLE_HAS_HWCDC
  written += hwUsbConsole.write(c);
#endif
  return written;
}

size_t UsbConsole::write(const uint8_t* buffer, size_t size) {
  if (!buffer || size == 0) {
    return 0;
  }

  size_t written = Serial.write(buffer, size);
#if USB_CONSOLE_HAS_HWCDC
  written += hwUsbConsole.write(buffer, size);
#endif
  return written;
}

size_t UsbConsole::print(const char* str) {
  if (!str) {
    return 0;
  }
  return write(reinterpret_cast<const uint8_t*>(str), strlen(str));
}

size_t UsbConsole::print(char c) {
  return write(static_cast<uint8_t>(c));
}

size_t UsbConsole::print(int n) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", n);
  return print(buf);
}

size_t UsbConsole::print(unsigned int n) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%u", n);
  return print(buf);
}

size_t UsbConsole::print(long n) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%ld", n);
  return print(buf);
}

size_t UsbConsole::print(unsigned long n) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%lu", n);
  return print(buf);
}

size_t UsbConsole::print(const String& s) {
  return print(s.c_str());
}

size_t UsbConsole::print(const __FlashStringHelper* f) {
  if (!f) {
    return 0;
  }

  size_t written = 0;
  PGM_P p = reinterpret_cast<PGM_P>(f);
  char c;
  while ((c = pgm_read_byte(p++)) != 0) {
    written += write(static_cast<uint8_t>(c));
  }
  return written;
}

size_t UsbConsole::println(const char* str) {
  size_t written = 0;
  if (str && strlen(str) > 0) {
    written += print(str);
  }
  written += write(static_cast<uint8_t>('\r'));
  written += write(static_cast<uint8_t>('\n'));
  return written;
}

size_t UsbConsole::println(char c) {
  size_t written = print(c);
  written += println();
  return written;
}

size_t UsbConsole::println(int n) {
  size_t written = print(n);
  written += println();
  return written;
}

size_t UsbConsole::println(unsigned int n) {
  size_t written = print(n);
  written += println();
  return written;
}

size_t UsbConsole::println(long n) {
  size_t written = print(n);
  written += println();
  return written;
}

size_t UsbConsole::println(unsigned long n) {
  size_t written = print(n);
  written += println();
  return written;
}

size_t UsbConsole::println(const String& s) {
  size_t written = print(s);
  written += println();
  return written;
}

size_t UsbConsole::println(const __FlashStringHelper* f) {
  size_t written = print(f);
  written += println();
  return written;
}

size_t UsbConsole::printf(const char* format, ...) {
  if (!format) {
    return 0;
  }

  char buffer[256];
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (len < 0) {
    return 0;
  }
  if (static_cast<size_t>(len) >= sizeof(buffer)) {
    len = sizeof(buffer) - 1;
  }
  return write(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(len));
}

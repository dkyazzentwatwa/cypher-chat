#pragma once

#include <Arduino.h>
#include <SPI.h>
#if defined(ARDUINO_ARCH_ESP32) && !defined(ESP32)
#define ESP32 1
#endif
#include <Adafruit_GFX.h>

#include "BoardProfiles.h"

#ifndef ST77XX_BLACK
#define ST77XX_BLACK 0x0000
#define ST77XX_BLUE 0x001F
#define ST77XX_RED 0xF800
#define ST77XX_GREEN 0x07E0
#define ST77XX_CYAN 0x07FF
#define ST77XX_MAGENTA 0xF81F
#define ST77XX_YELLOW 0xFFE0
#define ST77XX_WHITE 0xFFFF
#endif

#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_ST7789
#include <Adafruit_ST7789.h>
class WaveshareST7789 : public Adafruit_ST7789 {
public:
  WaveshareST7789(SPIClass *spiClass, int8_t cs, int8_t dc, int8_t rst)
      : Adafruit_ST7789(spiClass, cs, dc, rst) {}

  using Adafruit_ST7789::setColRowStart;
  void applyMadctlFix(uint8_t rotation, bool mirrorX, bool mirrorY);
};
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SSD1306
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SH8601
#include <Adafruit_XCA9554.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>

class ArduinoGfxBridge : public Adafruit_GFX {
public:
  ArduinoGfxBridge(uint16_t w, uint16_t h) : Adafruit_GFX(w, h) {}

  void attach(Arduino_GFX *driver) { _driver = driver; }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void startWrite() override;
  void writePixel(int16_t x, int16_t y, uint16_t color) override;
  void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override;
  void endWrite() override;
  void setRotation(uint8_t rotation) override;
  void invertDisplay(bool inverted) override;
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void fillScreen(uint16_t color) override;
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override;
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;

private:
  Arduino_GFX *_driver = nullptr;
};
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_M5GFX
#include <M5Cardputer.h>

class M5GfxBridge : public Adafruit_GFX {
public:
  M5GfxBridge(uint16_t w, uint16_t h) : Adafruit_GFX(w, h) {}

  void attach(M5GFX *driver) { _driver = driver; }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void startWrite() override;
  void writePixel(int16_t x, int16_t y, uint16_t color) override;
  void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override;
  void endWrite() override;
  void setRotation(uint8_t rotation) override;
  void invertDisplay(bool inverted) override;
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void fillScreen(uint16_t color) override;
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override;
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;

private:
  M5GFX *_driver = nullptr;
};
#endif

class NullGfx : public Adafruit_GFX {
public:
  NullGfx(uint16_t w, uint16_t h) : Adafruit_GFX(w, h) {}
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    (void)x;
    (void)y;
    (void)color;
  }
};

class DisplayPort {
public:
  DisplayPort();
  bool begin();
  Adafruit_GFX &gfx();

  void clear(uint16_t color = 0);
  void drawHeader(const char *title);
  void drawHeaderCentered(const char *title);
  void drawStatus(const String &line1, const String &line2 = String());
  void drawMessage(const char *title, const String &body, const char *footer);
  void drawFooter(const char *text);
  void setBacklight(bool on);
  void setRotation(uint8_t rotation);

  uint16_t width() const;
  uint16_t height() const;
  bool available() const;

private:
#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_ST7789
  WaveshareST7789 _tft;
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SSD1306
  Adafruit_SSD1306 _oled;
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SH8601
  Arduino_DataBus *_amoledBus = nullptr;
  Arduino_SH8601 *_amoled = nullptr;
  ArduinoGfxBridge _amoledGfx;
  Adafruit_XCA9554 _expander;
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_M5GFX
  M5GfxBridge _m5Gfx;
#endif
  NullGfx _nullGfx;
  bool _ready = false;

  void flushIfNeeded();
};

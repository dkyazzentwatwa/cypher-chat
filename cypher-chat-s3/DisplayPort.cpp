#include "DisplayPort.h"

#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_ST7789
#include <Adafruit_ST77xx.h>
#endif

namespace {
constexpr uint16_t COLOR_DARK_GREY = 0x7BEF;
constexpr uint16_t COLOR_LIGHT_GREY = 0xC618;
}

#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SH8601
void ArduinoGfxBridge::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (_driver) {
    _driver->drawPixel(x, y, color);
  }
}

void ArduinoGfxBridge::startWrite() {
  if (_driver) {
    _driver->startWrite();
  }
}

void ArduinoGfxBridge::writePixel(int16_t x, int16_t y, uint16_t color) {
  if (_driver) {
    _driver->writePixel(x, y, color);
  }
}

void ArduinoGfxBridge::writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (_driver) {
    _driver->writeFillRect(x, y, w, h, color);
  }
}

void ArduinoGfxBridge::writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (_driver) {
    _driver->writeFastVLine(x, y, h, color);
  }
}

void ArduinoGfxBridge::writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (_driver) {
    _driver->writeFastHLine(x, y, w, color);
  }
}

void ArduinoGfxBridge::writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                 uint16_t color) {
  if (_driver) {
    _driver->writeLine(x0, y0, x1, y1, color);
  }
}

void ArduinoGfxBridge::endWrite() {
  if (_driver) {
    _driver->endWrite();
  }
}

void ArduinoGfxBridge::setRotation(uint8_t rotation) {
  Adafruit_GFX::setRotation(rotation);
  if (_driver) {
    _driver->setRotation(rotation);
  }
}

void ArduinoGfxBridge::invertDisplay(bool inverted) {
  if (_driver) {
    _driver->invertDisplay(inverted);
  }
}

void ArduinoGfxBridge::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (_driver) {
    _driver->drawFastVLine(x, y, h, color);
  }
}

void ArduinoGfxBridge::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (_driver) {
    _driver->drawFastHLine(x, y, w, color);
  }
}

void ArduinoGfxBridge::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (_driver) {
    _driver->fillRect(x, y, w, h, color);
  }
}

void ArduinoGfxBridge::fillScreen(uint16_t color) {
  if (_driver) {
    _driver->fillScreen(color);
  }
}

void ArduinoGfxBridge::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                uint16_t color) {
  if (_driver) {
    _driver->drawLine(x0, y0, x1, y1, color);
  }
}

void ArduinoGfxBridge::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (_driver) {
    _driver->drawRect(x, y, w, h, color);
  }
}
#endif

#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_M5GFX
void M5GfxBridge::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (_driver) {
    _driver->drawPixel(x, y, color);
  }
}

void M5GfxBridge::startWrite() {
  if (_driver) {
    _driver->startWrite();
  }
}

void M5GfxBridge::writePixel(int16_t x, int16_t y, uint16_t color) {
  if (_driver) {
    _driver->writePixel(x, y, color);
  }
}

void M5GfxBridge::writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                uint16_t color) {
  if (_driver) {
    _driver->writeFillRect(x, y, w, h, color);
  }
}

void M5GfxBridge::writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (_driver) {
    _driver->writeFastVLine(x, y, h, color);
  }
}

void M5GfxBridge::writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (_driver) {
    _driver->writeFastHLine(x, y, w, color);
  }
}

void M5GfxBridge::writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                            uint16_t color) {
  if (_driver) {
    _driver->drawLine(x0, y0, x1, y1, color);
  }
}

void M5GfxBridge::endWrite() {
  if (_driver) {
    _driver->endWrite();
  }
}

void M5GfxBridge::setRotation(uint8_t rotation) {
  Adafruit_GFX::setRotation(rotation);
  if (_driver) {
    _driver->setRotation(rotation);
  }
}

void M5GfxBridge::invertDisplay(bool inverted) {
  if (_driver) {
    _driver->invertDisplay(inverted);
  }
}

void M5GfxBridge::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (_driver) {
    _driver->drawFastVLine(x, y, h, color);
  }
}

void M5GfxBridge::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (_driver) {
    _driver->drawFastHLine(x, y, w, color);
  }
}

void M5GfxBridge::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint16_t color) {
  if (_driver) {
    _driver->fillRect(x, y, w, h, color);
  }
}

void M5GfxBridge::fillScreen(uint16_t color) {
  if (_driver) {
    _driver->fillScreen(color);
  }
}

void M5GfxBridge::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           uint16_t color) {
  if (_driver) {
    _driver->drawLine(x0, y0, x1, y1, color);
  }
}

void M5GfxBridge::drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint16_t color) {
  if (_driver) {
    _driver->drawRect(x, y, w, h, color);
  }
}
#endif

DisplayPort::DisplayPort()
#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_ST7789
    : _tft(&SPI, PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST), _nullGfx(LCD_WIDTH, LCD_HEIGHT) {}
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SSD1306
    : _oled(LCD_WIDTH, LCD_HEIGHT, &Wire, PIN_SSD1306_RST), _nullGfx(LCD_WIDTH, LCD_HEIGHT) {}
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SH8601
    : _amoledGfx(LCD_WIDTH, LCD_HEIGHT), _nullGfx(LCD_WIDTH, LCD_HEIGHT) {}
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_M5GFX
    : _m5Gfx(LCD_WIDTH, LCD_HEIGHT), _nullGfx(LCD_WIDTH, LCD_HEIGHT) {}
#else
    : _nullGfx(128, 64) {}
#endif

bool DisplayPort::begin() {
#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_ST7789
  pinMode(PIN_LCD_BL, OUTPUT);
  setBacklight(true);

  SPI.begin(PIN_LCD_SCLK, -1, PIN_LCD_MOSI, PIN_LCD_CS);
  _tft.init(LCD_WIDTH, LCD_HEIGHT, DISPLAY_SPI_MODE);
  _tft.setColRowStart(DISPLAY_COL_OFFSET, DISPLAY_ROW_OFFSET);
  _tft.setRotation(LCD_ROTATION);
  _tft.applyMadctlFix(LCD_ROTATION, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
  _tft.fillScreen(ST77XX_BLACK);
  _tft.setTextWrap(false);
  _tft.setTextSize(DISPLAY_TEXT_SIZE);
  _tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  _ready = true;
  drawStatus("cypher-chat", "Display init OK");
  return true;
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SSD1306
  Wire.begin(PIN_SSD1306_SDA, PIN_SSD1306_SCL);
  _ready = _oled.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDR);
  if (_ready) {
    _oled.clearDisplay();
    _oled.setTextWrap(false);
    _oled.setTextSize(DISPLAY_TEXT_SIZE);
    _oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    _oled.setRotation(LCD_ROTATION);
    drawStatus("cypher-chat", "Display init OK");
  }
  return _ready;
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SH8601
  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
  if (!_expander.begin(AMOLED_EXPANDER_I2C_ADDR, &Wire)) {
    Serial.println("AMOLED XCA9554 not found; trying display init anyway");
  } else {
    for (uint8_t pin = 0; pin < 3; pin++) {
      _expander.pinMode(pin, OUTPUT);
      _expander.digitalWrite(pin, LOW);
    }
    delay(20);
    for (uint8_t pin = 0; pin < 3; pin++) {
      _expander.digitalWrite(pin, HIGH);
    }
  }

  _amoledBus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_SDIO0,
                                     PIN_LCD_SDIO1, PIN_LCD_SDIO2, PIN_LCD_SDIO3);
  _amoled = new Arduino_SH8601(_amoledBus, PIN_LCD_RST, LCD_ROTATION, LCD_WIDTH, LCD_HEIGHT);
  if (!_amoled->begin()) {
    Serial.println("SH8601 init failed");
    return false;
  }

  _amoled->setBrightness(AMOLED_BRIGHTNESS);
  _amoled->fillScreen(ST77XX_BLACK);
  _amoledGfx.attach(_amoled);
  _amoledGfx.setRotation(LCD_ROTATION);
  _amoledGfx.setTextWrap(false);
  _amoledGfx.setTextSize(DISPLAY_TEXT_SIZE);
  _amoledGfx.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  _ready = true;
  drawStatus("cypher-chat", "Display init OK");
  return true;
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_M5GFX
  auto cfg = M5.config();
  cfg.serial_baudrate = 0;
  cfg.fallback_board = m5::board_t::board_M5CardputerADV;
  cfg.clear_display = true;
  cfg.internal_imu = true;
  cfg.internal_mic = true;
  cfg.internal_spk = true;
  cfg.output_power = true;
  Serial.println("Cardputer display init begin");
  M5Cardputer.begin(cfg, true);
  Serial.printf("Cardputer board=%d display_count=%u width=%d height=%d\n",
                static_cast<int>(M5.getBoard()),
                static_cast<unsigned>(M5.getDisplayCount()),
                M5Cardputer.Display.width(), M5Cardputer.Display.height());
  M5Cardputer.Display.setRotation(LCD_ROTATION);
  M5Cardputer.Display.setBrightness(CARDPUTER_BRIGHTNESS);
  M5Cardputer.Display.fillScreen(ST77XX_BLACK);
  _m5Gfx.attach(&M5Cardputer.Display);
  _m5Gfx.setRotation(LCD_ROTATION);
  Serial.printf("Cardputer geometry driver=%dx%d port=%ux%u\n",
                M5Cardputer.Display.width(), M5Cardputer.Display.height(),
                static_cast<unsigned>(_m5Gfx.width()), static_cast<unsigned>(_m5Gfx.height()));
  _m5Gfx.setTextWrap(false);
  _m5Gfx.setTextSize(DISPLAY_TEXT_SIZE);
  _m5Gfx.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  _ready = true;
  drawStatus("cypher-chat", "Display init OK");
  return true;
#else
  _ready = false;
  return true;
#endif
}

Adafruit_GFX &DisplayPort::gfx() {
#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_ST7789
  return _tft;
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SSD1306
  return _oled;
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SH8601
  return _amoledGfx;
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_M5GFX
  return _m5Gfx;
#else
  return _nullGfx;
#endif
}

void DisplayPort::clear(uint16_t color) {
#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SSD1306
  (void)color;
  _oled.clearDisplay();
#elif BOARD_HAS_DISPLAY
  gfx().fillScreen(color);
#else
  (void)color;
  return;
#endif
  flushIfNeeded();
}

void DisplayPort::drawHeader(const char *title) {
#if !BOARD_HAS_DISPLAY
  (void)title;
  return;
#else
  Adafruit_GFX &d = gfx();
#if BOARD_DISPLAY_IS_SSD1306
  d.fillRect(0, 0, width(), 10, SSD1306_BLACK);
  d.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  d.setCursor(0, 1);
  d.print(title);
#else
  const int headerH = BOARD_DISPLAY_IS_SH8601 ? 34 : 22;
  const int headerTextY = BOARD_DISPLAY_IS_SH8601 ? 9 : 7;
  const int headerTextX = BOARD_DISPLAY_IS_SH8601 ? 14 : 8;
  d.fillRect(0, 0, width(), headerH, ST77XX_BLUE);
  d.setTextColor(ST77XX_WHITE, ST77XX_BLUE);
  d.setCursor(headerTextX, headerTextY);
  d.print(title);
  d.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
#endif
  flushIfNeeded();
#endif
}

void DisplayPort::drawHeaderCentered(const char *title) {
#if !BOARD_HAS_DISPLAY
  (void)title;
  return;
#else
  Adafruit_GFX &d = gfx();
#if BOARD_DISPLAY_IS_SSD1306
  d.fillRect(0, 0, width(), 10, SSD1306_BLACK);
  d.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t textW = 0;
  uint16_t textH = 0;
  d.getTextBounds(title, 0, 0, &x1, &y1, &textW, &textH);
  d.setCursor(max(0, (static_cast<int>(width()) - static_cast<int>(textW)) / 2 - x1), 1);
  d.print(title);
#else
  const int headerH = BOARD_DISPLAY_IS_SH8601 ? 34 : 22;
  const int headerTextY = BOARD_DISPLAY_IS_SH8601 ? 9 : 7;
  d.fillRect(0, 0, width(), headerH, ST77XX_BLUE);
  d.setTextColor(ST77XX_WHITE, ST77XX_BLUE);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t textW = 0;
  uint16_t textH = 0;
  d.getTextBounds(title, 0, 0, &x1, &y1, &textW, &textH);
  d.setCursor(max(0, (static_cast<int>(width()) - static_cast<int>(textW)) / 2 - x1), headerTextY);
  d.print(title);
  d.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
#endif
  flushIfNeeded();
#endif
}

void DisplayPort::drawStatus(const String &line1, const String &line2) {
#if !BOARD_HAS_DISPLAY
  (void)line1;
  (void)line2;
  return;
#else
  clear();
  drawHeader("Status");
  Adafruit_GFX &d = gfx();
#if BOARD_DISPLAY_IS_SSD1306
  d.setCursor(0, 16);
  d.print(line1);
  if (line2.length() > 0) {
    d.setCursor(0, 28);
    d.print(line2);
  }
#else
  d.setCursor(8, 42);
  d.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  d.print(line1);
  if (line2.length() > 0) {
    d.setCursor(8, 58);
    d.print(line2);
  }
#endif
  flushIfNeeded();
#endif
}

void DisplayPort::drawMessage(const char *title, const String &body, const char *footer) {
#if !BOARD_HAS_DISPLAY
  (void)title;
  (void)body;
  (void)footer;
  return;
#else
  clear();
  drawHeader(title);
  Adafruit_GFX &d = gfx();
  int y = BOARD_DISPLAY_IS_SSD1306 ? 12 : (BOARD_DISPLAY_IS_SH8601 ? 48 : 36);
  const int lineStep = BOARD_DISPLAY_IS_SSD1306 ? 10 : (BOARD_DISPLAY_IS_SH8601 ? 22 : 14);
  int start = 0;
  while (start < body.length() &&
         y < static_cast<int>(height() - (BOARD_DISPLAY_IS_SSD1306 ? 10 : (BOARD_DISPLAY_IS_SH8601 ? 36 : 24)))) {
    int end = body.indexOf('\n', start);
    if (end < 0) {
      end = body.length();
    }
    d.setCursor(BOARD_DISPLAY_IS_SSD1306 ? 0 : 6, y);
    d.print(body.substring(start, end));
    y += lineStep;
    start = end + 1;
  }
  drawFooter(footer);
#endif
}

void DisplayPort::drawFooter(const char *text) {
#if !BOARD_HAS_DISPLAY
  (void)text;
  return;
#else
  Adafruit_GFX &d = gfx();
#if BOARD_DISPLAY_IS_SSD1306
  d.fillRect(0, height() - 9, width(), 9, SSD1306_BLACK);
  d.drawFastHLine(0, height() - 10, width(), SSD1306_WHITE);
  d.setCursor(0, height() - 8);
  d.print(text);
#else
  const int footerH = BOARD_DISPLAY_IS_SH8601 ? 30 : 18;
  const int footerTextY = BOARD_DISPLAY_IS_SH8601 ? 22 : 13;
  const int footerTextX = BOARD_DISPLAY_IS_SH8601 ? 14 : 8;
  d.fillRect(0, height() - footerH, width(), footerH, ST77XX_BLACK);
  d.drawFastHLine(0, height() - footerH - 1, width(), COLOR_DARK_GREY);
  d.setCursor(footerTextX, height() - footerTextY);
  d.setTextColor(COLOR_LIGHT_GREY, ST77XX_BLACK);
  d.print(text);
#endif
  flushIfNeeded();
#endif
}

void DisplayPort::setBacklight(bool on) {
#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_ST7789
  digitalWrite(PIN_LCD_BL, on ? HIGH : LOW);
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SH8601
  if (_amoled) {
    _amoled->setBrightness(on ? AMOLED_BRIGHTNESS : 0);
  }
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_M5GFX
  M5Cardputer.Display.setBrightness(on ? CARDPUTER_BRIGHTNESS : 0);
#else
  (void)on;
#endif
}

void DisplayPort::setRotation(uint8_t rotation) {
#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_ST7789
  _tft.setRotation(rotation);
  _tft.applyMadctlFix(rotation, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SSD1306
  _oled.setRotation(rotation);
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SH8601
  _amoledGfx.setRotation(rotation);
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_M5GFX
  _m5Gfx.setRotation(rotation);
#else
  (void)rotation;
#endif
}

uint16_t DisplayPort::width() const {
#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_ST7789
  return _tft.width();
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SSD1306
  return _oled.width();
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SH8601
  return _amoledGfx.width();
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_M5GFX
  return _m5Gfx.width();
#else
  return _nullGfx.width();
#endif
}

uint16_t DisplayPort::height() const {
#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_ST7789
  return _tft.height();
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SSD1306
  return _oled.height();
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SH8601
  return _amoledGfx.height();
#elif BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_M5GFX
  return _m5Gfx.height();
#else
  return _nullGfx.height();
#endif
}

bool DisplayPort::available() const {
  return BOARD_HAS_DISPLAY && _ready;
}

void DisplayPort::flushIfNeeded() {
#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_SSD1306
  if (_ready) {
    _oled.display();
  }
#endif
}

#if BOARD_HAS_DISPLAY && BOARD_DISPLAY_IS_ST7789
void WaveshareST7789::applyMadctlFix(uint8_t rotation, bool mirrorX, bool mirrorY) {
  uint8_t madctl = 0;
  switch (rotation % 4) {
  case 0:
    madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_MY | ST77XX_MADCTL_RGB;
    break;
  case 1:
    madctl = ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB;
    break;
  case 2:
    madctl = ST77XX_MADCTL_RGB;
    break;
  case 3:
    madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB;
    break;
  }

  if (mirrorX) {
    madctl ^= ST77XX_MADCTL_MX;
  }
  if (mirrorY) {
    madctl ^= ST77XX_MADCTL_MY;
  }

  sendCommand(ST77XX_MADCTL, &madctl, 1);
}
#endif

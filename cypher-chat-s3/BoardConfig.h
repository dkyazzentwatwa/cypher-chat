#pragma once

#include <Arduino.h>

#define BOARD_PROFILE_CARDPUTER_ADV 1
#define BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18 2
#define BOARD_PROFILE_WAVESHARE_TOUCH_LCD_147 3

#ifndef BOARD_PROFILE
#define BOARD_PROFILE BOARD_PROFILE_CARDPUTER_ADV
#endif

#if BOARD_PROFILE == BOARD_PROFILE_CARDPUTER_ADV
#define BOARD_HAS_DISPLAY 1
#define BOARD_HAS_TOUCH 0
#define BOARD_HAS_CARDPUTER_INPUT 1
#define BOARD_HAS_TOUCH_INPUT 0
#define BOARD_DISPLAY_IS_ST7789 0
#define BOARD_DISPLAY_IS_SSD1306 0
#define BOARD_DISPLAY_IS_SH8601 0
#define BOARD_DISPLAY_IS_M5GFX 1
#define BOARD_TOUCH_IS_AXS5106L 0
#define BOARD_TOUCH_IS_FT3168 0
#define BOARD_HAS_AXP2101 0
#define BOARD_HAS_M5_POWER 1
constexpr const char *BOARD_PROFILE_NAME = "cardputer-adv";
#elif BOARD_PROFILE == BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18
#define BOARD_HAS_DISPLAY 1
#define BOARD_HAS_TOUCH 1
#define BOARD_HAS_CARDPUTER_INPUT 0
#define BOARD_HAS_TOUCH_INPUT 1
#define BOARD_DISPLAY_IS_ST7789 0
#define BOARD_DISPLAY_IS_SSD1306 0
#define BOARD_DISPLAY_IS_SH8601 1
#define BOARD_DISPLAY_IS_M5GFX 0
#define BOARD_TOUCH_IS_AXS5106L 0
#define BOARD_TOUCH_IS_FT3168 1
#define BOARD_HAS_AXP2101 1
#define BOARD_HAS_M5_POWER 0
constexpr const char *BOARD_PROFILE_NAME = "waveshare-touch-amoled-1.8";
#elif BOARD_PROFILE == BOARD_PROFILE_WAVESHARE_TOUCH_LCD_147
#define BOARD_HAS_DISPLAY 1
#define BOARD_HAS_TOUCH 1
#define BOARD_HAS_CARDPUTER_INPUT 0
#define BOARD_HAS_TOUCH_INPUT 1
#define BOARD_DISPLAY_IS_ST7789 1
#define BOARD_DISPLAY_IS_SSD1306 0
#define BOARD_DISPLAY_IS_SH8601 0
#define BOARD_DISPLAY_IS_M5GFX 0
#define BOARD_TOUCH_IS_AXS5106L 1
#define BOARD_TOUCH_IS_FT3168 0
#define BOARD_HAS_AXP2101 0
#define BOARD_HAS_M5_POWER 0
constexpr const char *BOARD_PROFILE_NAME = "waveshare-touch-lcd-1.47";
#else
#error "Unsupported BOARD_PROFILE"
#endif

constexpr uint32_t SERIAL_BAUD = 115200;

#ifndef ARDUINO_USB_MODE
#define ARDUINO_USB_MODE 0
#endif
#ifndef ARDUINO_USB_CDC_ON_BOOT
#define ARDUINO_USB_CDC_ON_BOOT 1
#endif

#if BOARD_PROFILE == BOARD_PROFILE_CARDPUTER_ADV
constexpr uint16_t LCD_WIDTH = 240;
constexpr uint16_t LCD_HEIGHT = 135;
constexpr uint8_t LCD_ROTATION = 1;
constexpr uint8_t DISPLAY_TEXT_SIZE = 1;
constexpr int PIN_CARDPUTER_IR = 44;
constexpr uint8_t CARDPUTER_BRIGHTNESS = 180;
#elif BOARD_PROFILE == BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18
constexpr uint16_t LCD_WIDTH = 368;
constexpr uint16_t LCD_HEIGHT = 448;
constexpr uint8_t LCD_ROTATION = 0;
constexpr uint8_t DISPLAY_TEXT_SIZE = 2;
constexpr int PIN_LCD_SDIO0 = 4;
constexpr int PIN_LCD_SDIO1 = 5;
constexpr int PIN_LCD_SDIO2 = 6;
constexpr int PIN_LCD_SDIO3 = 7;
constexpr int PIN_LCD_SCLK = 11;
constexpr int PIN_LCD_CS = 12;
constexpr int PIN_LCD_RST = -1;
constexpr int PIN_TOUCH_SDA = 15;
constexpr int PIN_TOUCH_SCL = 14;
constexpr int PIN_TOUCH_RST = -1;
constexpr int PIN_TOUCH_INT = 21;
constexpr int PIN_BOOT_BUTTON = 0;
constexpr bool BOOT_BUTTON_ACTIVE_LOW = true;
constexpr bool BOOT_BUTTON_AUTODETECT_POLARITY = false;
constexpr uint8_t AMOLED_EXPANDER_I2C_ADDR = 0x20;
constexpr uint8_t AMOLED_BRIGHTNESS = 255;
#elif BOARD_PROFILE == BOARD_PROFILE_WAVESHARE_TOUCH_LCD_147
constexpr uint16_t LCD_WIDTH = 172;
constexpr uint16_t LCD_HEIGHT = 320;
constexpr uint8_t LCD_ROTATION = 2;
constexpr uint8_t DISPLAY_TEXT_SIZE = 1;
constexpr int PIN_LCD_SCLK = 38;
constexpr int PIN_LCD_MOSI = 39;
constexpr int PIN_LCD_CS = 21;
constexpr int PIN_LCD_DC = 45;
constexpr int PIN_LCD_RST = 40;
constexpr int PIN_LCD_BL = 46;
constexpr int PIN_TOUCH_SDA = 42;
constexpr int PIN_TOUCH_SCL = 41;
constexpr int PIN_TOUCH_RST = 47;
constexpr int PIN_TOUCH_INT = 48;
constexpr int PIN_BOOT_BUTTON = 0;
constexpr bool BOOT_BUTTON_ACTIVE_LOW = true;
constexpr bool BOOT_BUTTON_AUTODETECT_POLARITY = false;
constexpr uint8_t DISPLAY_COL_OFFSET = 34;
constexpr uint8_t DISPLAY_ROW_OFFSET = 0;
constexpr uint8_t DISPLAY_SPI_MODE = SPI_MODE0;
constexpr bool DISPLAY_MIRROR_X = true;
constexpr bool DISPLAY_MIRROR_Y = false;
#endif

#ifndef ENABLE_TOUCH
#define ENABLE_TOUCH 1
#endif

#if !BOARD_HAS_TOUCH
#undef ENABLE_TOUCH
#define ENABLE_TOUCH 0
#endif

#ifndef ENABLE_CARDPUTER_IR_TEST
#define ENABLE_CARDPUTER_IR_TEST 0
#endif

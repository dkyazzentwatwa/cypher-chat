#include "TouchInput.h"

#if ENABLE_TOUCH && BOARD_TOUCH_IS_AXS5106L
#include <Wire.h>
#include "esp_lcd_touch_axs5106l.h"
#elif ENABLE_TOUCH && BOARD_TOUCH_IS_FT3168
#include <Arduino_DriveBus_Library.h>
#include <Wire.h>
#include <memory>
#endif

#if ENABLE_TOUCH && BOARD_TOUCH_IS_AXS5106L
namespace {
TwoWire touchWire(0);
}
#elif ENABLE_TOUCH && BOARD_TOUCH_IS_FT3168
namespace {
std::shared_ptr<Arduino_IIC_DriveBus> touchBus;
std::unique_ptr<Arduino_FT3x68> touchDevice;

void remapTouchForRotation(int32_t rawX, int32_t rawY, uint16_t &outX, uint16_t &outY) {
  int32_t x = rawY;
  int32_t y = (LCD_HEIGHT - 1) - rawX;
  if (x < 0) {
    x = 0;
  }
  if (y < 0) {
    y = 0;
  }
  if (x >= LCD_WIDTH) {
    x = LCD_WIDTH - 1;
  }
  if (y >= LCD_HEIGHT) {
    y = LCD_HEIGHT - 1;
  }
  outX = static_cast<uint16_t>(x);
  outY = static_cast<uint16_t>(y);
}
}
#endif

bool TouchInput::begin() {
#if ENABLE_TOUCH && BOARD_TOUCH_IS_AXS5106L
  touchWire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
  bsp_touch_init(&touchWire, PIN_TOUCH_RST, PIN_TOUCH_INT, LCD_ROTATION, LCD_WIDTH, LCD_HEIGHT);
  _ready = true;
  return _ready;
#elif ENABLE_TOUCH && BOARD_TOUCH_IS_FT3168
  touchBus = std::make_shared<Arduino_HWIIC>(PIN_TOUCH_SDA, PIN_TOUCH_SCL, &Wire);
  touchDevice.reset(new Arduino_FT3x68(touchBus, FT3168_DEVICE_ADDRESS, DRIVEBUS_DEFAULT_VALUE,
                                       PIN_TOUCH_INT));

  for (uint8_t tries = 0; tries < 5; tries++) {
    if (touchDevice->begin()) {
      touchDevice->IIC_Write_Device_State(touchDevice->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE,
                                          touchDevice->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_MONITOR);
      Serial.printf("FT3168 touch init ok id=0x%X\n",
                    static_cast<unsigned>(touchDevice->IIC_Read_Device_ID()));
      _ready = true;
      return true;
    }
    Serial.println("FT3168 touch init retry");
    delay(400);
  }

  Serial.println("FT3168 touch init failed");
  _ready = false;
  return false;
#else
  _ready = false;
  return false;
#endif
}

bool TouchInput::read(TouchPoint *points, uint8_t maxPoints, uint8_t &pointCount) {
  pointCount = 0;
  if (!_ready || points == nullptr || maxPoints == 0) {
    return false;
  }

#if ENABLE_TOUCH && BOARD_TOUCH_IS_AXS5106L
  bsp_touch_read();
  touch_data_t touchData;
  if (!bsp_touch_get_coordinates(&touchData)) {
    return false;
  }

  pointCount = min(touchData.touch_num, maxPoints);
  for (uint8_t i = 0; i < pointCount; i++) {
    points[i].x = touchData.coords[i].x;
    points[i].y = touchData.coords[i].y;
  }
  return pointCount > 0;
#elif ENABLE_TOUCH && BOARD_TOUCH_IS_FT3168
  if (!touchDevice) {
    return false;
  }

  const int32_t fingers = static_cast<int32_t>(
      touchDevice->IIC_Read_Device_Value(touchDevice->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER));
  if (fingers <= 0) {
    return false;
  }

  const int32_t rawX = static_cast<int32_t>(
      touchDevice->IIC_Read_Device_Value(touchDevice->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X));
  const int32_t rawY = static_cast<int32_t>(
      touchDevice->IIC_Read_Device_Value(touchDevice->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y));
  remapTouchForRotation(rawX, rawY, points[0].x, points[0].y);
  pointCount = 1;
  return true;
#else
  (void)points;
  (void)maxPoints;
  return false;
#endif
}

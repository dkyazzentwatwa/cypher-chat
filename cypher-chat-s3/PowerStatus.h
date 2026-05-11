#pragma once

#include <Arduino.h>

#include "BoardConfig.h"

struct PowerSnapshot {
  bool available = false;
  bool batteryPresent = false;
  bool charging = false;
  bool discharging = false;
  bool vbusPresent = false;
  bool speakerAvailable = false;
  bool micAvailable = false;
  bool imuAvailable = false;
  bool irAvailable = false;
  uint16_t batteryMv = 0;
  uint16_t vbusMv = 0;
  uint16_t systemMv = 0;
  int batteryPercent = -1;
  float accelX = 0.0f;
  float accelY = 0.0f;
  float accelZ = 0.0f;
  float gyroX = 0.0f;
  float gyroY = 0.0f;
  float gyroZ = 0.0f;
  float imuTempC = 0.0f;
};

class PowerStatus {
public:
  bool begin();
  PowerSnapshot snapshot();
  String menuLabel();
  String headerLabel();
  String detailText();
  bool available() const { return _ready; }

private:
  bool _ready = false;
};

extern PowerStatus powerStatus;

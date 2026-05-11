#include "PowerStatus.h"

#if BOARD_PROFILE == BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18
#include <Wire.h>
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>
#elif BOARD_PROFILE == BOARD_PROFILE_CARDPUTER_ADV
#include <M5Cardputer.h>
#endif

namespace {
#if BOARD_PROFILE == BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18
XPowersPMU pmu;
#endif

#if BOARD_PROFILE == BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18 || BOARD_PROFILE == BOARD_PROFILE_CARDPUTER_ADV
String yesNo(bool value) {
  return value ? "yes" : "no";
}
#endif
}

PowerStatus powerStatus;

bool PowerStatus::begin() {
#if BOARD_PROFILE == BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18
  _ready = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, PIN_TOUCH_SDA, PIN_TOUCH_SCL);
  if (!_ready) {
    Serial.println("AXP2101 PMIC unavailable");
    return false;
  }

  pmu.enableBattDetection();
  pmu.enableBattVoltageMeasure();
  pmu.enableVbusVoltageMeasure();
  pmu.enableSystemVoltageMeasure();
  Serial.printf("AXP2101 PMIC init ok id=0x%X\n", static_cast<unsigned>(pmu.getChipID()));
  return true;
#elif BOARD_PROFILE == BOARD_PROFILE_CARDPUTER_ADV
  _ready = true;
  pinMode(PIN_CARDPUTER_IR, OUTPUT);
  digitalWrite(PIN_CARDPUTER_IR, LOW);
  if (M5Cardputer.Speaker.isEnabled()) {
    M5Cardputer.Speaker.setVolume(96);
    M5Cardputer.Speaker.tone(2200, 35);
  }
#if ENABLE_CARDPUTER_IR_TEST
  for (uint8_t i = 0; i < 4; i++) {
    digitalWrite(PIN_CARDPUTER_IR, HIGH);
    delayMicroseconds(500);
    digitalWrite(PIN_CARDPUTER_IR, LOW);
    delayMicroseconds(500);
  }
#endif
  Serial.printf("Cardputer hardware init: power=%d imu=%d speaker=%d mic=%d\n",
                M5Cardputer.Power.getType() != m5::Power_Class::pmic_unknown ? 1 : 0,
                M5.Imu.isEnabled() ? 1 : 0,
                M5Cardputer.Speaker.isEnabled() ? 1 : 0,
                M5Cardputer.Mic.isEnabled() ? 1 : 0);
  return true;
#else
  _ready = false;
  return false;
#endif
}

PowerSnapshot PowerStatus::snapshot() {
  PowerSnapshot status;
#if BOARD_PROFILE == BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18
  if (!_ready) {
    return status;
  }

  status.available = true;
  status.batteryPresent = pmu.isBatteryConnect();
  status.charging = pmu.isCharging();
  status.discharging = pmu.isDischarge();
  status.vbusPresent = pmu.isVbusIn();
  status.batteryMv = pmu.getBattVoltage();
  status.vbusMv = pmu.getVbusVoltage();
  status.systemMv = pmu.getSystemVoltage();
  if (status.batteryPresent) {
    status.batteryPercent = pmu.getBatteryPercent();
  }
#elif BOARD_PROFILE == BOARD_PROFILE_CARDPUTER_ADV
  if (!_ready) {
    return status;
  }

  status.available = true;
  status.batteryPercent = M5Cardputer.Power.getBatteryLevel();
  status.batteryPresent = status.batteryPercent >= 0;
  const int16_t batteryMv = M5Cardputer.Power.getBatteryVoltage();
  const int16_t vbusMv = M5Cardputer.Power.getVBUSVoltage();
  status.batteryMv = batteryMv > 0 ? batteryMv : 0;
  status.vbusMv = vbusMv > 0 ? vbusMv : 0;
  status.vbusPresent = status.vbusMv > 4200;
  status.charging = M5Cardputer.Power.isCharging() == m5::Power_Class::is_charging;
  status.discharging = status.batteryPresent && !status.charging && !status.vbusPresent;
  status.speakerAvailable = M5Cardputer.Speaker.isEnabled();
  status.micAvailable = M5Cardputer.Mic.isEnabled();
  status.imuAvailable = M5.Imu.isEnabled();
  status.irAvailable = true;
  if (status.imuAvailable) {
    M5.Imu.update();
    M5.Imu.getAccel(&status.accelX, &status.accelY, &status.accelZ);
    M5.Imu.getGyro(&status.gyroX, &status.gyroY, &status.gyroZ);
    M5.Imu.getTemp(&status.imuTempC);
  }
#endif
  return status;
}

String PowerStatus::menuLabel() {
#if BOARD_PROFILE == BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18 || BOARD_PROFILE == BOARD_PROFILE_CARDPUTER_ADV
  const PowerSnapshot status = snapshot();
  if (!status.available) {
    return "Power unavailable";
  }
  if (status.batteryPresent && status.batteryPercent >= 0) {
    String label = "BAT ";
    label += status.batteryPercent;
    label += "%";
    if (status.charging) {
      label += " CHG";
    } else if (status.vbusPresent) {
      label += " USB";
    }
    return label;
  }
  if (status.batteryPresent && status.batteryMv > 0) {
    String label = "BAT ";
    label += status.batteryMv;
    label += "mV";
    return label;
  }
  if (status.vbusPresent) {
    return "USB power";
  }
  return "Power unknown";
#else
  return String();
#endif
}

String PowerStatus::headerLabel() {
#if BOARD_PROFILE == BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18 || BOARD_PROFILE == BOARD_PROFILE_CARDPUTER_ADV
  const PowerSnapshot status = snapshot();
  if (!status.available) {
    return String();
  }
  if (status.batteryPresent && status.batteryPercent >= 0) {
    String label;
    label += status.batteryPercent;
    label += "%";
    if (status.charging) {
      label += " CHG";
    }
    return label;
  }
  if (status.vbusPresent) {
    return "USB";
  }
  return String();
#else
  return String();
#endif
}

String PowerStatus::detailText() {
#if BOARD_PROFILE == BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18
  const PowerSnapshot status = snapshot();
  if (!status.available) {
    return "Power: unavailable\n";
  }

  String body;
  body += "Power: AXP2101\n";
  body += "Battery: ";
  body += yesNo(status.batteryPresent);
  body += "\nCharging: ";
  body += yesNo(status.charging);
  body += "\nDischarge: ";
  body += yesNo(status.discharging);
  body += "\nUSB/VBUS: ";
  body += yesNo(status.vbusPresent);
  if (status.batteryPercent >= 0) {
    body += "\nBatt pct: ";
    body += status.batteryPercent;
    body += "%";
  }
  if (status.batteryMv > 0) {
    body += "\nBatt mV: ";
    body += status.batteryMv;
  }
  if (status.vbusMv > 0) {
    body += "\nVBUS mV: ";
    body += status.vbusMv;
  }
  if (status.systemMv > 0) {
    body += "\nSYS mV: ";
    body += status.systemMv;
  }
  body += "\n";
  return body;
#elif BOARD_PROFILE == BOARD_PROFILE_CARDPUTER_ADV
  const PowerSnapshot status = snapshot();
  if (!status.available) {
    return "Cardputer HW: unavailable\n";
  }

  String body;
  body += "Cardputer HW\n";
  body += "Battery: ";
  if (status.batteryPercent >= 0) {
    body += status.batteryPercent;
    body += "%";
  } else {
    body += "unknown";
  }
  if (status.batteryMv > 0) {
    body += " ";
    body += status.batteryMv;
    body += "mV";
  }
  body += "\nCharging: ";
  body += yesNo(status.charging);
  body += "\nUSB/VBUS: ";
  body += yesNo(status.vbusPresent);
  body += "\nIMU: ";
  body += yesNo(status.imuAvailable);
  if (status.imuAvailable) {
    body += "\nAccel: ";
    body += String(status.accelX, 1);
    body += ",";
    body += String(status.accelY, 1);
    body += ",";
    body += String(status.accelZ, 1);
    body += "\nGyro: ";
    body += String(status.gyroX, 1);
    body += ",";
    body += String(status.gyroY, 1);
    body += ",";
    body += String(status.gyroZ, 1);
    body += "\nIMU C: ";
    body += String(status.imuTempC, 1);
  }
  body += "\nSpeaker: ";
  body += yesNo(status.speakerAvailable);
  body += "\nMic: ";
  body += yesNo(status.micAvailable);
  body += "\nIR test: ";
  body += ENABLE_CARDPUTER_IR_TEST ? "enabled" : "guarded";
  body += "\n";
  return body;
#else
  return String();
#endif
}

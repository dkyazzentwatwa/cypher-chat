#pragma once

#include <Arduino.h>
#include "Config_Starbeam.h"
#include <time.h>

/**
 * TimeManager - Time/date synchronization (Stub implementation)
 */

class TimeManager {
public:
  TimeManager() : _timezoneOffset(0) {}

  bool begin() { return true; }
  void setTime(time_t unixTime) {}
  time_t getTime() { return 0; }
  void setTimezone(int8_t offset) { _timezoneOffset = offset; }
  int8_t getTimezone() { return _timezoneOffset; }
  bool syncFromGPS() { return false; }
  String formatTime(const char* format = "%Y-%m-%d %H:%M:%S") { return String("N/A"); }

private:
  int8_t _timezoneOffset;
};

extern TimeManager timeMgr;

#pragma once

#include <Arduino.h>
#include "Config.h"
#include "MeshManager.h"

enum GpsStatus {
  GPS_STATUS_DISABLED,
  GPS_STATUS_NO_FIX,
  GPS_STATUS_FIX_2D,
  GPS_STATUS_FIX_3D
};

class GpsPort {
public:
  bool begin();
  void update();
  bool available() const { return GPS_ENABLED; }
  bool hasFix() const { return _coords.valid; }
  GpsStatus status() const { return GPS_ENABLED ? _status : GPS_STATUS_DISABLED; }
  GPSCoordinates coordinates() const { return _coords; }
  int satellites() const { return _satellites; }
  float hdop() const { return _hdop; }
  unsigned long fixAgeMs() const;
  void printStatus();

private:
  HardwareSerial _serial = HardwareSerial(1);
  GpsStatus _status = GPS_STATUS_DISABLED;
  GPSCoordinates _coords = {0, 0, 0, false};
  char _nmea[128] = {0};
  uint8_t _nmeaIndex = 0;
  bool _nmeaStarted = false;
  int _satellites = 0;
  float _hdop = 99.9f;
  unsigned long _lastFixMs = 0;

  void processChar(char c);
  void parseGga(const char *sentence);
  void parseRmc(const char *sentence);
  bool validateChecksum(const char *sentence) const;
  float parseCoordinate(const char *value, char direction) const;
};

extern GpsPort gpsPort;

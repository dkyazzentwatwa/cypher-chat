#include "GpsPort.h"
#include "OutputManager.h"
#include <cstdlib>
#include <cstring>

GpsPort gpsPort;

bool GpsPort::begin() {
#if GPS_ENABLED
  _serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  _status = GPS_STATUS_NO_FIX;
  output.printf("GPS: enabled RX=%d TX=%d baud=%lu\n",
                GPS_RX_PIN, GPS_TX_PIN, (unsigned long)GPS_BAUD);
  return true;
#else
  _status = GPS_STATUS_DISABLED;
  return false;
#endif
}

void GpsPort::update() {
#if GPS_ENABLED
  while (_serial.available()) {
    processChar(static_cast<char>(_serial.read()));
  }

  if (_coords.valid && millis() - _lastFixMs > 10000) {
    _coords.valid = false;
    _status = GPS_STATUS_NO_FIX;
  }
#endif
}

unsigned long GpsPort::fixAgeMs() const {
  if (_lastFixMs == 0) {
    return 0;
  }
  return millis() - _lastFixMs;
}

void GpsPort::printStatus() {
#if GPS_ENABLED
  output.println("[GPS] Enabled");
  output.printf("[GPS] Status: %s\n", _coords.valid ? "fix" : "no fix");
  output.printf("[GPS] Satellites: %d  HDOP: %.1f\n", _satellites, _hdop);
  if (_coords.valid) {
    output.printf("[GPS] Lat: %.6f  Lon: %.6f  Alt: %.1fm\n",
                  _coords.latitude, _coords.longitude, _coords.altitude);
    output.printf("[GPS] Fix age: %lums\n", fixAgeMs());
  }
#else
  output.println("[GPS] Disabled for this board profile");
#endif
}

void GpsPort::processChar(char c) {
  if (c == '$') {
    _nmeaStarted = true;
    _nmeaIndex = 0;
    _nmea[_nmeaIndex++] = c;
    return;
  }

  if (!_nmeaStarted) {
    return;
  }

  if (c == '\r' || c == '\n') {
    _nmea[_nmeaIndex] = '\0';
    _nmeaStarted = false;
    _nmeaIndex = 0;

    if (!validateChecksum(_nmea)) {
      return;
    }

    if (strncmp(_nmea + 3, "GGA", 3) == 0) {
      parseGga(_nmea);
    } else if (strncmp(_nmea + 3, "RMC", 3) == 0) {
      parseRmc(_nmea);
    }
    return;
  }

  if (_nmeaIndex < sizeof(_nmea) - 1) {
    _nmea[_nmeaIndex++] = c;
  }
}

void GpsPort::parseGga(const char *sentence) {
  char fields[15][20] = {};
  uint8_t field = 0;
  uint8_t index = 0;
  const char *p = strchr(sentence, ',');
  if (!p) {
    return;
  }
  p++;

  while (*p && field < 15) {
    if (*p == ',' || *p == '*') {
      fields[field][index] = '\0';
      field++;
      index = 0;
      if (*p == '*') break;
    } else if (index < sizeof(fields[0]) - 1) {
      fields[field][index++] = *p;
    }
    p++;
  }

  if (field < 9 || atoi(fields[5]) == 0) {
    _coords.valid = false;
    _status = GPS_STATUS_NO_FIX;
    return;
  }

  if (fields[1][0] && fields[2][0]) {
    _coords.latitude = parseCoordinate(fields[1], fields[2][0]);
  }
  if (fields[3][0] && fields[4][0]) {
    _coords.longitude = parseCoordinate(fields[3], fields[4][0]);
  }

  _satellites = atoi(fields[6]);
  _hdop = fields[7][0] ? atof(fields[7]) : 99.9f;
  _coords.altitude = fields[8][0] ? atof(fields[8]) : 0.0f;
  _coords.valid = true;
  _status = fields[8][0] ? GPS_STATUS_FIX_3D : GPS_STATUS_FIX_2D;
  _lastFixMs = millis();
  meshMgr.setGPS(_coords);
}

void GpsPort::parseRmc(const char *sentence) {
  char fields[12][20] = {};
  uint8_t field = 0;
  uint8_t index = 0;
  const char *p = strchr(sentence, ',');
  if (!p) {
    return;
  }
  p++;

  while (*p && field < 12) {
    if (*p == ',' || *p == '*') {
      fields[field][index] = '\0';
      field++;
      index = 0;
      if (*p == '*') break;
    } else if (index < sizeof(fields[0]) - 1) {
      fields[field][index++] = *p;
    }
    p++;
  }

  if (field < 6 || fields[1][0] != 'A') {
    return;
  }

  if (fields[2][0] && fields[3][0]) {
    _coords.latitude = parseCoordinate(fields[2], fields[3][0]);
  }
  if (fields[4][0] && fields[5][0]) {
    _coords.longitude = parseCoordinate(fields[4], fields[5][0]);
  }
  _coords.valid = true;
  if (_status == GPS_STATUS_NO_FIX) {
    _status = GPS_STATUS_FIX_2D;
  }
  _lastFixMs = millis();
  meshMgr.setGPS(_coords);
}

bool GpsPort::validateChecksum(const char *sentence) const {
  if (!sentence || sentence[0] != '$') {
    return false;
  }

  const char *star = strchr(sentence, '*');
  if (!star || strlen(star) < 3) {
    return false;
  }

  uint8_t checksum = 0;
  for (const char *p = sentence + 1; p < star; p++) {
    checksum ^= static_cast<uint8_t>(*p);
  }

  char expected[3];
  snprintf(expected, sizeof(expected), "%02X", checksum);
  return strncasecmp(expected, star + 1, 2) == 0;
}

float GpsPort::parseCoordinate(const char *value, char direction) const {
  if (!value || strlen(value) < 4) {
    return 0.0f;
  }

  float raw = atof(value);
  int degrees = static_cast<int>(raw / 100);
  float minutes = raw - (degrees * 100);
  float decimal = degrees + (minutes / 60.0f);
  if (direction == 'S' || direction == 'W') {
    decimal = -decimal;
  }
  return decimal;
}

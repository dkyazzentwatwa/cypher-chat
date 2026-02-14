#include "GPSManager.h"
#include <cstring>
#include <cstdlib>

// Global instance
GPSManager gpsMgr;

GPSManager::GPSManager()
  : _enabled(false)
  , _status(GPS_NO_MODULE)
  , _satellites(0)
  , _hdop(99.9f)
  , _speed(0)
  , _course(0)
  , _lastFixTime(0)
  , _nmeaIndex(0)
  , _nmeaStarted(false)
  , _serial(nullptr)
{
  _coords = {0, 0, 0, false};
  memset(_nmeaBuffer, 0, sizeof(_nmeaBuffer));
}

bool GPSManager::begin() {
#if !GPS_ENABLED
  _status = GPS_NO_MODULE;
  return false;
#else
  // Use Serial2 for GPS (ESP32 has 3 hardware serial ports)
  _serial = &Serial2;
  _serial->begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  _enabled = true;
  _status = GPS_NO_FIX;

  Serial.println("GPSManager: Initialized on Serial2");
  Serial.printf("  RX Pin: %d, TX Pin: %d, Baud: %d\n", GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);

  return true;
#endif
}

void GPSManager::update() {
  if (!_enabled || _serial == nullptr) return;

  // Process all available GPS data
  while (_serial->available()) {
    char c = _serial->read();
    processNMEA(c);
  }

  // Update status based on fix age
  if (_coords.valid && (millis() - _lastFixTime > 5000)) {
    // Fix is stale (>5 seconds old)
    _coords.valid = false;
    _status = GPS_NO_FIX;
  }
}

void GPSManager::processNMEA(char c) {
  if (c == '$') {
    // Start of new sentence
    _nmeaStarted = true;
    _nmeaIndex = 0;
    _nmeaBuffer[0] = '$';
    _nmeaIndex = 1;
    return;
  }

  if (!_nmeaStarted) return;

  if (c == '\r' || c == '\n') {
    // End of sentence
    if (_nmeaIndex > 0) {
      _nmeaBuffer[_nmeaIndex] = '\0';

      // Validate checksum
      if (validateChecksum(_nmeaBuffer)) {
        // Parse based on sentence type
        if (strncmp(_nmeaBuffer + 3, "GGA", 3) == 0) {
          parseGGA(_nmeaBuffer);
        } else if (strncmp(_nmeaBuffer + 3, "RMC", 3) == 0) {
          parseRMC(_nmeaBuffer);
        } else if (strncmp(_nmeaBuffer + 3, "GSA", 3) == 0) {
          parseGSA(_nmeaBuffer);
        }
      }
    }
    _nmeaStarted = false;
    _nmeaIndex = 0;
    return;
  }

  // Add character to buffer
  if (_nmeaIndex < sizeof(_nmeaBuffer) - 1) {
    _nmeaBuffer[_nmeaIndex++] = c;
  }
}

void GPSManager::parseGGA(const char* sentence) {
  // $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,*47
  // Fields: time, lat, N/S, lon, E/W, quality, satellites, hdop, altitude, M, geoid, M, age, station*checksum

  char fields[15][20];
  int fieldCount = 0;
  int fieldIndex = 0;
  bool inField = false;

  // Skip $GPGGA,
  const char* p = strchr(sentence, ',');
  if (!p) return;
  p++;

  while (*p && fieldCount < 15) {
    if (*p == ',' || *p == '*') {
      fields[fieldCount][fieldIndex] = '\0';
      fieldCount++;
      fieldIndex = 0;
    } else {
      if (fieldIndex < 19) {
        fields[fieldCount][fieldIndex++] = *p;
      }
    }
    if (*p == '*') break;
    p++;
  }

  if (fieldCount < 10) return;

  // Parse fix quality (field 5)
  int quality = parseInt(fields[5]);
  if (quality == 0) {
    _status = GPS_NO_FIX;
    _coords.valid = false;
    return;
  }

  // Parse latitude (fields 1-2)
  if (strlen(fields[1]) > 0 && strlen(fields[2]) > 0) {
    _coords.latitude = parseCoordinate(fields[1], fields[2][0]);
  }

  // Parse longitude (fields 3-4)
  if (strlen(fields[3]) > 0 && strlen(fields[4]) > 0) {
    _coords.longitude = parseCoordinate(fields[3], fields[4][0]);
  }

  // Parse satellites (field 6)
  _satellites = parseInt(fields[6]);

  // Parse HDOP (field 7)
  _hdop = parseFloat(fields[7]);

  // Parse altitude (field 8)
  if (strlen(fields[8]) > 0) {
    _coords.altitude = parseFloat(fields[8]);
    _status = GPS_FIX_3D;
  } else {
    _coords.altitude = 0;
    _status = GPS_FIX_2D;
  }

  _coords.valid = true;
  _lastFixTime = millis();

  // Update mesh manager with current GPS
  meshMgr.setGPS(_coords);
}

void GPSManager::parseRMC(const char* sentence) {
  // $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
  // Fields: time, status, lat, N/S, lon, E/W, speed, course, date, magnetic, E/W*checksum

  char fields[12][20];
  int fieldCount = 0;
  int fieldIndex = 0;

  const char* p = strchr(sentence, ',');
  if (!p) return;
  p++;

  while (*p && fieldCount < 12) {
    if (*p == ',' || *p == '*') {
      fields[fieldCount][fieldIndex] = '\0';
      fieldCount++;
      fieldIndex = 0;
    } else {
      if (fieldIndex < 19) {
        fields[fieldCount][fieldIndex++] = *p;
      }
    }
    if (*p == '*') break;
    p++;
  }

  if (fieldCount < 8) return;

  // Check status (field 1) - 'A' = active, 'V' = void
  if (fields[1][0] != 'A') {
    return;
  }

  // Parse speed in knots (field 6), convert to km/h
  if (strlen(fields[6]) > 0) {
    _speed = parseFloat(fields[6]) * 1.852f;
  }

  // Parse course (field 7)
  if (strlen(fields[7]) > 0) {
    _course = parseFloat(fields[7]);
  }
}

void GPSManager::parseGSA(const char* sentence) {
  // $GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39
  // Fields: mode, fix_type, prn1-12, pdop, hdop, vdop*checksum

  char fields[18][10];
  int fieldCount = 0;
  int fieldIndex = 0;

  const char* p = strchr(sentence, ',');
  if (!p) return;
  p++;

  while (*p && fieldCount < 18) {
    if (*p == ',' || *p == '*') {
      fields[fieldCount][fieldIndex] = '\0';
      fieldCount++;
      fieldIndex = 0;
    } else {
      if (fieldIndex < 9) {
        fields[fieldCount][fieldIndex++] = *p;
      }
    }
    if (*p == '*') break;
    p++;
  }

  if (fieldCount < 17) return;

  // Parse fix type (field 1): 1=no fix, 2=2D, 3=3D
  int fixType = parseInt(fields[1]);
  if (fixType == 1) {
    _status = GPS_NO_FIX;
  } else if (fixType == 2) {
    _status = GPS_FIX_2D;
  } else if (fixType == 3) {
    _status = GPS_FIX_3D;
  }

  // Parse HDOP (field 15)
  if (strlen(fields[15]) > 0) {
    _hdop = parseFloat(fields[15]);
  }
}

float GPSManager::parseCoordinate(const char* str, char dir) {
  // NMEA format: DDDMM.MMMM or DDMM.MMMM
  // Convert to decimal degrees

  float coord = atof(str);
  int degrees = (int)(coord / 100);
  float minutes = coord - (degrees * 100);
  float decimal = degrees + (minutes / 60.0f);

  if (dir == 'S' || dir == 'W') {
    decimal = -decimal;
  }

  return decimal;
}

float GPSManager::parseFloat(const char* str) {
  if (str == nullptr || strlen(str) == 0) return 0.0f;
  return atof(str);
}

int GPSManager::parseInt(const char* str) {
  if (str == nullptr || strlen(str) == 0) return 0;
  return atoi(str);
}

bool GPSManager::validateChecksum(const char* sentence) {
  // Find checksum marker
  const char* asterisk = strchr(sentence, '*');
  if (!asterisk || strlen(asterisk) < 3) return false;

  // Parse expected checksum
  char hexStr[3] = {asterisk[1], asterisk[2], '\0'};
  uint8_t expected = (uint8_t)strtol(hexStr, nullptr, 16);

  // Calculate actual checksum
  uint8_t actual = calculateChecksum(sentence);

  return actual == expected;
}

uint8_t GPSManager::calculateChecksum(const char* sentence) {
  // XOR all characters between $ and *
  uint8_t checksum = 0;
  const char* p = sentence;

  // Skip $
  if (*p == '$') p++;

  // XOR until * or end
  while (*p && *p != '*') {
    checksum ^= *p++;
  }

  return checksum;
}

unsigned long GPSManager::getFixAge() const {
  if (_lastFixTime == 0) return 0;
  return millis() - _lastFixTime;
}

char* GPSManager::formatCoordinates(char* buffer, size_t bufSize) const {
  if (!_coords.valid) {
    snprintf(buffer, bufSize, "No GPS fix");
  } else {
    snprintf(buffer, bufSize, "%.6f, %.6f (alt: %.1fm)",
             _coords.latitude, _coords.longitude, _coords.altitude);
  }
  return buffer;
}

char* GPSManager::formatMapsURL(char* buffer, size_t bufSize) const {
  if (!_coords.valid) {
    snprintf(buffer, bufSize, "No GPS fix");
  } else {
    snprintf(buffer, bufSize, "https://maps.google.com/?q=%.6f,%.6f",
             _coords.latitude, _coords.longitude);
  }
  return buffer;
}

void GPSManager::setEnabled(bool enabled) {
  _enabled = enabled;
  if (!enabled) {
    _status = GPS_NO_MODULE;
    _coords.valid = false;
  }
}

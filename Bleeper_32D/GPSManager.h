#pragma once

#include <Arduino.h>
#include "Config_Starbeam.h"
#include "MeshManager.h"

/**
 * GPSManager - Optional GPS support for location-tagged emergencies
 *
 * Supports standard NMEA GPS modules (Neo-6M, Neo-7M, etc.)
 * When GPS is available, emergency broadcasts include coordinates
 * for locating people in disaster scenarios.
 *
 * Hardware: Connect GPS TX to ESP32 RX (GPS_RX_PIN in Config.h)
 */

// GPS fix status
enum GPSStatus {
  GPS_NO_MODULE,      // GPS not enabled/detected
  GPS_NO_FIX,         // Module present but no satellite fix
  GPS_FIX_2D,         // 2D fix (lat/lon only)
  GPS_FIX_3D          // 3D fix (lat/lon/alt)
};

class GPSManager {
public:
  GPSManager();

  /**
   * Initialize GPS module
   * @return true if GPS module detected
   */
  bool begin();

  /**
   * Process incoming GPS data - call from loop()
   */
  void update();

  /**
   * Get current GPS status
   * @return GPS fix status
   */
  GPSStatus getStatus() const { return _status; }

  /**
   * Check if we have a valid fix
   * @return true if 2D or 3D fix available
   */
  bool hasFix() const { return _status >= GPS_FIX_2D; }

  /**
   * Get current coordinates
   * @return GPSCoordinates struct (check .valid field)
   */
  GPSCoordinates getCoordinates() const { return _coords; }

  /**
   * Get latitude in degrees
   * @return Latitude (-90 to +90)
   */
  float getLatitude() const { return _coords.latitude; }

  /**
   * Get longitude in degrees
   * @return Longitude (-180 to +180)
   */
  float getLongitude() const { return _coords.longitude; }

  /**
   * Get altitude in meters
   * @return Altitude in meters (0 if 2D fix only)
   */
  float getAltitude() const { return _coords.altitude; }

  /**
   * Get number of satellites in view
   * @return Satellite count
   */
  int getSatellites() const { return _satellites; }

  /**
   * Get horizontal dilution of precision
   * @return HDOP (lower is better, <2 is good)
   */
  float getHDOP() const { return _hdop; }

  /**
   * Get speed in km/h
   * @return Speed over ground
   */
  float getSpeed() const { return _speed; }

  /**
   * Get course/heading in degrees
   * @return Course (0-360 degrees)
   */
  float getCourse() const { return _course; }

  /**
   * Get time since last valid fix
   * @return Milliseconds since last fix (0 if never)
   */
  unsigned long getFixAge() const;

  /**
   * Get formatted coordinate string
   * @param buffer Output buffer
   * @param bufSize Buffer size
   * @return Pointer to buffer
   */
  char* formatCoordinates(char* buffer, size_t bufSize) const;

  /**
   * Get Google Maps URL for current position
   * @param buffer Output buffer
   * @param bufSize Buffer size
   * @return Pointer to buffer
   */
  char* formatMapsURL(char* buffer, size_t bufSize) const;

  /**
   * Enable/disable GPS (saves power when not needed)
   * @param enabled true to enable
   */
  void setEnabled(bool enabled);

  /**
   * Check if GPS is enabled
   * @return true if enabled
   */
  bool isEnabled() const { return _enabled; }

private:
  // NMEA parsing
  void processNMEA(char c);
  void parseGGA(const char* sentence);   // Position data
  void parseRMC(const char* sentence);   // Recommended minimum
  void parseGSA(const char* sentence);   // DOP and satellites
  float parseCoordinate(const char* str, char dir);
  float parseFloat(const char* str);
  int parseInt(const char* str);

  // Checksum validation
  bool validateChecksum(const char* sentence);
  uint8_t calculateChecksum(const char* sentence);

  // State
  bool _enabled;
  GPSStatus _status;
  GPSCoordinates _coords;

  // Additional GPS data
  int _satellites;
  float _hdop;
  float _speed;
  float _course;
  unsigned long _lastFixTime;

  // NMEA sentence buffer
  char _nmeaBuffer[128];
  int _nmeaIndex;
  bool _nmeaStarted;

  // Serial port (using HardwareSerial)
  HardwareSerial* _serial;
};

// Global instance
extern GPSManager gpsMgr;

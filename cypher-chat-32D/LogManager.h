#pragma once

#include <Arduino.h>
#include "Config.h"
#include <vector>

/**
 * LogManager - Circular log buffer (Stub implementation)
 */

enum LogLevel {
  LOG_NONE = 0,
  LOG_ERROR = 1,
  LOG_INFO = 2,
  LOG_WARN = 3,
  LOG_DEBUG = 4
};

class LogManager {
public:
  LogManager() : _logLevel(LOG_INFO) {}

  bool begin() { return true; }
  void log(const char* level, const char* msg) {}
  std::vector<String> getRecentLogs(int count = 20) { return std::vector<String>(); }
  void setLogLevel(uint8_t level) { _logLevel = level; }
  uint8_t getLogLevel() { return _logLevel; }
  void clearLogs() {}
  void dumpToSerial() {}

private:
  uint8_t _logLevel;
};

extern LogManager logMgr;

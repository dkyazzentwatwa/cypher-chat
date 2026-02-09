#pragma once

#include <Arduino.h>
#include "Config_Starbeam.h"
#include <vector>
#include <SPIFFS.h>

/**
 * FileSystemManager - SPIFFS/LittleFS operations (Stub implementation)
 */

class FileSystemManager {
public:
  FileSystemManager() {}

  bool begin() {
#if FILESYSTEM_ENABLED
    return SPIFFS.begin(true);
#else
    return false;
#endif
  }

  std::vector<String> listFiles(const char* path = "/") {
    return std::vector<String>();
  }

  String readFile(const char* path) { return String(); }
  bool writeFile(const char* path, const char* data) { return false; }
  bool deleteFile(const char* path) { return false; }
  bool format() { return false; }
  uint32_t getTotalSpace() { return 0; }
  uint32_t getUsedSpace() { return 0; }
};

extern FileSystemManager fileSystemMgr;

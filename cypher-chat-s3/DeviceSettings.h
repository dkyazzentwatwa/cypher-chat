#pragma once

#include "Config_S3.h"

struct DeviceSettingsState {
  bool hasName;
  bool hasPasskey;
};

class DeviceSettings {
public:
  static DeviceSettingsState load();
  static bool saveName(const char* name);
  static bool savePasskey(uint32_t passkey);
  static bool clear();
};

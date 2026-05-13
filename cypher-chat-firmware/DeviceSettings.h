#pragma once

#include "Config.h"

struct DeviceSettingsState {
  bool hasName;
  bool hasPassphrase;
};

class DeviceSettings {
public:
  static DeviceSettingsState load();
  static bool saveName(const char* name);
  static bool savePassphrase(const char* passphrase);
  static bool clear();
};

#pragma once

#include "Config_Cardputer.h"

struct DeviceSettingsState {
  bool hasName;
  bool hasPasskey;
  bool firstRun;
  uint8_t brightness;
  bool speakerMuted;
  uint8_t defaultScreen;
  uint8_t meshChannel;
  uint8_t fieldMode;
};

class DeviceSettings {
public:
  static DeviceSettingsState load();
  static bool saveName(const char* name);
  static bool savePasskey(uint32_t passkey);
  static bool saveBrightness(uint8_t brightness);
  static bool saveSpeakerMuted(bool muted);
  static bool saveDefaultScreen(uint8_t screen);
  static bool saveMeshChannel(uint8_t channel);
  static bool saveFieldMode(uint8_t mode);
  static bool clear();
};

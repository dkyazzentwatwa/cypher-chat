#include "DeviceSettings.h"
#include <Preferences.h>

DeviceSettingsState DeviceSettings::load() {
  DeviceSettingsState state = { false, false, true, 180, false, 0, MESH_CHANNEL, 0 };
  Preferences prefs;
  prefs.begin("device", true);

  char savedName[MAX_UNIT_NAME_LEN + 1];
  size_t nameLen = prefs.getString("name", savedName, sizeof(savedName));
  if (nameLen > 0) {
    savedName[MAX_UNIT_NAME_LEN] = '\0';
    unitName = String(savedName);
    state.hasName = true;
  }

  if (prefs.isKey("passkey")) {
    uint32_t savedPasskey = prefs.getUInt("passkey", DEFAULT_PASSKEY);
    if (savedPasskey >= MIN_PASSKEY && savedPasskey <= MAX_PASSKEY) {
      currentPasskey = savedPasskey;
      state.hasPasskey = true;
    }
  }

  state.firstRun = !(state.hasName && state.hasPasskey);
  state.brightness = prefs.getUChar("bright", 180);
  if (state.brightness < 40) state.brightness = 40;
  if (state.brightness > 255) state.brightness = 255;
  state.speakerMuted = prefs.getBool("muted", false);
  state.defaultScreen = prefs.getUChar("screen", 0);
  state.meshChannel = prefs.getUChar("channel", MESH_CHANNEL);
  if (state.meshChannel < 1 || state.meshChannel > 13) state.meshChannel = MESH_CHANNEL;
  state.fieldMode = prefs.getUChar("fieldmode", 0);

  prefs.end();
  return state;
}

bool DeviceSettings::saveName(const char* name) {
  if (!name || strlen(name) == 0 || strlen(name) > MAX_UNIT_NAME_LEN) {
    return false;
  }

  Preferences prefs;
  prefs.begin("device", false);
  size_t written = prefs.putString("name", name);
  prefs.end();
  return written > 0;
}

bool DeviceSettings::savePasskey(uint32_t passkey) {
  if (passkey < MIN_PASSKEY || passkey > MAX_PASSKEY) {
    return false;
  }

  Preferences prefs;
  prefs.begin("device", false);
  size_t written = prefs.putUInt("passkey", passkey);
  prefs.end();
  return written > 0;
}

bool DeviceSettings::saveBrightness(uint8_t brightness) {
  if (brightness < 40) brightness = 40;
  Preferences prefs;
  prefs.begin("device", false);
  size_t written = prefs.putUChar("bright", brightness);
  prefs.end();
  return written > 0;
}

bool DeviceSettings::saveSpeakerMuted(bool muted) {
  Preferences prefs;
  prefs.begin("device", false);
  size_t written = prefs.putBool("muted", muted);
  prefs.end();
  return written > 0;
}

bool DeviceSettings::saveDefaultScreen(uint8_t screen) {
  Preferences prefs;
  prefs.begin("device", false);
  size_t written = prefs.putUChar("screen", screen);
  prefs.end();
  return written > 0;
}

bool DeviceSettings::saveMeshChannel(uint8_t channel) {
  if (channel < 1 || channel > 13) {
    return false;
  }

  Preferences prefs;
  prefs.begin("device", false);
  size_t written = prefs.putUChar("channel", channel);
  prefs.end();
  return written > 0;
}

bool DeviceSettings::saveFieldMode(uint8_t mode) {
  Preferences prefs;
  prefs.begin("device", false);
  size_t written = prefs.putUChar("fieldmode", mode);
  prefs.end();
  return written > 0;
}

bool DeviceSettings::clear() {
  Preferences prefs;
  prefs.begin("device", false);
  bool ok = prefs.clear();
  prefs.end();
  return ok;
}

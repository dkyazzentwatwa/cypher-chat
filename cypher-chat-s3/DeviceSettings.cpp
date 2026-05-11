#include "DeviceSettings.h"
#include <Preferences.h>

DeviceSettingsState DeviceSettings::load() {
  DeviceSettingsState state = { false, false };
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

bool DeviceSettings::clear() {
  Preferences prefs;
  prefs.begin("device", false);
  bool ok = prefs.clear();
  prefs.end();
  return ok;
}

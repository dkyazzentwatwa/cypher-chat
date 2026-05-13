#include "DeviceSettings.h"
#include "MeshCrypto.h"
#include <Preferences.h>

DeviceSettingsState DeviceSettings::load() {
  DeviceSettingsState state = { false, false };
  Preferences prefs;
  if (!prefs.begin("device", false)) {
    return state;
  }

  if (prefs.isKey("name")) {
    char savedName[MAX_UNIT_NAME_LEN + 1];
    size_t nameLen = prefs.getString("name", savedName, sizeof(savedName));
    if (nameLen > 0) {
      savedName[MAX_UNIT_NAME_LEN] = '\0';
      unitName = String(savedName);
      state.hasName = true;
    }
  }

  prefs.end();

  char savedPassphrase[MAX_PASSPHRASE_LEN + 1];
  if (MeshCrypto::loadPassphrase(savedPassphrase, sizeof(savedPassphrase))) {
    strncpy(currentPassphrase, savedPassphrase, MAX_PASSPHRASE_LEN);
    currentPassphrase[MAX_PASSPHRASE_LEN] = '\0';
    state.hasPassphrase = true;
  }

  return state;
}

bool DeviceSettings::saveName(const char* name) {
  if (!name || strlen(name) == 0 || strlen(name) > MAX_UNIT_NAME_LEN) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin("device", false)) {
    return false;
  }
  size_t written = prefs.putString("name", name);
  prefs.end();
  return written > 0;
}

bool DeviceSettings::savePassphrase(const char* passphrase) {
  if (!passphrase || strlen(passphrase) < MIN_PASSPHRASE_LEN || strlen(passphrase) > MAX_PASSPHRASE_LEN) {
    return false;
  }
  return MeshCrypto::savePassphrase(passphrase);
}

bool DeviceSettings::clear() {
  Preferences prefs;
  if (!prefs.begin("device", false)) {
    return false;
  }
  bool ok = prefs.clear();
  prefs.end();
  return ok;
}

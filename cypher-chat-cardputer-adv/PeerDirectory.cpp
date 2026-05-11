#include "PeerDirectory.h"
#include <Preferences.h>
#include <cstring>

PeerDirectory peerDirectory;

PeerDirectory::PeerDirectory() {
  memset(_entries, 0, sizeof(_entries));
}

void PeerDirectory::begin() {
  for (int i = 0; i < FIELD_PEER_DIRECTORY_SIZE; i++) {
    loadEntry(i);
  }
}

void PeerDirectory::observe(const MeshPeerInfo& peer) {
  int idx = findIndex(peer.mac);
  if (idx < 0) {
    idx = findFreeOrOldest();
    memset(&_entries[idx], 0, sizeof(_entries[idx]));
    memcpy(_entries[idx].mac, peer.mac, 6);
    _entries[idx].trust = PEER_TRUST_KNOWN;
    _entries[idx].active = true;
  }

  if (strlen(_entries[idx].nickname) == 0 && strlen(peer.unitName) > 0) {
    strncpy(_entries[idx].nickname, peer.unitName, sizeof(_entries[idx].nickname) - 1);
    _entries[idx].nickname[sizeof(_entries[idx].nickname) - 1] = '\0';
  }

  saveEntry(idx);
}

bool PeerDirectory::setNicknameBySuffix(const char* suffix, const char* nickname) {
  if (!suffix || !nickname || strlen(nickname) == 0 || strlen(nickname) > MAX_UNIT_NAME_LEN) {
    return false;
  }

  auto peers = meshMgr.getPeers();
  for (const auto& peer : peers) {
    if (!suffixMatches(peer.mac, suffix)) {
      continue;
    }

    observe(peer);
    int idx = findIndex(peer.mac);
    if (idx < 0) return false;
    strncpy(_entries[idx].nickname, nickname, sizeof(_entries[idx].nickname) - 1);
    _entries[idx].nickname[sizeof(_entries[idx].nickname) - 1] = '\0';
    saveEntry(idx);
    return true;
  }
  return false;
}

bool PeerDirectory::setTrustBySuffix(const char* suffix, PeerTrust trust) {
  if (!suffix) {
    return false;
  }

  auto peers = meshMgr.getPeers();
  for (const auto& peer : peers) {
    if (!suffixMatches(peer.mac, suffix)) {
      continue;
    }

    observe(peer);
    int idx = findIndex(peer.mac);
    if (idx < 0) return false;
    _entries[idx].trust = trust;
    saveEntry(idx);
    return true;
  }
  return false;
}

const PeerDirectoryEntry* PeerDirectory::find(const uint8_t* mac) const {
  int idx = findIndex(mac);
  return idx >= 0 ? &_entries[idx] : nullptr;
}

const char* PeerDirectory::displayName(const MeshPeerInfo& peer, char* out, size_t outSize) const {
  if (!out || outSize == 0) {
    return "";
  }

  const PeerDirectoryEntry* entry = find(peer.mac);
  if (entry && strlen(entry->nickname) > 0) {
    strncpy(out, entry->nickname, outSize - 1);
  } else if (strlen(peer.unitName) > 0) {
    strncpy(out, peer.unitName, outSize - 1);
  } else {
    snprintf(out, outSize, "%02X:%02X", peer.mac[4], peer.mac[5]);
  }
  out[outSize - 1] = '\0';
  return out;
}

const char* PeerDirectory::trustLabel(const uint8_t* mac) const {
  const PeerDirectoryEntry* entry = find(mac);
  if (!entry) {
    return "unknown";
  }

  switch (entry->trust) {
    case PEER_TRUST_TRUSTED: return "trusted";
    case PEER_TRUST_KNOWN: return "known";
    case PEER_TRUST_UNKNOWN:
    default: return "unknown";
  }
}

bool PeerDirectory::formatPeerLine(const MeshPeerInfo& peer, char* out, size_t outSize) const {
  if (!out || outSize == 0) {
    return false;
  }

  char name[MAX_UNIT_NAME_LEN + 1];
  displayName(peer, name, sizeof(name));
  unsigned long age = (millis() - peer.lastSeen) / 1000;
  snprintf(out, outSize, "%s %02X:%02X %ddBm h%d %lus %s",
           name,
           peer.mac[4],
           peer.mac[5],
           peer.rssi,
           peer.hopDistance,
           age,
           peer.isOnline ? "on" : "off");
  return true;
}

int PeerDirectory::findIndex(const uint8_t* mac) const {
  if (!mac) {
    return -1;
  }

  for (int i = 0; i < FIELD_PEER_DIRECTORY_SIZE; i++) {
    if (_entries[i].active && memcmp(_entries[i].mac, mac, 6) == 0) {
      return i;
    }
  }
  return -1;
}

int PeerDirectory::findFreeOrOldest() const {
  for (int i = 0; i < FIELD_PEER_DIRECTORY_SIZE; i++) {
    if (!_entries[i].active) {
      return i;
    }
  }
  return 0;
}

bool PeerDirectory::suffixMatches(const uint8_t* mac, const char* suffix) const {
  if (!mac || !suffix) {
    return false;
  }

  char compact[13];
  snprintf(compact, sizeof(compact), "%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  char shortSuffix[5];
  snprintf(shortSuffix, sizeof(shortSuffix), "%02X%02X", mac[4], mac[5]);

  String cleaned;
  for (const char* p = suffix; *p; p++) {
    if (isxdigit(*p)) {
      cleaned += (char)toupper(*p);
    }
  }
  return cleaned == shortSuffix || cleaned == compact;
}

void PeerDirectory::saveEntry(int index) {
  if (index < 0 || index >= FIELD_PEER_DIRECTORY_SIZE || !_entries[index].active) {
    return;
  }

  Preferences prefs;
  prefs.begin("peers", false);
  char key[12];
  snprintf(key, sizeof(key), "p%dmac", index);
  prefs.putBytes(key, _entries[index].mac, 6);
  snprintf(key, sizeof(key), "p%dname", index);
  prefs.putString(key, _entries[index].nickname);
  snprintf(key, sizeof(key), "p%dtrust", index);
  prefs.putUChar(key, (uint8_t)_entries[index].trust);
  prefs.end();
}

void PeerDirectory::loadEntry(int index) {
  if (index < 0 || index >= FIELD_PEER_DIRECTORY_SIZE) {
    return;
  }

  Preferences prefs;
  prefs.begin("peers", true);
  char key[12];
  snprintf(key, sizeof(key), "p%dmac", index);
  if (prefs.getBytesLength(key) == 6) {
    prefs.getBytes(key, _entries[index].mac, 6);
    snprintf(key, sizeof(key), "p%dname", index);
    prefs.getString(key, _entries[index].nickname, sizeof(_entries[index].nickname));
    snprintf(key, sizeof(key), "p%dtrust", index);
    _entries[index].trust = (PeerTrust)prefs.getUChar(key, PEER_TRUST_KNOWN);
    _entries[index].active = true;
  }
  prefs.end();
}

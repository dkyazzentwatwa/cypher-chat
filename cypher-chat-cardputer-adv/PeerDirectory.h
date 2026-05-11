#pragma once

#include "Config_Cardputer.h"
#include "MeshManager.h"

enum PeerTrust : uint8_t {
  PEER_TRUST_UNKNOWN,
  PEER_TRUST_KNOWN,
  PEER_TRUST_TRUSTED
};

struct PeerDirectoryEntry {
  uint8_t mac[6];
  char nickname[MAX_UNIT_NAME_LEN + 1];
  PeerTrust trust;
  bool active;
};

class PeerDirectory {
public:
  PeerDirectory();

  void begin();
  void observe(const MeshPeerInfo& peer);
  bool setNicknameBySuffix(const char* suffix, const char* nickname);
  bool setTrustBySuffix(const char* suffix, PeerTrust trust);
  const PeerDirectoryEntry* find(const uint8_t* mac) const;
  const char* displayName(const MeshPeerInfo& peer, char* out, size_t outSize) const;
  const char* trustLabel(const uint8_t* mac) const;
  bool formatPeerLine(const MeshPeerInfo& peer, char* out, size_t outSize) const;

private:
  PeerDirectoryEntry _entries[FIELD_PEER_DIRECTORY_SIZE];

  int findIndex(const uint8_t* mac) const;
  int findFreeOrOldest() const;
  bool suffixMatches(const uint8_t* mac, const char* suffix) const;
  void saveEntry(int index);
  void loadEntry(int index);
};

extern PeerDirectory peerDirectory;

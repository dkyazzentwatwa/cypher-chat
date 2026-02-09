#pragma once

#include <Arduino.h>
#include "Config_Starbeam.h"
#include <vector>
#include <Preferences.h>

/**
 * SecurityManager - Blocklist and trust list management (Stub implementation)
 */

class SecurityManager {
public:
  SecurityManager() {}

  bool begin() { return true; }
  bool isBlocked(const uint8_t* mac) { return false; }
  void blockPeer(const uint8_t* mac) {}
  void unblockPeer(const uint8_t* mac) {}
  bool isTrusted(const uint8_t* mac) { return false; }
  void trustPeer(const uint8_t* mac) {}
  std::vector<uint64_t> getBlocklist() { return std::vector<uint64_t>(); }
  std::vector<uint64_t> getTrustlist() { return std::vector<uint64_t>(); }
  void saveToNVS() {}
  void loadFromNVS() {}
};

extern SecurityManager securityMgr;

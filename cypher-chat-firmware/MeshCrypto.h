#pragma once

#include <Arduino.h>
#include <map>

struct MeshPacket;
struct MeshHeader;

#define MESH_CRYPTO_KEY_SIZE 32
#define MESH_CRYPTO_NONCE_SIZE 12
#define MESH_CRYPTO_TAG_SIZE 16
#define MESH_CRYPTO_OVERHEAD (MESH_CRYPTO_NONCE_SIZE + MESH_CRYPTO_TAG_SIZE)
#define MESH_CRYPTO_LMK_SIZE 16
#define MESH_CRYPTO_PMK_SIZE 16
#define MESH_REPLAY_MAX_PEERS 20

class MeshCrypto {
public:
  static bool init(const char* passphrase);

  static bool encryptPayload(MeshPacket* packet);
  static bool decryptPayload(MeshPacket* packet);

  static bool checkReplayCounter(const uint8_t* originMac, uint32_t messageId);
  static bool deriveLMK(const uint8_t* peerMac, uint8_t* lmkOut);
  static bool derivePMK(uint8_t* pmkOut);

  static bool savePassphrase(const char* passphrase);
  static bool loadPassphrase(char* buffer, size_t bufSize);
  static void saveReplayCounters();
  static void loadReplayCounters();
  static void clearReplayCounters();

  static bool isInitialized() { return _initialized; }

private:
  static bool deriveKeys(const char* passphrase);
  static void buildAAD(const MeshHeader* header, uint8_t* aad);
  static uint64_t macToKey(const uint8_t* mac);
  static bool deriveStorageKey(uint8_t* keyOut);

  static bool _initialized;
  static uint8_t _encKey[MESH_CRYPTO_KEY_SIZE];
  static uint8_t _lmkBaseKey[MESH_CRYPTO_LMK_SIZE];
  static uint8_t _pmkKey[MESH_CRYPTO_PMK_SIZE];
  static std::map<uint64_t, uint32_t> _replayCounters;
};

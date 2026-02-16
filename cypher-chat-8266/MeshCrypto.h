#pragma once

#include <Arduino.h>
#include <map>

/**
 * MeshCrypto - Application-layer encryption for ESP-NOW mesh (ESP8266)
 *
 * Wire-compatible with ESP32 cypher-chat MeshCrypto.
 * Uses BearSSL (shipped with ESP8266 Arduino core) instead of mbedTLS.
 *
 * Provides:
 * - HKDF-SHA256 key derivation from passphrase (RFC 5869)
 * - AES-256-GCM AEAD encryption (confidentiality + authentication)
 * - Per-sender monotonic replay counter protection
 * - LMK key derivation for ESP-NOW link-layer encryption
 * - EEPROM persistence for replay counters and passphrase
 *
 * Wire format for encrypted payload (identical to ESP32):
 *   [iv:12][ciphertext:N][gcm_tag:16]
 *
 * AAD (authenticated but not encrypted) = immutable header fields:
 *   [version:1][type:1][messageId:4][originMac:6][destMac:6][payloadLen:1]
 */

// Crypto constants - must match ESP32 version exactly
#define MESH_CRYPTO_KEY_SIZE       32   // AES-256-GCM key (256-bit)
#define MESH_CRYPTO_NONCE_SIZE     12   // AES-GCM IV/nonce
#define MESH_CRYPTO_TAG_SIZE       16   // GCM auth tag
#define MESH_CRYPTO_OVERHEAD       (MESH_CRYPTO_NONCE_SIZE + MESH_CRYPTO_TAG_SIZE)  // 28 bytes
#define MESH_CRYPTO_LMK_SIZE       16   // ESP-NOW LMK key size
#define MESH_CRYPTO_AAD_SIZE       19   // Immutable header fields for AAD

// Forward declaration
struct MeshPacket;
struct MeshHeader;

class MeshCrypto {
public:
  static bool init(const char* passphrase);
  static bool encryptPayload(MeshPacket* packet);
  static bool decryptPayload(MeshPacket* packet);
  static bool checkReplayCounter(const uint8_t* originMac, uint32_t messageId);
  static bool deriveLMK(const uint8_t* peerMac, uint8_t* lmkOut);
  static bool rotateKey(const char* newPassphrase);
  static void saveReplayCounters();
  static void loadReplayCounters();
  static void savePassphrase(const char* passphrase);
  static bool loadPassphrase(char* buffer, size_t bufSize);
  static bool isInitialized() { return _initialized; }

private:
  static bool deriveKeys(const char* passphrase);
  static void buildAAD(const MeshHeader* header, uint8_t* aad);
  static uint64_t macToKey(const uint8_t* mac);

  // HKDF-SHA256 (RFC 5869) - manual implementation using BearSSL HMAC
  static bool hkdfSha256(const uint8_t* salt, size_t saltLen,
                         const uint8_t* ikm, size_t ikmLen,
                         const uint8_t* info, size_t infoLen,
                         uint8_t* okm, size_t okmLen);

  // BearSSL HMAC-SHA256 helper
  static void hmacSha256(const uint8_t* key, size_t keyLen,
                         const uint8_t* data, size_t dataLen,
                         uint8_t output[32]);

  // State
  static bool _initialized;
  static uint8_t _encKey[MESH_CRYPTO_KEY_SIZE];
  static uint8_t _lmkBaseKey[MESH_CRYPTO_LMK_SIZE];

  // Replay counter tracking: MAC -> highest seen messageId
  static std::map<uint64_t, uint32_t> _replayCounters;
};

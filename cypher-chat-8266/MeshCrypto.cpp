#include "MeshCrypto.h"
#include "MeshManager.h"
#include "Config_8266.h"
#include <EEPROM.h>
#include <cstring>

// BearSSL headers (shipped with ESP8266 Arduino core)
#include <bearssl/bearssl_aead.h>
#include <bearssl/bearssl_hmac.h>
#include <bearssl/bearssl_hash.h>
#include <bearssl/bearssl_block.h>

// ESP8266 hardware RNG
extern "C" {
  #include <os_type.h>
  uint32_t os_random(void);
}

// Static member initialization
bool MeshCrypto::_initialized = false;
uint8_t MeshCrypto::_encKey[MESH_CRYPTO_KEY_SIZE] = {0};
uint8_t MeshCrypto::_lmkBaseKey[MESH_CRYPTO_LMK_SIZE] = {0};
std::map<uint64_t, uint32_t> MeshCrypto::_replayCounters;

// HKDF salt and info strings - MUST match ESP32 version exactly
static const char HKDF_SALT[] = "cypher-chat-mesh-v2";
static const char HKDF_INFO_ENC[] = "encryption-key";
static const char HKDF_INFO_LMK[] = "lmk-base-key";

// ============================================================================
// BearSSL HMAC-SHA256 helper
// ============================================================================
void MeshCrypto::hmacSha256(const uint8_t* key, size_t keyLen,
                            const uint8_t* data, size_t dataLen,
                            uint8_t output[32]) {
  br_hmac_key_context kc;
  br_hmac_key_init(&kc, &br_sha256_vtable, key, keyLen);

  br_hmac_context ctx;
  br_hmac_init(&ctx, &kc, 0);  // 0 = full hash output (32 bytes)
  br_hmac_update(&ctx, data, dataLen);
  br_hmac_out(&ctx, output);
}

// ============================================================================
// HKDF-SHA256 (RFC 5869) - manual implementation
// Produces identical output to mbedtls_hkdf() on ESP32
// ============================================================================
bool MeshCrypto::hkdfSha256(const uint8_t* salt, size_t saltLen,
                            const uint8_t* ikm, size_t ikmLen,
                            const uint8_t* info, size_t infoLen,
                            uint8_t* okm, size_t okmLen) {
  if (!ikm || ikmLen == 0 || !okm || okmLen == 0) return false;

  // HKDF-Extract: PRK = HMAC-SHA256(salt, IKM)
  uint8_t prk[32];
  hmacSha256(salt, saltLen, ikm, ikmLen, prk);

  // HKDF-Expand: OKM = T(1) || T(2) || ...
  // T(0) = empty
  // T(i) = HMAC-SHA256(PRK, T(i-1) || info || i)
  uint8_t T[32];
  size_t T_len = 0;
  size_t offset = 0;
  uint8_t counter = 1;

  while (offset < okmLen) {
    // Build HMAC input: T(i-1) || info || counter
    br_hmac_key_context kc;
    br_hmac_key_init(&kc, &br_sha256_vtable, prk, 32);

    br_hmac_context ctx;
    br_hmac_init(&ctx, &kc, 0);

    if (T_len > 0) {
      br_hmac_update(&ctx, T, T_len);
    }
    if (info && infoLen > 0) {
      br_hmac_update(&ctx, info, infoLen);
    }
    br_hmac_update(&ctx, &counter, 1);
    br_hmac_out(&ctx, T);

    T_len = 32;
    size_t copyLen = (okmLen - offset < 32) ? okmLen - offset : 32;
    memcpy(okm + offset, T, copyLen);
    offset += copyLen;
    counter++;
  }

  // Zeroize
  memset(prk, 0, sizeof(prk));
  memset(T, 0, sizeof(T));

  return true;
}

// ============================================================================
// Key derivation - identical logic to ESP32 version
// ============================================================================
bool MeshCrypto::init(const char* passphrase) {
  if (!passphrase || strlen(passphrase) == 0) {
    return false;
  }

  if (!deriveKeys(passphrase)) {
    return false;
  }

  loadReplayCounters();
  _initialized = true;
  return true;
}

bool MeshCrypto::deriveKeys(const char* passphrase) {
  size_t passphraseLen = strlen(passphrase);

  // Derive 32-byte encryption key via HKDF-SHA256
  if (!hkdfSha256(
    (const uint8_t*)HKDF_SALT, strlen(HKDF_SALT),
    (const uint8_t*)passphrase, passphraseLen,
    (const uint8_t*)HKDF_INFO_ENC, strlen(HKDF_INFO_ENC),
    _encKey, MESH_CRYPTO_KEY_SIZE)) {
    return false;
  }

  // Derive 16-byte LMK base key via HKDF-SHA256
  uint8_t lmkFull[32];
  if (!hkdfSha256(
    (const uint8_t*)HKDF_SALT, strlen(HKDF_SALT),
    (const uint8_t*)passphrase, passphraseLen,
    (const uint8_t*)HKDF_INFO_LMK, strlen(HKDF_INFO_LMK),
    lmkFull, 32)) {
    return false;
  }

  // Take first 16 bytes for LMK base
  memcpy(_lmkBaseKey, lmkFull, MESH_CRYPTO_LMK_SIZE);
  memset(lmkFull, 0, sizeof(lmkFull));

  return true;
}

// ============================================================================
// AAD construction - identical to ESP32 version
// ============================================================================
void MeshCrypto::buildAAD(const MeshHeader* header, uint8_t* aad) {
  // AAD = immutable header fields only (19 bytes)
  size_t offset = 0;
  aad[offset++] = header->version;
  aad[offset++] = header->type;
  memcpy(&aad[offset], &header->messageId, 4);
  offset += 4;
  memcpy(&aad[offset], header->originMac, 6);
  offset += 6;
  memcpy(&aad[offset], header->destMac, 6);
  offset += 6;
  aad[offset++] = header->payloadLen;
  // Total: 19 bytes = MESH_CRYPTO_AAD_SIZE
}

// ============================================================================
// AES-256-GCM Encryption using BearSSL
// Produces identical wire format to ESP32's mbedTLS implementation
// ============================================================================
bool MeshCrypto::encryptPayload(MeshPacket* packet) {
  if (!_initialized || !packet) return false;

  uint8_t payloadLen = packet->header.payloadLen;

  // Check that encrypted output fits in payload buffer
  if (payloadLen + MESH_CRYPTO_OVERHEAD > MESH_MAX_PAYLOAD) {
    return false;
  }

  // Copy plaintext out of payload buffer temporarily
  uint8_t plaintext[MESH_MAX_PAYLOAD];
  memcpy(plaintext, packet->payload, payloadLen);

  // Generate random 12-byte IV using hardware RNG
  uint8_t iv[MESH_CRYPTO_NONCE_SIZE];
  uint32_t rng1 = os_random();
  uint32_t rng2 = os_random();
  uint32_t rng3 = os_random();
  memcpy(&iv[0], &rng1, 4);
  memcpy(&iv[4], &rng2, 4);
  memcpy(&iv[8], &rng3, 4);

  // Build AAD from immutable header fields
  uint8_t aad[MESH_CRYPTO_AAD_SIZE];
  buildAAD(&packet->header, aad);

  // Encrypt with AES-256-GCM using BearSSL
  br_aes_ct_ctr_keys bc;
  br_aes_ct_ctr_init(&bc, _encKey, MESH_CRYPTO_KEY_SIZE);

  br_gcm_context gc;
  br_gcm_init(&gc, &bc.vtable, br_ghash_ctmul32);

  br_gcm_reset(&gc, iv, MESH_CRYPTO_NONCE_SIZE);
  br_gcm_aad_inject(&gc, aad, MESH_CRYPTO_AAD_SIZE);
  br_gcm_flip(&gc);

  // Encrypt in-place (copy plaintext to ciphertext buffer first)
  uint8_t ciphertext[MESH_MAX_PAYLOAD];
  memcpy(ciphertext, plaintext, payloadLen);
  br_gcm_run(&gc, 1, ciphertext, payloadLen);  // 1 = encrypt

  // Get authentication tag
  uint8_t tag[MESH_CRYPTO_TAG_SIZE];
  br_gcm_get_tag(&gc, tag);

  // Pack into payload buffer: [iv:12][ciphertext:N][tag:16]
  size_t offset = 0;
  memcpy(&packet->payload[offset], iv, MESH_CRYPTO_NONCE_SIZE);
  offset += MESH_CRYPTO_NONCE_SIZE;
  memcpy(&packet->payload[offset], ciphertext, payloadLen);
  offset += payloadLen;
  memcpy(&packet->payload[offset], tag, MESH_CRYPTO_TAG_SIZE);

  // Zeroize plaintext
  memset(plaintext, 0, sizeof(plaintext));

  return true;
}

// ============================================================================
// AES-256-GCM Decryption using BearSSL
// ============================================================================
bool MeshCrypto::decryptPayload(MeshPacket* packet) {
  if (!_initialized || !packet) return false;

  uint8_t payloadLen = packet->header.payloadLen;

  // Verify minimum size
  size_t totalEncrypted = (size_t)MESH_CRYPTO_NONCE_SIZE + payloadLen + MESH_CRYPTO_TAG_SIZE;
  if (totalEncrypted > MESH_MAX_PAYLOAD) {
    return false;
  }

  // Extract IV from payload buffer
  uint8_t iv[MESH_CRYPTO_NONCE_SIZE];
  memcpy(iv, &packet->payload[0], MESH_CRYPTO_NONCE_SIZE);

  // Extract ciphertext
  uint8_t ciphertext[MESH_MAX_PAYLOAD];
  memcpy(ciphertext, &packet->payload[MESH_CRYPTO_NONCE_SIZE], payloadLen);

  // Extract tag
  uint8_t tag[MESH_CRYPTO_TAG_SIZE];
  memcpy(tag, &packet->payload[MESH_CRYPTO_NONCE_SIZE + payloadLen], MESH_CRYPTO_TAG_SIZE);

  // Build AAD from immutable header fields
  uint8_t aad[MESH_CRYPTO_AAD_SIZE];
  buildAAD(&packet->header, aad);

  // Decrypt and verify with AES-256-GCM using BearSSL
  br_aes_ct_ctr_keys bc;
  br_aes_ct_ctr_init(&bc, _encKey, MESH_CRYPTO_KEY_SIZE);

  br_gcm_context gc;
  br_gcm_init(&gc, &bc.vtable, br_ghash_ctmul32);

  br_gcm_reset(&gc, iv, MESH_CRYPTO_NONCE_SIZE);
  br_gcm_aad_inject(&gc, aad, MESH_CRYPTO_AAD_SIZE);
  br_gcm_flip(&gc);

  // Decrypt in-place
  br_gcm_run(&gc, 0, ciphertext, payloadLen);  // 0 = decrypt

  // Verify authentication tag
  if (!br_gcm_check_tag(&gc, tag)) {
    // Authentication failed - tampered or wrong key
    return false;
  }

  // Restore plaintext to payload buffer
  memcpy(packet->payload, ciphertext, payloadLen);

  // Zeroize temporary buffers
  memset(ciphertext, 0, sizeof(ciphertext));

  return true;
}

// ============================================================================
// Replay protection - identical logic to ESP32
// ============================================================================
bool MeshCrypto::checkReplayCounter(const uint8_t* originMac, uint32_t messageId) {
  if (!originMac) return false;

  uint64_t key = macToKey(originMac);
  auto it = _replayCounters.find(key);

  if (it == _replayCounters.end()) {
    _replayCounters[key] = messageId;
    if (_replayCounters.size() > MESH_REPLAY_MAX_PEERS) {
      _replayCounters.erase(_replayCounters.begin());
    }
    return true;
  }

  if (messageId <= it->second) {
    return false;
  }

  it->second = messageId;
  return true;
}

// ============================================================================
// LMK derivation using BearSSL HMAC-SHA256
// ============================================================================
bool MeshCrypto::deriveLMK(const uint8_t* peerMac, uint8_t* lmkOut) {
  if (!_initialized || !peerMac || !lmkOut) return false;

  // LMK = HMAC-SHA256(lmkBaseKey, peerMac)[0:16]
  uint8_t fullHash[32];
  hmacSha256(_lmkBaseKey, MESH_CRYPTO_LMK_SIZE, peerMac, 6, fullHash);

  memcpy(lmkOut, fullHash, MESH_CRYPTO_LMK_SIZE);
  memset(fullHash, 0, sizeof(fullHash));

  return true;
}

bool MeshCrypto::rotateKey(const char* newPassphrase) {
  if (!newPassphrase || strlen(newPassphrase) == 0) {
    return false;
  }

  if (!deriveKeys(newPassphrase)) {
    return false;
  }

  _replayCounters.clear();
  savePassphrase(newPassphrase);
  saveReplayCounters();

  return true;
}

// ============================================================================
// EEPROM persistence (replaces ESP32's Preferences/NVS)
// ============================================================================
void MeshCrypto::saveReplayCounters() {
  EEPROM.begin(EEPROM_SIZE);

  uint8_t count = min((size_t)_replayCounters.size(), (size_t)MESH_REPLAY_MAX_PEERS);
  EEPROM.write(EEPROM_ADDR_RC_COUNT, count);

  int idx = 0;
  for (auto& entry : _replayCounters) {
    if (idx >= MESH_REPLAY_MAX_PEERS) break;

    size_t addr = EEPROM_ADDR_RC_DATA + (idx * 10);

    // Convert uint64_t key back to 6 bytes
    uint8_t mac[6];
    uint64_t macKey = entry.first;
    for (int i = 5; i >= 0; i--) {
      mac[i] = macKey & 0xFF;
      macKey >>= 8;
    }

    // Write MAC (6 bytes) + messageId (4 bytes)
    for (int i = 0; i < 6; i++) {
      EEPROM.write(addr + i, mac[i]);
    }
    uint32_t msgId = entry.second;
    EEPROM.write(addr + 6, (msgId >> 0) & 0xFF);
    EEPROM.write(addr + 7, (msgId >> 8) & 0xFF);
    EEPROM.write(addr + 8, (msgId >> 16) & 0xFF);
    EEPROM.write(addr + 9, (msgId >> 24) & 0xFF);

    idx++;
  }

  EEPROM.commit();
  EEPROM.end();
}

void MeshCrypto::loadReplayCounters() {
  EEPROM.begin(EEPROM_SIZE);

  // Check magic byte
  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) {
    EEPROM.end();
    return;  // EEPROM not initialized
  }

  uint8_t count = EEPROM.read(EEPROM_ADDR_RC_COUNT);
  if (count > MESH_REPLAY_MAX_PEERS) count = MESH_REPLAY_MAX_PEERS;

  _replayCounters.clear();
  for (int idx = 0; idx < count; idx++) {
    size_t addr = EEPROM_ADDR_RC_DATA + (idx * 10);

    uint8_t mac[6];
    for (int i = 0; i < 6; i++) {
      mac[i] = EEPROM.read(addr + i);
    }

    uint32_t msgId = EEPROM.read(addr + 6)
                   | (EEPROM.read(addr + 7) << 8)
                   | (EEPROM.read(addr + 8) << 16)
                   | (EEPROM.read(addr + 9) << 24);

    if (msgId > 0) {
      _replayCounters[macToKey(mac)] = msgId;
    }
  }

  EEPROM.end();
}

void MeshCrypto::savePassphrase(const char* passphrase) {
  EEPROM.begin(EEPROM_SIZE);

  // Write magic byte
  EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);

  // Write passphrase length and data
  uint8_t len = strlen(passphrase);
  if (len > MAX_PASSPHRASE_LEN) len = MAX_PASSPHRASE_LEN;
  EEPROM.write(EEPROM_ADDR_PP_LEN, len);

  for (uint8_t i = 0; i < len; i++) {
    EEPROM.write(EEPROM_ADDR_PP_DATA + i, passphrase[i]);
  }
  EEPROM.write(EEPROM_ADDR_PP_DATA + len, '\0');

  EEPROM.commit();
  EEPROM.end();
}

bool MeshCrypto::loadPassphrase(char* buffer, size_t bufSize) {
  EEPROM.begin(EEPROM_SIZE);

  // Check magic byte
  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) {
    EEPROM.end();
    return false;
  }

  uint8_t len = EEPROM.read(EEPROM_ADDR_PP_LEN);
  if (len == 0 || len > MAX_PASSPHRASE_LEN || len >= bufSize) {
    EEPROM.end();
    return false;
  }

  for (uint8_t i = 0; i < len; i++) {
    buffer[i] = EEPROM.read(EEPROM_ADDR_PP_DATA + i);
  }
  buffer[len] = '\0';

  EEPROM.end();
  return true;
}

uint64_t MeshCrypto::macToKey(const uint8_t* mac) {
  uint64_t key = 0;
  for (int i = 0; i < 6; i++) {
    key = (key << 8) | mac[i];
  }
  return key;
}

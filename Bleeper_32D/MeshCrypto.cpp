#include "MeshCrypto.h"
#include "MeshManager.h"
#include <mbedtls/hkdf.h>
#include <mbedtls/md.h>
#include <mbedtls/chachapoly.h>
#include <esp_random.h>
#include <Preferences.h>
#include <cstring>

// Static member initialization
bool MeshCrypto::_initialized = false;
uint8_t MeshCrypto::_encKey[MESH_CRYPTO_KEY_SIZE] = {0};
uint8_t MeshCrypto::_lmkBaseKey[MESH_CRYPTO_LMK_SIZE] = {0};
std::map<uint64_t, uint32_t> MeshCrypto::_replayCounters;

// HKDF salt and info strings (domain separation)
static const char HKDF_SALT[] = "cypher-chat-mesh-v2";
static const char HKDF_INFO_ENC[] = "encryption-key";
static const char HKDF_INFO_LMK[] = "lmk-base-key";

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
  const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!md_info) return false;

  size_t passphraseLen = strlen(passphrase);

  // Derive 32-byte encryption key via HKDF-SHA256
  int ret = mbedtls_hkdf(
    md_info,
    (const uint8_t*)HKDF_SALT, strlen(HKDF_SALT),
    (const uint8_t*)passphrase, passphraseLen,
    (const uint8_t*)HKDF_INFO_ENC, strlen(HKDF_INFO_ENC),
    _encKey, MESH_CRYPTO_KEY_SIZE
  );
  if (ret != 0) return false;

  // Derive 16-byte LMK base key via HKDF-SHA256
  uint8_t lmkFull[32];
  ret = mbedtls_hkdf(
    md_info,
    (const uint8_t*)HKDF_SALT, strlen(HKDF_SALT),
    (const uint8_t*)passphrase, passphraseLen,
    (const uint8_t*)HKDF_INFO_LMK, strlen(HKDF_INFO_LMK),
    lmkFull, 32
  );
  if (ret != 0) return false;

  // Take first 16 bytes for LMK base
  memcpy(_lmkBaseKey, lmkFull, MESH_CRYPTO_LMK_SIZE);

  // Zeroize temporary
  memset(lmkFull, 0, sizeof(lmkFull));

  return true;
}

void MeshCrypto::buildAAD(const MeshHeader* header, uint8_t* aad) {
  // AAD = immutable header fields only (19 bytes)
  // These fields MUST NOT change during relay
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

  // Generate random 12-byte nonce using hardware RNG
  uint8_t nonce[MESH_CRYPTO_NONCE_SIZE];
  uint32_t rng1 = esp_random();
  uint32_t rng2 = esp_random();
  uint32_t rng3 = esp_random();
  memcpy(&nonce[0], &rng1, 4);
  memcpy(&nonce[4], &rng2, 4);
  memcpy(&nonce[8], &rng3, 4);

  // Build AAD from immutable header fields
  uint8_t aad[MESH_CRYPTO_AAD_SIZE];
  buildAAD(&packet->header, aad);

  // Encrypt with ChaCha20-Poly1305
  mbedtls_chachapoly_context ctx;
  mbedtls_chachapoly_init(&ctx);

  int ret = mbedtls_chachapoly_setkey(&ctx, _encKey);
  if (ret != 0) {
    mbedtls_chachapoly_free(&ctx);
    return false;
  }

  uint8_t ciphertext[MESH_MAX_PAYLOAD];
  uint8_t tag[MESH_CRYPTO_TAG_SIZE];

  ret = mbedtls_chachapoly_encrypt_and_tag(
    &ctx,
    payloadLen,            // plaintext length
    nonce,                 // 12-byte nonce
    aad, MESH_CRYPTO_AAD_SIZE,  // additional authenticated data
    plaintext,             // input plaintext
    ciphertext,            // output ciphertext
    tag                    // 16-byte auth tag
  );

  mbedtls_chachapoly_free(&ctx);

  if (ret != 0) {
    memset(plaintext, 0, sizeof(plaintext));
    return false;
  }

  // Pack into payload buffer: [nonce:12][ciphertext:N][tag:16]
  size_t offset = 0;
  memcpy(&packet->payload[offset], nonce, MESH_CRYPTO_NONCE_SIZE);
  offset += MESH_CRYPTO_NONCE_SIZE;
  memcpy(&packet->payload[offset], ciphertext, payloadLen);
  offset += payloadLen;
  memcpy(&packet->payload[offset], tag, MESH_CRYPTO_TAG_SIZE);

  // Zeroize plaintext
  memset(plaintext, 0, sizeof(plaintext));

  return true;
}

bool MeshCrypto::decryptPayload(MeshPacket* packet) {
  if (!_initialized || !packet) return false;

  uint8_t payloadLen = packet->header.payloadLen;

  // Verify minimum size (nonce + at least 0 bytes ciphertext + tag)
  size_t totalEncrypted = (size_t)MESH_CRYPTO_NONCE_SIZE + payloadLen + MESH_CRYPTO_TAG_SIZE;
  if (totalEncrypted > MESH_MAX_PAYLOAD) {
    return false;
  }

  // Extract nonce from payload buffer
  uint8_t nonce[MESH_CRYPTO_NONCE_SIZE];
  memcpy(nonce, &packet->payload[0], MESH_CRYPTO_NONCE_SIZE);

  // Extract ciphertext
  uint8_t ciphertext[MESH_MAX_PAYLOAD];
  memcpy(ciphertext, &packet->payload[MESH_CRYPTO_NONCE_SIZE], payloadLen);

  // Extract tag
  uint8_t tag[MESH_CRYPTO_TAG_SIZE];
  memcpy(tag, &packet->payload[MESH_CRYPTO_NONCE_SIZE + payloadLen], MESH_CRYPTO_TAG_SIZE);

  // Build AAD from immutable header fields
  uint8_t aad[MESH_CRYPTO_AAD_SIZE];
  buildAAD(&packet->header, aad);

  // Decrypt and verify with ChaCha20-Poly1305
  mbedtls_chachapoly_context ctx;
  mbedtls_chachapoly_init(&ctx);

  int ret = mbedtls_chachapoly_setkey(&ctx, _encKey);
  if (ret != 0) {
    mbedtls_chachapoly_free(&ctx);
    return false;
  }

  uint8_t plaintext[MESH_MAX_PAYLOAD];
  ret = mbedtls_chachapoly_auth_decrypt(
    &ctx,
    payloadLen,            // ciphertext length
    nonce,                 // 12-byte nonce
    aad, MESH_CRYPTO_AAD_SIZE,  // additional authenticated data
    tag,                   // 16-byte auth tag to verify
    ciphertext,            // input ciphertext
    plaintext              // output plaintext
  );

  mbedtls_chachapoly_free(&ctx);

  if (ret != 0) {
    // Authentication failed - tampered or wrong key
    return false;
  }

  // Restore plaintext to payload buffer
  memcpy(packet->payload, plaintext, payloadLen);

  // Zeroize temporary buffers
  memset(plaintext, 0, sizeof(plaintext));
  memset(ciphertext, 0, sizeof(ciphertext));

  return true;
}

bool MeshCrypto::checkReplayCounter(const uint8_t* originMac, uint32_t messageId) {
  if (!originMac) return false;

  uint64_t key = macToKey(originMac);
  auto it = _replayCounters.find(key);

  if (it == _replayCounters.end()) {
    // First message from this sender - accept and record
    _replayCounters[key] = messageId;

    // Evict oldest if too many tracked senders
    if (_replayCounters.size() > MESH_REPLAY_MAX_PEERS) {
      _replayCounters.erase(_replayCounters.begin());
    }
    return true;
  }

  // Check if messageId is strictly greater than last seen
  if (messageId <= it->second) {
    // Replay or out-of-order - reject
    return false;
  }

  // New message - update counter
  it->second = messageId;
  return true;
}

bool MeshCrypto::deriveLMK(const uint8_t* peerMac, uint8_t* lmkOut) {
  if (!_initialized || !peerMac || !lmkOut) return false;

  // LMK = HMAC-SHA256(lmkBaseKey, peerMac)[0:16]
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);

  const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  int ret = mbedtls_md_setup(&ctx, md_info, 1);
  if (ret != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }

  ret = mbedtls_md_hmac_starts(&ctx, _lmkBaseKey, MESH_CRYPTO_LMK_SIZE);
  if (ret != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }

  ret = mbedtls_md_hmac_update(&ctx, peerMac, 6);
  if (ret != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }

  uint8_t fullHash[32];
  ret = mbedtls_md_hmac_finish(&ctx, fullHash);
  mbedtls_md_free(&ctx);

  if (ret != 0) return false;

  // Take first 16 bytes as LMK
  memcpy(lmkOut, fullHash, MESH_CRYPTO_LMK_SIZE);
  memset(fullHash, 0, sizeof(fullHash));

  return true;
}

bool MeshCrypto::rotateKey(const char* newPassphrase) {
  if (!newPassphrase || strlen(newPassphrase) == 0) {
    return false;
  }

  // Re-derive keys
  if (!deriveKeys(newPassphrase)) {
    return false;
  }

  // Clear replay counters (new key epoch)
  _replayCounters.clear();

  // Save new passphrase to NVS
  savePassphrase(newPassphrase);
  saveReplayCounters();

  return true;
}

void MeshCrypto::saveReplayCounters() {
  Preferences prefs;
  prefs.begin("meshcrypto", false);

  // Store counter count
  uint8_t count = min((size_t)_replayCounters.size(), (size_t)MESH_REPLAY_MAX_PEERS);
  prefs.putUChar("rc_count", count);

  // Store each counter as mac (6 bytes) + msgId (4 bytes)
  int idx = 0;
  for (auto& entry : _replayCounters) {
    if (idx >= MESH_REPLAY_MAX_PEERS) break;

    char keyMac[16], keyId[16];
    snprintf(keyMac, sizeof(keyMac), "rc_mac_%d", idx);
    snprintf(keyId, sizeof(keyId), "rc_id_%d", idx);

    // Convert uint64_t key back to 6 bytes
    uint8_t mac[6];
    uint64_t macKey = entry.first;
    for (int i = 5; i >= 0; i--) {
      mac[i] = macKey & 0xFF;
      macKey >>= 8;
    }

    prefs.putBytes(keyMac, mac, 6);
    prefs.putUInt(keyId, entry.second);
    idx++;
  }

  prefs.end();
}

void MeshCrypto::loadReplayCounters() {
  Preferences prefs;
  prefs.begin("meshcrypto", true);  // Read-only

  uint8_t count = prefs.getUChar("rc_count", 0);

  _replayCounters.clear();
  for (int idx = 0; idx < count && idx < MESH_REPLAY_MAX_PEERS; idx++) {
    char keyMac[16], keyId[16];
    snprintf(keyMac, sizeof(keyMac), "rc_mac_%d", idx);
    snprintf(keyId, sizeof(keyId), "rc_id_%d", idx);

    uint8_t mac[6] = {0};
    prefs.getBytes(keyMac, mac, 6);
    uint32_t msgId = prefs.getUInt(keyId, 0);

    if (msgId > 0) {
      _replayCounters[macToKey(mac)] = msgId;
    }
  }

  prefs.end();
}

void MeshCrypto::savePassphrase(const char* passphrase) {
  Preferences prefs;
  prefs.begin("meshcrypto", false);
  prefs.putString("passphrase", passphrase);
  prefs.end();
}

bool MeshCrypto::loadPassphrase(char* buffer, size_t bufSize) {
  Preferences prefs;
  prefs.begin("meshcrypto", true);  // Read-only
  String saved = prefs.getString("passphrase", "");
  prefs.end();

  if (saved.length() == 0) {
    return false;
  }

  strncpy(buffer, saved.c_str(), bufSize - 1);
  buffer[bufSize - 1] = '\0';
  return true;
}

uint64_t MeshCrypto::macToKey(const uint8_t* mac) {
  uint64_t key = 0;
  for (int i = 0; i < 6; i++) {
    key = (key << 8) | mac[i];
  }
  return key;
}

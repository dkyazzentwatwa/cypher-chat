#include "MeshCrypto.h"
#include "MeshManager.h"
#include "Config.h"

#include <Preferences.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <mbedtls/gcm.h>
#include <mbedtls/hkdf.h>
#include <mbedtls/md.h>
#include <cstring>

bool MeshCrypto::_initialized = false;
uint8_t MeshCrypto::_encKey[MESH_CRYPTO_KEY_SIZE] = {0};
uint8_t MeshCrypto::_lmkBaseKey[MESH_CRYPTO_LMK_SIZE] = {0};
uint8_t MeshCrypto::_pmkKey[MESH_CRYPTO_PMK_SIZE] = {0};
std::map<uint64_t, uint32_t> MeshCrypto::_replayCounters;

static const char HKDF_SALT[] = "cypher-chat-mesh-v2";
static const char HKDF_INFO_ENC[] = "encryption-key";
static const char HKDF_INFO_LMK[] = "lmk-base-key";
static const char HKDF_INFO_PMK[] = "esp-now-pmk";

bool MeshCrypto::init(const char* passphrase) {
  if (!passphrase || strlen(passphrase) < MIN_PASSPHRASE_LEN || strlen(passphrase) > MAX_PASSPHRASE_LEN) {
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
  const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!mdInfo) return false;

  const size_t passphraseLen = strlen(passphrase);
  int ret = mbedtls_hkdf(
    mdInfo,
    reinterpret_cast<const uint8_t*>(HKDF_SALT), strlen(HKDF_SALT),
    reinterpret_cast<const uint8_t*>(passphrase), passphraseLen,
    reinterpret_cast<const uint8_t*>(HKDF_INFO_ENC), strlen(HKDF_INFO_ENC),
    _encKey, sizeof(_encKey)
  );
  if (ret != 0) return false;

  uint8_t lmkFull[32];
  ret = mbedtls_hkdf(
    mdInfo,
    reinterpret_cast<const uint8_t*>(HKDF_SALT), strlen(HKDF_SALT),
    reinterpret_cast<const uint8_t*>(passphrase), passphraseLen,
    reinterpret_cast<const uint8_t*>(HKDF_INFO_LMK), strlen(HKDF_INFO_LMK),
    lmkFull, sizeof(lmkFull)
  );
  if (ret != 0) return false;
  memcpy(_lmkBaseKey, lmkFull, sizeof(_lmkBaseKey));
  memset(lmkFull, 0, sizeof(lmkFull));

  uint8_t pmkFull[32];
  ret = mbedtls_hkdf(
    mdInfo,
    reinterpret_cast<const uint8_t*>(HKDF_SALT), strlen(HKDF_SALT),
    reinterpret_cast<const uint8_t*>(passphrase), passphraseLen,
    reinterpret_cast<const uint8_t*>(HKDF_INFO_PMK), strlen(HKDF_INFO_PMK),
    pmkFull, sizeof(pmkFull)
  );
  if (ret != 0) return false;
  memcpy(_pmkKey, pmkFull, sizeof(_pmkKey));
  memset(pmkFull, 0, sizeof(pmkFull));

  return true;
}

void MeshCrypto::buildAAD(const MeshHeader* header, uint8_t* aad) {
  memcpy(aad, header, sizeof(MeshHeader));
}

bool MeshCrypto::encryptPayload(MeshPacket* packet) {
  if (!_initialized || !packet) return false;

  const uint8_t payloadLen = packet->header.payloadLen;
  if (payloadLen > MESH_MAX_PLAINTEXT || payloadLen + MESH_CRYPTO_OVERHEAD > MESH_MAX_PAYLOAD) {
    return false;
  }

  uint8_t plaintext[MESH_MAX_PAYLOAD];
  memcpy(plaintext, packet->payload, payloadLen);

  uint8_t iv[MESH_CRYPTO_NONCE_SIZE];
  uint32_t rng[3] = {esp_random(), esp_random(), esp_random()};
  memcpy(iv, rng, sizeof(iv));

  uint8_t aad[sizeof(MeshHeader)];
  buildAAD(&packet->header, aad);

  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, _encKey, 256);
  if (ret != 0) {
    mbedtls_gcm_free(&ctx);
    memset(plaintext, 0, sizeof(plaintext));
    return false;
  }

  uint8_t ciphertext[MESH_MAX_PAYLOAD];
  uint8_t tag[MESH_CRYPTO_TAG_SIZE];
  ret = mbedtls_gcm_crypt_and_tag(
    &ctx,
    MBEDTLS_GCM_ENCRYPT,
    payloadLen,
    iv, sizeof(iv),
    aad, sizeof(aad),
    plaintext,
    ciphertext,
    sizeof(tag),
    tag
  );
  mbedtls_gcm_free(&ctx);

  memset(plaintext, 0, sizeof(plaintext));
  if (ret != 0) {
    memset(ciphertext, 0, sizeof(ciphertext));
    return false;
  }

  size_t offset = 0;
  memcpy(&packet->payload[offset], iv, sizeof(iv));
  offset += sizeof(iv);
  memcpy(&packet->payload[offset], ciphertext, payloadLen);
  offset += payloadLen;
  memcpy(&packet->payload[offset], tag, sizeof(tag));
  memset(ciphertext, 0, sizeof(ciphertext));
  return true;
}

bool MeshCrypto::decryptPayload(MeshPacket* packet) {
  if (!_initialized || !packet) return false;

  const uint8_t payloadLen = packet->header.payloadLen;
  if (payloadLen > MESH_MAX_PLAINTEXT || payloadLen + MESH_CRYPTO_OVERHEAD > MESH_MAX_PAYLOAD) {
    return false;
  }

  uint8_t iv[MESH_CRYPTO_NONCE_SIZE];
  memcpy(iv, packet->payload, sizeof(iv));

  uint8_t ciphertext[MESH_MAX_PAYLOAD];
  memcpy(ciphertext, &packet->payload[MESH_CRYPTO_NONCE_SIZE], payloadLen);

  uint8_t tag[MESH_CRYPTO_TAG_SIZE];
  memcpy(tag, &packet->payload[MESH_CRYPTO_NONCE_SIZE + payloadLen], sizeof(tag));

  uint8_t aad[sizeof(MeshHeader)];
  buildAAD(&packet->header, aad);

  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, _encKey, 256);
  if (ret != 0) {
    mbedtls_gcm_free(&ctx);
    memset(ciphertext, 0, sizeof(ciphertext));
    return false;
  }

  uint8_t plaintext[MESH_MAX_PAYLOAD];
  ret = mbedtls_gcm_auth_decrypt(
    &ctx,
    payloadLen,
    iv, sizeof(iv),
    aad, sizeof(aad),
    tag, sizeof(tag),
    ciphertext,
    plaintext
  );
  mbedtls_gcm_free(&ctx);
  memset(ciphertext, 0, sizeof(ciphertext));

  if (ret != 0) {
    return false;
  }

  memcpy(packet->payload, plaintext, payloadLen);
  memset(plaintext, 0, sizeof(plaintext));
  return true;
}

bool MeshCrypto::checkReplayCounter(const uint8_t* originMac, uint32_t messageId) {
  if (!originMac || messageId == 0) return false;

  const uint64_t key = macToKey(originMac);
  auto it = _replayCounters.find(key);
  if (it != _replayCounters.end() && messageId <= it->second) {
    return false;
  }

  _replayCounters[key] = messageId;
  if (_replayCounters.size() > MESH_REPLAY_MAX_PEERS) {
    _replayCounters.erase(_replayCounters.begin());
  }
  saveReplayCounters();
  return true;
}

bool MeshCrypto::deriveLMK(const uint8_t* peerMac, uint8_t* lmkOut) {
  if (!_initialized || !peerMac || !lmkOut) return false;

  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!mdInfo || mbedtls_md_setup(&ctx, mdInfo, 1) != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }

  int ret = mbedtls_md_hmac_starts(&ctx, _lmkBaseKey, sizeof(_lmkBaseKey));
  if (ret == 0) ret = mbedtls_md_hmac_update(&ctx, peerMac, 6);

  uint8_t fullHash[32];
  if (ret == 0) ret = mbedtls_md_hmac_finish(&ctx, fullHash);
  mbedtls_md_free(&ctx);

  if (ret != 0) return false;
  memcpy(lmkOut, fullHash, MESH_CRYPTO_LMK_SIZE);
  memset(fullHash, 0, sizeof(fullHash));
  return true;
}

bool MeshCrypto::derivePMK(uint8_t* pmkOut) {
  if (!_initialized || !pmkOut) return false;
  memcpy(pmkOut, _pmkKey, sizeof(_pmkKey));
  return true;
}

void MeshCrypto::saveReplayCounters() {
  Preferences prefs;
  if (!prefs.begin("meshcrypto", false)) return;

  const uint8_t count = min((size_t)_replayCounters.size(), (size_t)MESH_REPLAY_MAX_PEERS);
  prefs.putUChar("rc_count", count);

  int index = 0;
  for (auto& entry : _replayCounters) {
    if (index >= MESH_REPLAY_MAX_PEERS) break;

    char keyMac[16];
    char keyId[16];
    snprintf(keyMac, sizeof(keyMac), "rc_mac_%d", index);
    snprintf(keyId, sizeof(keyId), "rc_id_%d", index);

    uint8_t mac[6];
    uint64_t macKey = entry.first;
    for (int i = 5; i >= 0; i--) {
      mac[i] = macKey & 0xFF;
      macKey >>= 8;
    }

    prefs.putBytes(keyMac, mac, sizeof(mac));
    prefs.putUInt(keyId, entry.second);
    index++;
  }

  prefs.end();
}

void MeshCrypto::loadReplayCounters() {
  Preferences prefs;
  if (!prefs.begin("meshcrypto", true)) return;

  const uint8_t count = prefs.getUChar("rc_count", 0);
  _replayCounters.clear();
  for (uint8_t index = 0; index < count && index < MESH_REPLAY_MAX_PEERS; index++) {
    char keyMac[16];
    char keyId[16];
    snprintf(keyMac, sizeof(keyMac), "rc_mac_%d", index);
    snprintf(keyId, sizeof(keyId), "rc_id_%d", index);

    uint8_t mac[6] = {0};
    prefs.getBytes(keyMac, mac, sizeof(mac));
    const uint32_t messageId = prefs.getUInt(keyId, 0);
    if (messageId > 0) {
      _replayCounters[macToKey(mac)] = messageId;
    }
  }

  prefs.end();
}

void MeshCrypto::clearReplayCounters() {
  _replayCounters.clear();
  saveReplayCounters();
}

bool MeshCrypto::deriveStorageKey(uint8_t* keyOut) {
  if (!keyOut) return false;

  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);

  const char* salt = "cypher-chat-nvs-key";
  const char* info = "passphrase-storage";
  const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!mdInfo) return false;

  return mbedtls_hkdf(
    mdInfo,
    reinterpret_cast<const uint8_t*>(salt), strlen(salt),
    mac, sizeof(mac),
    reinterpret_cast<const uint8_t*>(info), strlen(info),
    keyOut, MESH_CRYPTO_KEY_SIZE
  ) == 0;
}

bool MeshCrypto::savePassphrase(const char* passphrase) {
  if (!passphrase || strlen(passphrase) < MIN_PASSPHRASE_LEN || strlen(passphrase) > MAX_PASSPHRASE_LEN) {
    return false;
  }

  uint8_t storageKey[MESH_CRYPTO_KEY_SIZE];
  if (!deriveStorageKey(storageKey)) return false;

  const size_t passphraseLen = strlen(passphrase);
  uint8_t iv[MESH_CRYPTO_NONCE_SIZE];
  uint32_t rng[3] = {esp_random(), esp_random(), esp_random()};
  memcpy(iv, rng, sizeof(iv));

  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, storageKey, 256);
  memset(storageKey, 0, sizeof(storageKey));
  if (ret != 0) {
    mbedtls_gcm_free(&ctx);
    return false;
  }

  uint8_t ciphertext[MAX_PASSPHRASE_LEN + 1];
  uint8_t tag[MESH_CRYPTO_TAG_SIZE];
  ret = mbedtls_gcm_crypt_and_tag(
    &ctx,
    MBEDTLS_GCM_ENCRYPT,
    passphraseLen,
    iv, sizeof(iv),
    nullptr, 0,
    reinterpret_cast<const uint8_t*>(passphrase),
    ciphertext,
    sizeof(tag),
    tag
  );
  mbedtls_gcm_free(&ctx);

  if (ret != 0) return false;

  Preferences prefs;
  if (!prefs.begin("meshcrypto", false)) return false;
  prefs.putBytes("pp_iv", iv, sizeof(iv));
  prefs.putBytes("pp_tag", tag, sizeof(tag));
  prefs.putBytes("pp_enc", ciphertext, passphraseLen);
  prefs.putUChar("pp_len", static_cast<uint8_t>(passphraseLen));
  prefs.remove("passphrase");
  prefs.end();
  memset(ciphertext, 0, sizeof(ciphertext));
  return true;
}

bool MeshCrypto::loadPassphrase(char* buffer, size_t bufSize) {
  if (!buffer || bufSize == 0) return false;

  Preferences prefs;
  if (!prefs.begin("meshcrypto", true)) return false;

  const uint8_t storedLen = prefs.getUChar("pp_len", 0);
  if (storedLen == 0 || storedLen >= bufSize || storedLen > MAX_PASSPHRASE_LEN) {
    prefs.end();
    return false;
  }

  uint8_t iv[MESH_CRYPTO_NONCE_SIZE];
  uint8_t tag[MESH_CRYPTO_TAG_SIZE];
  uint8_t ciphertext[MAX_PASSPHRASE_LEN + 1];
  prefs.getBytes("pp_iv", iv, sizeof(iv));
  prefs.getBytes("pp_tag", tag, sizeof(tag));
  prefs.getBytes("pp_enc", ciphertext, storedLen);
  prefs.end();

  uint8_t storageKey[MESH_CRYPTO_KEY_SIZE];
  if (!deriveStorageKey(storageKey)) {
    memset(ciphertext, 0, sizeof(ciphertext));
    return false;
  }

  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, storageKey, 256);
  memset(storageKey, 0, sizeof(storageKey));
  if (ret != 0) {
    mbedtls_gcm_free(&ctx);
    memset(ciphertext, 0, sizeof(ciphertext));
    return false;
  }

  uint8_t plaintext[MAX_PASSPHRASE_LEN + 1];
  ret = mbedtls_gcm_auth_decrypt(
    &ctx,
    storedLen,
    iv, sizeof(iv),
    nullptr, 0,
    tag, sizeof(tag),
    ciphertext,
    plaintext
  );
  mbedtls_gcm_free(&ctx);
  memset(ciphertext, 0, sizeof(ciphertext));

  if (ret != 0) return false;
  memcpy(buffer, plaintext, storedLen);
  buffer[storedLen] = '\0';
  memset(plaintext, 0, sizeof(plaintext));
  return strlen(buffer) >= MIN_PASSPHRASE_LEN && strlen(buffer) <= MAX_PASSPHRASE_LEN;
}

uint64_t MeshCrypto::macToKey(const uint8_t* mac) {
  uint64_t key = 0;
  for (int i = 0; i < 6; i++) {
    key = (key << 8) | mac[i];
  }
  return key;
}

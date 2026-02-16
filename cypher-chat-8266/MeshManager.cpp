#include "MeshManager.h"
#include "MeshCrypto.h"
#include "Config_8266.h"
#include "OutputManager.h"
#include <cstring>
#include <algorithm>

// ============================================================================
// Global instance and ESP-NOW C callbacks
// ============================================================================

MeshManager meshMgr;

// Broadcast MAC address
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ESP8266 ESP-NOW receive callback (C-style)
static void ICACHE_RAM_ATTR espnowRecvCb(uint8_t* mac, uint8_t* data, uint8_t len) {
  meshMgr.handleReceive(mac, data, len);
}

// ESP8266 ESP-NOW send callback (C-style)
static void espnowSendCb(uint8_t* mac, uint8_t status) {
  meshMgr.handleSendResult(mac, status);
}

static uint64_t macToKey(const uint8_t* mac) {
  uint64_t key = 0;
  for (int i = 0; i < 6; i++) {
    key = (key << 8) | mac[i];
  }
  return key;
}

// ============================================================================
// MeshManager Implementation
// ============================================================================

MeshManager::MeshManager()
  : _running(false)
  , _messageCallback(nullptr)
  , _peerCallback(nullptr)
  , _seenIndex(0)
  , _lastHeartbeat(0)
  , _lastPeerPrune(0)
  , _msgsSent(0)
  , _msgsReceived(0)
  , _msgsRelayed(0)
  , _msgsDropped(0)
  , _messageIdCounter(0)
{
  memset(_unitName, 0, sizeof(_unitName));
  memset(_myMac, 0, sizeof(_myMac));
  memset(_seenMessages, 0, sizeof(_seenMessages));
  memset(_storeQueue, 0, sizeof(_storeQueue));
  _myGPS = {0, 0, 0, false};
}

bool MeshManager::begin(const char* unitName, const char* passphrase) {
  if (_running) {
    return true;
  }

  // Store configuration
  strncpy(_unitName, unitName, sizeof(_unitName) - 1);
  _unitName[sizeof(_unitName) - 1] = '\0';

  // Initialize crypto with passphrase (HKDF key derivation)
  if (!MeshCrypto::init(passphrase)) {
    output.println("MeshManager: Crypto init failed");
    return false;
  }

  // Initialize WiFi in station mode (required for ESP-NOW)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Set WiFi channel
  wifi_set_channel(MESH_CHANNEL);

  // Get our MAC address
  WiFi.macAddress(_myMac);

  // Initialize ESP-NOW
  if (esp_now_init() != 0) {
    output.println("MeshManager: ESP-NOW init failed");
    return false;
  }

  // Set role to COMBO (both sender and receiver)
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

  // Register callbacks
  esp_now_register_recv_cb(espnowRecvCb);
  esp_now_register_send_cb(espnowSendCb);

  // Add broadcast peer for flood messages (no encryption at link layer)
  if (esp_now_add_peer((u8*)BROADCAST_MAC, ESP_NOW_ROLE_COMBO, MESH_CHANNEL, NULL, 0) != 0) {
    output.println("MeshManager: Failed to add broadcast peer");
    esp_now_deinit();
    return false;
  }

  // Generate initial message ID from MAC + time
  _messageIdCounter = (_myMac[4] << 24) | (_myMac[5] << 16) | (millis() & 0xFFFF);

  _running = true;
  _lastHeartbeat = millis();
  _lastPeerPrune = millis();

  output.print("MeshManager: Started on channel ");
  output.print(MESH_CHANNEL);
  output.print(", MAC: ");
  for (int i = 0; i < 6; i++) {
    if (i > 0) output.print(":");
    output.printf("%02X", _myMac[i]);
  }
  output.println();

  output.println("Security: AES-256-GCM AEAD + HKDF-SHA256 + replay protection");

  // Send initial heartbeat
  sendHeartbeat();

  return true;
}

void MeshManager::end() {
  if (!_running) return;

  MeshCrypto::saveReplayCounters();
  esp_now_deinit();
  _peerInfo.clear();

  _running = false;
  output.println("MeshManager: Stopped");
}

void MeshManager::handleReceive(uint8_t* senderMac, uint8_t* data, uint8_t len) {
  if (!_running || !data || len < sizeof(MeshHeader)) {
    return;
  }

  const MeshPacket* packet = reinterpret_cast<const MeshPacket*>(data);

  // ESP8266 ESP-NOW callback doesn't provide RSSI
  int8_t rssi = -50;

  processPacket(packet, senderMac, rssi);
}

void MeshManager::handleSendResult(uint8_t* mac, uint8_t status) {
  if (status != 0) {
    _msgsDropped++;
  }
}

void MeshManager::update() {
  if (!_running) return;

  unsigned long now = millis();

  // Send periodic heartbeat
  if (now - _lastHeartbeat >= MESH_HEARTBEAT_MS) {
    sendHeartbeat();
    _lastHeartbeat = now;
  }

  // Prune offline peers
  if (now - _lastPeerPrune >= MESH_PEER_TIMEOUT_MS / 2) {
    pruneOfflinePeers();
    _lastPeerPrune = now;
  }

  // Process store-and-forward queue
  processStoreQueue();
}

// Helper: calculate total wire size for an encrypted packet
static size_t wireSize(uint8_t payloadLen) {
  return sizeof(MeshHeader) + MESH_CRYPTO_NONCE_SIZE + payloadLen + MESH_CRYPTO_TAG_SIZE;
}

uint32_t MeshManager::broadcast(const uint8_t* data, size_t len, MeshMessageType type, uint8_t ttl) {
  if (!_running || data == nullptr || len == 0 || len > MESH_MAX_PLAINTEXT) {
    return 0;
  }

  MeshPacket packet;
  memset(&packet, 0, sizeof(packet));

  // Fill header
  packet.header.version = MESH_PROTOCOL_VERSION;
  packet.header.type = type;
  packet.header.ttl = ttl;
  packet.header.hopCount = 0;
  packet.header.messageId = generateMessageId();
  memcpy(packet.header.originMac, _myMac, 6);
  memcpy(packet.header.destMac, BROADCAST_MAC, 6);
  memcpy(packet.header.lastHopMac, _myMac, 6);
  packet.header.timestamp = millis();
  packet.header.payloadLen = len;
  packet.header.flags = 0;

  // Copy plaintext payload
  memcpy(packet.payload, data, len);

  // Encrypt payload (AEAD: AES-256-GCM)
  if (!MeshCrypto::encryptPayload(&packet)) {
    output.println("MeshManager: Failed to encrypt packet");
    return 0;
  }

  // Mark as seen to prevent echo
  markMessageSeen(packet.header.messageId);

  // Send via ESP-NOW broadcast
  size_t totalSize = wireSize(packet.header.payloadLen);

  int result = esp_now_send((u8*)BROADCAST_MAC, (u8*)&packet, totalSize);
  if (result == 0) {
    _msgsSent++;
    return packet.header.messageId;
  }

  output.println("MeshManager: Broadcast send failed");
  return 0;
}

uint32_t MeshManager::sendTo(const uint8_t* destMac, const uint8_t* data, size_t len, MeshMessageType type) {
  if (!_running || destMac == nullptr || data == nullptr || len == 0 || len > MESH_MAX_PLAINTEXT) {
    return 0;
  }

  MeshPacket packet;
  memset(&packet, 0, sizeof(packet));

  packet.header.version = MESH_PROTOCOL_VERSION;
  packet.header.type = type;
  packet.header.ttl = MESH_DEFAULT_TTL;
  packet.header.hopCount = 0;
  packet.header.messageId = generateMessageId();
  memcpy(packet.header.originMac, _myMac, 6);
  memcpy(packet.header.destMac, destMac, 6);
  memcpy(packet.header.lastHopMac, _myMac, 6);
  packet.header.timestamp = millis();
  packet.header.payloadLen = len;
  packet.header.flags = 0x01;  // Needs ACK

  memcpy(packet.payload, data, len);

  if (!MeshCrypto::encryptPayload(&packet)) {
    return 0;
  }

  markMessageSeen(packet.header.messageId);

  size_t totalSize = wireSize(packet.header.payloadLen);

  // Try direct send to peer first
  addEspNowPeer(destMac);
  if (esp_now_send((u8*)destMac, (u8*)&packet, totalSize) == 0) {
    _msgsSent++;
    return packet.header.messageId;
  }

  // Fall back to broadcast for mesh routing
  if (esp_now_send((u8*)BROADCAST_MAC, (u8*)&packet, totalSize) == 0) {
    _msgsSent++;
    return packet.header.messageId;
  }

  // Store for later delivery
  storeForDelivery(destMac, data, len);
  return 0;
}

uint32_t MeshManager::sendEmergency(const char* message, const GPSCoordinates* gps) {
  if (!_running || message == nullptr) {
    return 0;
  }

  // Build emergency payload
  uint8_t payload[MESH_MAX_PLAINTEXT];
  size_t offset = 0;

  uint8_t flags = 0;
  if (gps && gps->valid) {
    flags |= 0x01;
  }
  payload[offset++] = flags;

  if (gps && gps->valid) {
    memcpy(&payload[offset], &gps->latitude, 4);
    offset += 4;
    memcpy(&payload[offset], &gps->longitude, 4);
    offset += 4;
    memcpy(&payload[offset], &gps->altitude, 4);
    offset += 4;
  } else if (_myGPS.valid) {
    flags |= 0x01;
    payload[0] = flags;
    memcpy(&payload[offset], &_myGPS.latitude, 4);
    offset += 4;
    memcpy(&payload[offset], &_myGPS.longitude, 4);
    offset += 4;
    memcpy(&payload[offset], &_myGPS.altitude, 4);
    offset += 4;
  }

  size_t msgLen = strlen(message);
  if (offset + msgLen > sizeof(payload)) {
    msgLen = sizeof(payload) - offset;
  }
  memcpy(&payload[offset], message, msgLen);
  offset += msgLen;

  return broadcast(payload, offset, MESH_MSG_EMERGENCY, MESH_MAX_TTL);
}

bool MeshManager::storeForDelivery(const uint8_t* destMac, const uint8_t* data, size_t len) {
  if (!_running || destMac == nullptr || data == nullptr || len == 0) {
    return false;
  }

  // Find free slot or oldest entry
  int freeSlot = -1;
  uint32_t oldestTime = UINT32_MAX;
  int oldestSlot = 0;

  for (int i = 0; i < MESH_STORE_QUEUE_SIZE; i++) {
    if (!_storeQueue[i].active) {
      freeSlot = i;
      break;
    }
    if (_storeQueue[i].storedTime < oldestTime) {
      oldestTime = _storeQueue[i].storedTime;
      oldestSlot = i;
    }
  }

  int slot = (freeSlot >= 0) ? freeSlot : oldestSlot;

  MeshPacket packet;
  memset(&packet, 0, sizeof(packet));
  packet.header.version = MESH_PROTOCOL_VERSION;
  packet.header.type = MESH_MSG_FORWARD;
  packet.header.ttl = MESH_DEFAULT_TTL;
  packet.header.hopCount = 0;
  packet.header.messageId = generateMessageId();
  memcpy(packet.header.originMac, _myMac, 6);
  memcpy(packet.header.destMac, destMac, 6);
  memcpy(packet.header.lastHopMac, _myMac, 6);
  packet.header.timestamp = millis();
  packet.header.flags = 0x02;

  if (len > MESH_MAX_PLAINTEXT) {
    len = MESH_MAX_PLAINTEXT;
  }
  packet.header.payloadLen = len;
  memcpy(packet.payload, data, len);

  MeshCrypto::encryptPayload(&packet);

  _storeQueue[slot].packet = packet;
  memcpy(_storeQueue[slot].targetMac, destMac, 6);
  _storeQueue[slot].storedTime = millis();
  _storeQueue[slot].retryCount = 0;
  _storeQueue[slot].active = true;

  output.printf("MeshManager: Stored message for later delivery (%d/%d slots used)\n",
                getStoredMessageCount(), MESH_STORE_QUEUE_SIZE);

  return true;
}

void MeshManager::onMessage(MeshMessageCallback callback) {
  _messageCallback = callback;
}

void MeshManager::onPeerUpdate(MeshPeerCallback callback) {
  _peerCallback = callback;
}

std::vector<MeshPeerInfo> MeshManager::getPeers() const {
  std::vector<MeshPeerInfo> peers;
  peers.reserve(_peerInfo.size());
  for (const auto& entry : _peerInfo) {
    peers.push_back(entry.second);
  }
  return peers;
}

int MeshManager::getOnlinePeerCount() const {
  int count = 0;
  for (const auto& entry : _peerInfo) {
    if (entry.second.isOnline) count++;
  }
  return count;
}

void MeshManager::getMyMac(uint8_t* mac) const {
  if (mac) {
    memcpy(mac, _myMac, 6);
  }
}

void MeshManager::setGPS(const GPSCoordinates& gps) {
  _myGPS = gps;
}

void MeshManager::processPacket(const MeshPacket* packet, const uint8_t* senderMac, int8_t rssi) {
  // Verify protocol version
  if (packet->header.version != MESH_PROTOCOL_VERSION) {
    _msgsDropped++;
    return;
  }

  // Check deduplication before decryption
  if (isMessageSeen(packet->header.messageId)) {
    return;
  }

  // Decrypt and authenticate
  MeshPacket decrypted = *packet;
  if (!MeshCrypto::decryptPayload(&decrypted)) {
    output.println("MeshManager: AEAD decryption failed - wrong key or tampered");
    _msgsDropped++;
    return;
  }

  // Replay counter check
  if (!MeshCrypto::checkReplayCounter(decrypted.header.originMac, decrypted.header.messageId)) {
    output.println("MeshManager: Replay attack detected - message rejected");
    _msgsDropped++;
    return;
  }

  markMessageSeen(decrypted.header.messageId);
  _msgsReceived++;

  // Update peer info
  updatePeer(senderMac, rssi, nullptr, 1);

  if (memcmp(decrypted.header.originMac, senderMac, 6) != 0) {
    updatePeer(decrypted.header.originMac, -127, nullptr, decrypted.header.hopCount + 1);
  }

  // Handle by message type
  switch (decrypted.header.type) {
    case MESH_MSG_DATA:
    case MESH_MSG_FORWARD:
      handleDataMessage(&decrypted, senderMac, rssi);
      break;
    case MESH_MSG_EMERGENCY:
      handleEmergency(&decrypted, senderMac);
      break;
    case MESH_MSG_ACK:
      handleAck(&decrypted);
      break;
    case MESH_MSG_HEARTBEAT:
      handleHeartbeat(&decrypted, senderMac, rssi);
      break;
    default:
      output.printf("MeshManager: Unknown message type: 0x%02X\n", decrypted.header.type);
      break;
  }

  // Relay using original encrypted packet
  if (shouldRelay(&decrypted)) {
    MeshPacket relay = *packet;
    relayPacket(&relay);
  }
}

void MeshManager::handleDataMessage(const MeshPacket* packet, const uint8_t* senderMac, int8_t rssi) {
  bool isForUs = memcmp(packet->header.destMac, _myMac, 6) == 0 ||
                 memcmp(packet->header.destMac, BROADCAST_MAC, 6) == 0;

  if (isForUs) {
    if (_messageCallback) {
      _messageCallback(packet, senderMac, rssi);
    }

    // Send ACK if requested
    if (packet->header.flags & 0x01) {
      MeshPacket ack;
      memset(&ack, 0, sizeof(ack));
      ack.header.version = MESH_PROTOCOL_VERSION;
      ack.header.type = MESH_MSG_ACK;
      ack.header.ttl = MESH_DEFAULT_TTL;
      ack.header.hopCount = 0;
      ack.header.messageId = generateMessageId();
      memcpy(ack.header.originMac, _myMac, 6);
      memcpy(ack.header.destMac, packet->header.originMac, 6);
      memcpy(ack.header.lastHopMac, _myMac, 6);
      ack.header.timestamp = millis();

      memcpy(ack.payload, &packet->header.messageId, 4);
      ack.header.payloadLen = 4;

      MeshCrypto::encryptPayload(&ack);

      esp_now_send((u8*)BROADCAST_MAC, (u8*)&ack, wireSize(ack.header.payloadLen));
    }
  }
}

void MeshManager::handleEmergency(const MeshPacket* packet, const uint8_t* senderMac) {
  uint64_t senderKey = macToKey(packet->header.originMac);
  uint32_t now = millis();
  auto it = _emergencyRateLimit.find(senderKey);
  if (it != _emergencyRateLimit.end()) {
    if (now - it->second < MESH_EMERGENCY_COOLDOWN_MS) {
      return;
    }
  }
  _emergencyRateLimit[senderKey] = now;

  if (_messageCallback) {
    _messageCallback(packet, senderMac, 0);
  }
}

void MeshManager::handleAck(const MeshPacket* packet) {
  if (memcmp(packet->header.destMac, _myMac, 6) != 0) {
    return;
  }

  if (packet->header.payloadLen >= 4) {
    uint32_t originalMsgId;
    memcpy(&originalMsgId, packet->payload, 4);

    output.printf("MeshManager: ACK received for message %08lX\n",
                  (unsigned long)originalMsgId);

    for (int i = 0; i < MESH_STORE_QUEUE_SIZE; i++) {
      if (_storeQueue[i].active && _storeQueue[i].packet.header.messageId == originalMsgId) {
        _storeQueue[i].active = false;
        break;
      }
    }
  }
}

void MeshManager::handleHeartbeat(const MeshPacket* packet, const uint8_t* senderMac, int8_t rssi) {
  char peerName[17] = {0};
  if (packet->header.payloadLen > 0 && packet->header.payloadLen <= 16) {
    memcpy(peerName, packet->payload, packet->header.payloadLen);
    peerName[packet->header.payloadLen] = '\0';
  }

  updatePeer(senderMac, rssi, peerName, 1);
  deliverStoredMessages(senderMac);
}

bool MeshManager::shouldRelay(const MeshPacket* packet) {
  if (memcmp(packet->header.originMac, _myMac, 6) == 0) {
    return false;
  }
  if (packet->header.ttl <= 1) {
    return false;
  }
  if (packet->header.type == MESH_MSG_EMERGENCY) {
    return true;
  }
  if (memcmp(packet->header.destMac, BROADCAST_MAC, 6) == 0) {
    return true;
  }
  if (memcmp(packet->header.destMac, _myMac, 6) != 0) {
    return true;
  }
  return false;
}

void MeshManager::relayPacket(MeshPacket* packet) {
  packet->header.ttl--;
  packet->header.hopCount++;
  memcpy(packet->header.lastHopMac, _myMac, 6);

  if (esp_now_send((u8*)BROADCAST_MAC, (u8*)packet, wireSize(packet->header.payloadLen)) == 0) {
    _msgsRelayed++;
  }
}

bool MeshManager::isMessageSeen(uint32_t messageId) {
  for (int i = 0; i < MESH_MSG_CACHE_SIZE; i++) {
    if (_seenMessages[i] == messageId) {
      return true;
    }
  }
  return false;
}

void MeshManager::markMessageSeen(uint32_t messageId) {
  _seenMessages[_seenIndex] = messageId;
  _seenIndex = (_seenIndex + 1) % MESH_MSG_CACHE_SIZE;
}

void MeshManager::updatePeer(const uint8_t* mac, int8_t rssi, const char* unitName, uint8_t hopDistance) {
  if (mac == nullptr) return;
  if (memcmp(mac, _myMac, 6) == 0) return;
  if (memcmp(mac, BROADCAST_MAC, 6) == 0) return;

  uint64_t macKey = macToKey(mac);
  auto it = _peerInfo.find(macKey);
  bool isNew = (it == _peerInfo.end());

  if (isNew) {
    if (_peerInfo.size() >= MESH_MAX_PEERS) {
      uint64_t evictKey = 0;
      bool evictFound = false;

      for (const auto& entry : _peerInfo) {
        if (!entry.second.isOnline) {
          evictKey = entry.first;
          evictFound = true;
          break;
        }
      }

      if (!evictFound && !_peerInfo.empty()) {
        evictKey = _peerInfo.begin()->first;
        evictFound = true;
      }

      if (evictFound) {
        _peerInfo.erase(evictKey);
      }
    }

    MeshPeerInfo newPeer;
    memset(&newPeer, 0, sizeof(newPeer));
    memcpy(newPeer.mac, mac, 6);
    newPeer.rssi = rssi;
    newPeer.hopDistance = hopDistance;
    newPeer.lastSeen = millis();
    newPeer.isOnline = true;
    if (unitName && strlen(unitName) > 0) {
      strncpy(newPeer.unitName, unitName, sizeof(newPeer.unitName) - 1);
      newPeer.unitName[sizeof(newPeer.unitName) - 1] = '\0';
    }
    _peerInfo[macKey] = newPeer;
    it = _peerInfo.find(macKey);
  }

  // Update peer info
  MeshPeerInfo& peer = it->second;
  if (rssi != -127) {
    peer.rssi = rssi;
  }
  peer.lastSeen = millis();
  peer.hopDistance = hopDistance;
  peer.isOnline = true;

  if (unitName && strlen(unitName) > 0) {
    strncpy(peer.unitName, unitName, sizeof(peer.unitName) - 1);
    peer.unitName[sizeof(peer.unitName) - 1] = '\0';
  }

  // Ensure ESP-NOW peer is registered
  addEspNowPeer(mac);

  if (_peerCallback) {
    _peerCallback(&peer, isNew);
  }
}

void MeshManager::pruneOfflinePeers() {
  uint32_t now = millis();

  for (auto& entry : _peerInfo) {
    MeshPeerInfo& peer = entry.second;
    if (peer.isOnline && (now - peer.lastSeen > MESH_PEER_TIMEOUT_MS)) {
      peer.isOnline = false;
      if (_peerCallback) {
        _peerCallback(&peer, false);
      }
    }
  }
}

MeshPeerInfo* MeshManager::findPeer(const uint8_t* mac) {
  if (!mac) return nullptr;
  uint64_t macKey = macToKey(mac);
  auto it = _peerInfo.find(macKey);
  if (it == _peerInfo.end()) return nullptr;
  return &it->second;
}

void MeshManager::processStoreQueue() {
  uint32_t now = millis();

  for (int i = 0; i < MESH_STORE_QUEUE_SIZE; i++) {
    if (!_storeQueue[i].active) continue;

    if (now - _storeQueue[i].storedTime > MESH_MSG_EXPIRE_MS) {
      _storeQueue[i].active = false;
      output.printf("MeshManager: Stored message %08lX expired\n",
                    (unsigned long)_storeQueue[i].packet.header.messageId);
      continue;
    }

    MeshPeerInfo* peer = findPeer(_storeQueue[i].targetMac);
    if (peer && peer->isOnline) {
      deliverStoredMessages(_storeQueue[i].targetMac);
    }
  }
}

void MeshManager::deliverStoredMessages(const uint8_t* peerMac) {
  for (int i = 0; i < MESH_STORE_QUEUE_SIZE; i++) {
    if (!_storeQueue[i].active) continue;

    if (memcmp(_storeQueue[i].targetMac, peerMac, 6) == 0) {
      MeshPacket* packet = &_storeQueue[i].packet;
      packet->header.timestamp = millis();
      memcpy(packet->header.lastHopMac, _myMac, 6);

      if (esp_now_send((u8*)BROADCAST_MAC, (u8*)packet, wireSize(packet->header.payloadLen)) == 0) {
        output.printf("MeshManager: Delivered stored message %08lX\n",
                      (unsigned long)packet->header.messageId);
        _msgsSent++;
        _storeQueue[i].retryCount++;
        if (_storeQueue[i].retryCount >= 3) {
          _storeQueue[i].active = false;
        }
      }
    }
  }
}

void MeshManager::sendHeartbeat() {
  size_t nameLen = strlen(_unitName);
  broadcast((uint8_t*)_unitName, nameLen, MESH_MSG_HEARTBEAT, 1);
}

uint32_t MeshManager::generateMessageId() {
  return ++_messageIdCounter;
}

bool MeshManager::addEspNowPeer(const uint8_t* mac) {
  if (!mac) return false;

  // ESP8266: esp_now_add_peer returns 0 on success, or if peer already exists
  // No LMK for ESP8266 (application-layer AES-256-GCM provides security)
  int result = esp_now_add_peer((u8*)mac, ESP_NOW_ROLE_COMBO, MESH_CHANNEL, NULL, 0);
  return (result == 0);
}

int MeshManager::getStoredMessageCount() const {
  int count = 0;
  for (int i = 0; i < MESH_STORE_QUEUE_SIZE; i++) {
    if (_storeQueue[i].active) count++;
  }
  return count;
}

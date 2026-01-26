#include "MeshManager.h"
#include "MessageAuth.h"
#include "Config.h"
#include <cstring>
#include <algorithm>

// Singleton instance
MeshManager* MeshManager::_instance = nullptr;
MeshManager meshMgr;

// Broadcast MAC address
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

MeshManager::MeshManager()
  : _running(false)
  , _passkey(0)
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
  _instance = this;
}

bool MeshManager::begin(const char* unitName, uint32_t passkey) {
  if (_running) {
    return true;  // Already running
  }

  // Store configuration
  strncpy(_unitName, unitName, sizeof(_unitName) - 1);
  _unitName[sizeof(_unitName) - 1] = '\0';
  _passkey = passkey;

  // Initialize WiFi in station mode (required for ESP-NOW)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Get our MAC address
  WiFi.macAddress(_myMac);

  // Set WiFi channel
  esp_wifi_set_channel(MESH_CHANNEL, WIFI_SECOND_CHAN_NONE);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("MeshManager: ESP-NOW init failed");
    return false;
  }

  // Register callbacks
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // Add broadcast peer for flood messages
  addEspNowPeer(BROADCAST_MAC);

  // Generate initial message ID from MAC + time
  _messageIdCounter = (_myMac[4] << 24) | (_myMac[5] << 16) | (millis() & 0xFFFF);

  _running = true;
  _lastHeartbeat = millis();
  _lastPeerPrune = millis();

  Serial.print("MeshManager: Started on channel ");
  Serial.print(MESH_CHANNEL);
  Serial.print(", MAC: ");
  for (int i = 0; i < 6; i++) {
    if (i > 0) Serial.print(":");
    Serial.printf("%02X", _myMac[i]);
  }
  Serial.println();

  // Send initial heartbeat to announce presence
  sendHeartbeat();

  return true;
}

void MeshManager::end() {
  if (!_running) return;

  esp_now_unregister_send_cb();
  esp_now_unregister_recv_cb();
  esp_now_deinit();

  _running = false;
  _peers.clear();

  Serial.println("MeshManager: Stopped");
}

void MeshManager::update() {
  if (!_running) return;

  unsigned long now = millis();

  // Send periodic heartbeat
  if (now - _lastHeartbeat >= MESH_HEARTBEAT_MS) {
    sendHeartbeat();
    _lastHeartbeat = now;
  }

  // Prune offline peers periodically
  if (now - _lastPeerPrune >= MESH_PEER_TIMEOUT_MS / 2) {
    pruneOfflinePeers();
    _lastPeerPrune = now;
  }

  // Process store-and-forward queue
  processStoreQueue();
}

uint32_t MeshManager::broadcast(const uint8_t* data, size_t len, MeshMessageType type, uint8_t ttl) {
  if (!_running || data == nullptr || len == 0 || len > MESH_MAX_PAYLOAD - HMAC_SIZE) {
    return 0;
  }

  MeshPacket packet;
  memset(&packet, 0, sizeof(packet));

  // Fill header
  packet.header.version = 0x01;
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

  // Copy payload
  memcpy(packet.payload, data, len);

  // Sign packet
  if (!signPacket(&packet)) {
    Serial.println("MeshManager: Failed to sign packet");
    return 0;
  }

  // Mark as seen to prevent echo
  markMessageSeen(packet.header.messageId);

  // Send via ESP-NOW broadcast
  esp_err_t result = esp_now_send(BROADCAST_MAC, (uint8_t*)&packet, sizeof(MeshHeader) + len + HMAC_SIZE);
  if (result == ESP_OK) {
    _msgsSent++;
    return packet.header.messageId;
  }

  Serial.println("MeshManager: Broadcast send failed");
  return 0;
}

uint32_t MeshManager::sendTo(const uint8_t* destMac, const uint8_t* data, size_t len, MeshMessageType type) {
  if (!_running || destMac == nullptr || data == nullptr || len == 0 || len > MESH_MAX_PAYLOAD - HMAC_SIZE) {
    return 0;
  }

  MeshPacket packet;
  memset(&packet, 0, sizeof(packet));

  // Fill header
  packet.header.version = 0x01;
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

  // Copy payload
  memcpy(packet.payload, data, len);

  // Sign packet
  if (!signPacket(&packet)) {
    return 0;
  }

  // Mark as seen
  markMessageSeen(packet.header.messageId);

  // Check if peer is known and online
  MeshPeerInfo* peer = findPeer(destMac);
  if (peer && peer->isOnline && peer->hopDistance == 1) {
    // Direct send to known peer
    addEspNowPeer(destMac);
    esp_err_t result = esp_now_send(destMac, (uint8_t*)&packet, sizeof(MeshHeader) + len + HMAC_SIZE);
    if (result == ESP_OK) {
      _msgsSent++;
      return packet.header.messageId;
    }
  }

  // Peer not directly reachable - broadcast for mesh routing
  esp_err_t result = esp_now_send(BROADCAST_MAC, (uint8_t*)&packet, sizeof(MeshHeader) + len + HMAC_SIZE);
  if (result == ESP_OK) {
    _msgsSent++;
    return packet.header.messageId;
  }

  // Store for later delivery if send fails
  storeForDelivery(destMac, data, len);
  return 0;
}

uint32_t MeshManager::sendEmergency(const char* message, const GPSCoordinates* gps) {
  if (!_running || message == nullptr) {
    return 0;
  }

  // Build emergency payload
  // Format: [flags:1][lat:4][lon:4][alt:4][msg:variable]
  uint8_t payload[MESH_MAX_PAYLOAD - HMAC_SIZE];
  size_t offset = 0;

  // Flags byte
  uint8_t flags = 0;
  if (gps && gps->valid) {
    flags |= 0x01;  // Has GPS
  }
  payload[offset++] = flags;

  // GPS coordinates if available
  if (gps && gps->valid) {
    memcpy(&payload[offset], &gps->latitude, 4);
    offset += 4;
    memcpy(&payload[offset], &gps->longitude, 4);
    offset += 4;
    memcpy(&payload[offset], &gps->altitude, 4);
    offset += 4;
  } else if (_myGPS.valid) {
    // Use device's stored GPS
    flags |= 0x01;
    payload[0] = flags;
    memcpy(&payload[offset], &_myGPS.latitude, 4);
    offset += 4;
    memcpy(&payload[offset], &_myGPS.longitude, 4);
    offset += 4;
    memcpy(&payload[offset], &_myGPS.altitude, 4);
    offset += 4;
  }

  // Message content
  size_t msgLen = strlen(message);
  if (offset + msgLen > sizeof(payload)) {
    msgLen = sizeof(payload) - offset;
  }
  memcpy(&payload[offset], message, msgLen);
  offset += msgLen;

  // Send with maximum TTL for emergency
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

  // Build packet for storage
  MeshPacket packet;
  memset(&packet, 0, sizeof(packet));
  packet.header.version = 0x01;
  packet.header.type = MESH_MSG_FORWARD;
  packet.header.ttl = MESH_DEFAULT_TTL;
  packet.header.hopCount = 0;
  packet.header.messageId = generateMessageId();
  memcpy(packet.header.originMac, _myMac, 6);
  memcpy(packet.header.destMac, destMac, 6);
  memcpy(packet.header.lastHopMac, _myMac, 6);
  packet.header.timestamp = millis();
  packet.header.payloadLen = len;
  packet.header.flags = 0x02;  // Is relay/stored

  if (len > MESH_MAX_PAYLOAD - HMAC_SIZE) {
    len = MESH_MAX_PAYLOAD - HMAC_SIZE;
  }
  memcpy(packet.payload, data, len);

  // Sign packet
  signPacket(&packet);

  // Store entry
  _storeQueue[slot].packet = packet;
  memcpy(_storeQueue[slot].targetMac, destMac, 6);
  _storeQueue[slot].storedTime = millis();
  _storeQueue[slot].retryCount = 0;
  _storeQueue[slot].active = true;

  Serial.printf("MeshManager: Stored message for later delivery (%d/%d slots used)\n",
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
  return _peers;
}

int MeshManager::getOnlinePeerCount() const {
  int count = 0;
  for (const auto& peer : _peers) {
    if (peer.isOnline) count++;
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

// Static ESP-NOW callbacks
void MeshManager::onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
  // Could track delivery status here if needed
  if (status != ESP_NOW_SEND_SUCCESS) {
    if (_instance) {
      _instance->_msgsDropped++;
    }
  }
}

void MeshManager::onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (_instance == nullptr || !_instance->_running) return;
  if (len < (int)sizeof(MeshHeader)) return;

  const MeshPacket* packet = (const MeshPacket*)data;
  _instance->processPacket(packet, info->src_addr, info->rx_ctrl->rssi);
}

void MeshManager::processPacket(const MeshPacket* packet, const uint8_t* senderMac, int8_t rssi) {
  // Verify protocol version
  if (packet->header.version != 0x01) {
    Serial.println("MeshManager: Unknown protocol version");
    _msgsDropped++;
    return;
  }

  // Verify HMAC
  if (!verifyPacket(packet)) {
    Serial.println("MeshManager: HMAC verification failed - spoofed/corrupted");
    _msgsDropped++;
    return;
  }

  // Check if message already seen (deduplication)
  if (isMessageSeen(packet->header.messageId)) {
    // Already processed - don't relay again
    return;
  }

  // Mark as seen
  markMessageSeen(packet->header.messageId);

  _msgsReceived++;

  // Update peer info from sender
  updatePeer(senderMac, rssi, nullptr, 1);

  // Update peer info from origin (multi-hop)
  if (memcmp(packet->header.originMac, senderMac, 6) != 0) {
    updatePeer(packet->header.originMac, -127, nullptr, packet->header.hopCount + 1);
  }

  // Handle by message type
  switch (packet->header.type) {
    case MESH_MSG_DATA:
    case MESH_MSG_FORWARD:
      handleDataMessage(packet, senderMac, rssi);
      break;

    case MESH_MSG_EMERGENCY:
      handleEmergency(packet, senderMac);
      break;

    case MESH_MSG_ACK:
      handleAck(packet);
      break;

    case MESH_MSG_HEARTBEAT:
      handleHeartbeat(packet, senderMac, rssi);
      break;

    default:
      Serial.printf("MeshManager: Unknown message type: 0x%02X\n", packet->header.type);
      break;
  }
}

void MeshManager::handleDataMessage(const MeshPacket* packet, const uint8_t* senderMac, int8_t rssi) {
  // Check if message is for us
  bool isForUs = memcmp(packet->header.destMac, _myMac, 6) == 0 ||
                 memcmp(packet->header.destMac, BROADCAST_MAC, 6) == 0;

  if (isForUs) {
    // Deliver to application
    if (_messageCallback) {
      _messageCallback(packet, senderMac, rssi);
    }

    // Send ACK if requested
    if (packet->header.flags & 0x01) {
      // Build ACK packet
      MeshPacket ack;
      memset(&ack, 0, sizeof(ack));
      ack.header.version = 0x01;
      ack.header.type = MESH_MSG_ACK;
      ack.header.ttl = MESH_DEFAULT_TTL;
      ack.header.hopCount = 0;
      ack.header.messageId = generateMessageId();
      memcpy(ack.header.originMac, _myMac, 6);
      memcpy(ack.header.destMac, packet->header.originMac, 6);
      memcpy(ack.header.lastHopMac, _myMac, 6);
      ack.header.timestamp = millis();

      // ACK payload contains original message ID
      memcpy(ack.payload, &packet->header.messageId, 4);
      ack.header.payloadLen = 4;

      signPacket(&ack);

      // Send ACK
      esp_now_send(BROADCAST_MAC, (uint8_t*)&ack, sizeof(MeshHeader) + 4 + HMAC_SIZE);
    }
  }

  // Relay if needed
  if (shouldRelay(packet)) {
    MeshPacket relay = *packet;
    relayPacket(&relay);
  }
}

void MeshManager::handleEmergency(const MeshPacket* packet, const uint8_t* senderMac) {
  // Emergency messages are always delivered and always relayed
  if (_messageCallback) {
    _messageCallback(packet, senderMac, 0);
  }

  // Always relay emergencies (regardless of destination)
  if (shouldRelay(packet)) {
    MeshPacket relay = *packet;
    relayPacket(&relay);
  }
}

void MeshManager::handleAck(const MeshPacket* packet) {
  // Check if ACK is for us
  if (memcmp(packet->header.destMac, _myMac, 6) != 0) {
    // Relay ACK
    if (shouldRelay(packet)) {
      MeshPacket relay = *packet;
      relayPacket(&relay);
    }
    return;
  }

  // Extract original message ID from ACK payload
  if (packet->header.payloadLen >= 4) {
    uint32_t originalMsgId;
    memcpy(&originalMsgId, packet->payload, 4);

    // Could notify application of delivery confirmation here
    Serial.printf("MeshManager: ACK received for message %08X\n", originalMsgId);

    // Clear from store queue if present
    for (int i = 0; i < MESH_STORE_QUEUE_SIZE; i++) {
      if (_storeQueue[i].active && _storeQueue[i].packet.header.messageId == originalMsgId) {
        _storeQueue[i].active = false;
        break;
      }
    }
  }
}

void MeshManager::handleHeartbeat(const MeshPacket* packet, const uint8_t* senderMac, int8_t rssi) {
  // Extract unit name from heartbeat payload
  char peerName[17] = {0};
  if (packet->header.payloadLen > 0 && packet->header.payloadLen <= 16) {
    memcpy(peerName, packet->payload, packet->header.payloadLen);
    peerName[packet->header.payloadLen] = '\0';
  }

  // Update peer
  updatePeer(senderMac, rssi, peerName, 1);

  // Check if we have stored messages for this peer
  deliverStoredMessages(senderMac);
}

bool MeshManager::shouldRelay(const MeshPacket* packet) {
  // Don't relay if we're the origin
  if (memcmp(packet->header.originMac, _myMac, 6) == 0) {
    return false;
  }

  // Don't relay if TTL exhausted
  if (packet->header.ttl <= 1) {
    return false;
  }

  // Always relay emergencies
  if (packet->header.type == MESH_MSG_EMERGENCY) {
    return true;
  }

  // Relay broadcasts
  if (memcmp(packet->header.destMac, BROADCAST_MAC, 6) == 0) {
    return true;
  }

  // Relay unicast messages not for us
  if (memcmp(packet->header.destMac, _myMac, 6) != 0) {
    return true;
  }

  return false;
}

void MeshManager::relayPacket(MeshPacket* packet) {
  // Decrement TTL
  packet->header.ttl--;
  packet->header.hopCount++;

  // Update last hop
  memcpy(packet->header.lastHopMac, _myMac, 6);

  // Re-sign packet with updated header
  signPacket(packet);

  // Broadcast relay
  esp_err_t result = esp_now_send(BROADCAST_MAC, (uint8_t*)packet,
                                   sizeof(MeshHeader) + packet->header.payloadLen + HMAC_SIZE);
  if (result == ESP_OK) {
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

  // Don't track ourselves
  if (memcmp(mac, _myMac, 6) == 0) return;

  // Don't track broadcast address
  if (memcmp(mac, BROADCAST_MAC, 6) == 0) return;

  // Find existing peer
  MeshPeerInfo* peer = findPeer(mac);
  bool isNew = (peer == nullptr);

  if (isNew) {
    // Add new peer
    if (_peers.size() >= MESH_MAX_PEERS) {
      // Remove oldest offline peer
      auto it = std::find_if(_peers.begin(), _peers.end(),
                             [](const MeshPeerInfo& p) { return !p.isOnline; });
      if (it != _peers.end()) {
        _peers.erase(it);
      } else {
        // All online, remove last
        _peers.pop_back();
      }
    }

    MeshPeerInfo newPeer;
    memset(&newPeer, 0, sizeof(newPeer));
    memcpy(newPeer.mac, mac, 6);
    _peers.push_back(newPeer);
    peer = &_peers.back();
  }

  // Update peer info
  if (rssi != -127) {
    peer->rssi = rssi;
  }
  peer->lastSeen = millis();
  peer->hopDistance = hopDistance;
  peer->isOnline = true;

  if (unitName && strlen(unitName) > 0) {
    strncpy(peer->unitName, unitName, sizeof(peer->unitName) - 1);
    peer->unitName[sizeof(peer->unitName) - 1] = '\0';
  }

  // Add to ESP-NOW peer list for direct communication
  addEspNowPeer(mac);

  // Notify callback
  if (_peerCallback) {
    _peerCallback(peer, isNew);
  }
}

void MeshManager::pruneOfflinePeers() {
  uint32_t now = millis();

  for (auto& peer : _peers) {
    if (peer.isOnline && (now - peer.lastSeen > MESH_PEER_TIMEOUT_MS)) {
      peer.isOnline = false;

      if (_peerCallback) {
        _peerCallback(&peer, false);
      }
    }
  }
}

MeshPeerInfo* MeshManager::findPeer(const uint8_t* mac) {
  for (auto& peer : _peers) {
    if (memcmp(peer.mac, mac, 6) == 0) {
      return &peer;
    }
  }
  return nullptr;
}

void MeshManager::processStoreQueue() {
  uint32_t now = millis();

  for (int i = 0; i < MESH_STORE_QUEUE_SIZE; i++) {
    if (!_storeQueue[i].active) continue;

    // Check if message expired
    if (now - _storeQueue[i].storedTime > MESH_MSG_EXPIRE_MS) {
      _storeQueue[i].active = false;
      Serial.printf("MeshManager: Stored message %08X expired\n",
                    _storeQueue[i].packet.header.messageId);
      continue;
    }

    // Check if target is now online
    MeshPeerInfo* peer = findPeer(_storeQueue[i].targetMac);
    if (peer && peer->isOnline) {
      // Attempt delivery
      deliverStoredMessages(_storeQueue[i].targetMac);
    }
  }
}

void MeshManager::deliverStoredMessages(const uint8_t* peerMac) {
  for (int i = 0; i < MESH_STORE_QUEUE_SIZE; i++) {
    if (!_storeQueue[i].active) continue;

    if (memcmp(_storeQueue[i].targetMac, peerMac, 6) == 0) {
      // Attempt delivery
      MeshPacket* packet = &_storeQueue[i].packet;

      // Update timestamp and resign
      packet->header.timestamp = millis();
      memcpy(packet->header.lastHopMac, _myMac, 6);
      signPacket(packet);

      esp_err_t result = esp_now_send(BROADCAST_MAC, (uint8_t*)packet,
                                       sizeof(MeshHeader) + packet->header.payloadLen + HMAC_SIZE);

      if (result == ESP_OK) {
        Serial.printf("MeshManager: Delivered stored message %08X\n", packet->header.messageId);
        _msgsSent++;
        // Keep active until ACK received or expired
        _storeQueue[i].retryCount++;
        if (_storeQueue[i].retryCount >= 3) {
          _storeQueue[i].active = false;
        }
      }
    }
  }
}

void MeshManager::sendHeartbeat() {
  // Build heartbeat with unit name
  size_t nameLen = strlen(_unitName);
  broadcast((uint8_t*)_unitName, nameLen, MESH_MSG_HEARTBEAT, 1);  // TTL=1, direct neighbors only
}

bool MeshManager::signPacket(MeshPacket* packet) {
  if (packet == nullptr) return false;

  // Build message to sign: header + payload
  size_t signLen = sizeof(MeshHeader) + packet->header.payloadLen;

  // Generate HMAC
  uint8_t hmac[32];
  char passkeyStr[16];
  snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", (unsigned long)_passkey);

  if (!MessageAuth::generateHMAC((const char*)packet, passkeyStr, hmac, 32)) {
    return false;
  }

  // Append truncated HMAC to payload
  memcpy(&packet->payload[packet->header.payloadLen], hmac, HMAC_SIZE);

  return true;
}

bool MeshManager::verifyPacket(const MeshPacket* packet) {
  if (packet == nullptr) return false;

  // Extract received HMAC
  uint8_t receivedHmac[HMAC_SIZE];
  memcpy(receivedHmac, &packet->payload[packet->header.payloadLen], HMAC_SIZE);

  // Create copy without HMAC for verification
  MeshPacket verify = *packet;

  // Generate expected HMAC
  uint8_t expectedHmac[32];
  char passkeyStr[16];
  snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", (unsigned long)_passkey);

  if (!MessageAuth::generateHMAC((const char*)&verify, passkeyStr, expectedHmac, 32)) {
    return false;
  }

  // Compare truncated HMAC
  return memcmp(receivedHmac, expectedHmac, HMAC_SIZE) == 0;
}

uint32_t MeshManager::generateMessageId() {
  return ++_messageIdCounter;
}

bool MeshManager::addEspNowPeer(const uint8_t* mac) {
  if (mac == nullptr) return false;

  // Check if already added
  if (esp_now_is_peer_exist(mac)) {
    return true;
  }

  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = MESH_CHANNEL;
  peerInfo.encrypt = false;  // Using application-level HMAC instead

  return esp_now_add_peer(&peerInfo) == ESP_OK;
}

int MeshManager::getStoredMessageCount() const {
  int count = 0;
  for (int i = 0; i < MESH_STORE_QUEUE_SIZE; i++) {
    if (_storeQueue[i].active) count++;
  }
  return count;
}

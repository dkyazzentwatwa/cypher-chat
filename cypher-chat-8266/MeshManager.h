#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <vector>
#include <map>
#include "MeshCrypto.h"

extern "C" {
  #include <espnow.h>
  #include <user_interface.h>
}

/**
 * MeshManager - ESP-NOW mesh networking for ESP8266
 *
 * Uses the ESP8266 C-style ESP-NOW API instead of ESP32's class-based API.
 * Wire-compatible with ESP32 cypher-chat mesh nodes.
 *
 * Features:
 * - Multi-hop message relay with TTL
 * - Store-and-forward for offline nodes
 * - Message deduplication (seen cache)
 * - Automatic peer discovery via heartbeat
 * - Cross-compatible with ESP32 mesh nodes
 *
 * Range: ~250m line-of-sight
 * Latency: ~1ms per hop
 */

// Mesh protocol constants - reduced for ESP8266 RAM
#ifndef MESH_MAX_PEERS
#define MESH_MAX_PEERS        10
#endif
#ifndef MESH_MAX_TTL
#define MESH_MAX_TTL          5
#endif
#ifndef MESH_DEFAULT_TTL
#define MESH_DEFAULT_TTL      3
#endif
#ifndef MESH_MSG_CACHE_SIZE
#define MESH_MSG_CACHE_SIZE   32
#endif
#ifndef MESH_STORE_QUEUE_SIZE
#define MESH_STORE_QUEUE_SIZE 8
#endif
#ifndef MESH_MSG_EXPIRE_MS
#define MESH_MSG_EXPIRE_MS    300000
#endif
#ifndef MESH_PEER_TIMEOUT_MS
#define MESH_PEER_TIMEOUT_MS  60000
#endif
#ifndef MESH_HEARTBEAT_MS
#define MESH_HEARTBEAT_MS     15000
#endif
#ifndef MESH_CHANNEL
#define MESH_CHANNEL          1
#endif
#define MESH_EMERGENCY_COOLDOWN_MS 10000
#define MESH_MAX_PLAINTEXT    (MESH_MAX_PAYLOAD - MESH_CRYPTO_OVERHEAD)

// Message types - identical to ESP32 version
enum MeshMessageType : uint8_t {
  MESH_MSG_DATA      = 0x01,
  MESH_MSG_EMERGENCY = 0x02,
  MESH_MSG_ACK       = 0x03,
  MESH_MSG_HEARTBEAT = 0x04,
  MESH_MSG_FORWARD   = 0x05,
  MESH_MSG_GPS       = 0x06
};

// Mesh message header - identical to ESP32 version (32 bytes)
struct __attribute__((packed)) MeshHeader {
  uint8_t  version;
  uint8_t  type;
  uint8_t  ttl;
  uint8_t  hopCount;
  uint32_t messageId;
  uint8_t  originMac[6];
  uint8_t  destMac[6];
  uint8_t  lastHopMac[6];
  uint32_t timestamp;
  uint8_t  payloadLen;
  uint8_t  flags;
};

// Full mesh packet - identical to ESP32 version
#define MESH_MAX_PAYLOAD 200
struct __attribute__((packed)) MeshPacket {
  MeshHeader header;
  uint8_t payload[MESH_MAX_PAYLOAD];
};

// Peer info
struct MeshPeerInfo {
  uint8_t mac[6];
  int8_t rssi;
  uint32_t lastSeen;
  uint8_t hopDistance;
  bool isOnline;
  char unitName[17];
};

// Store-and-forward entry
struct StoreForwardEntry {
  MeshPacket packet;
  uint8_t targetMac[6];
  uint32_t storedTime;
  uint8_t retryCount;
  bool active;
};

// GPS coordinates
struct GPSCoordinates {
  float latitude;
  float longitude;
  float altitude;
  bool valid;
};

// Callback types
typedef void (*MeshMessageCallback)(const MeshPacket* packet, const uint8_t* senderMac, int8_t rssi);
typedef void (*MeshPeerCallback)(const MeshPeerInfo* peer, bool isNew);

class MeshManager {
public:
  MeshManager();

  bool begin(const char* unitName, const char* passphrase);
  void end();
  void update();

  uint32_t broadcast(const uint8_t* data, size_t len, MeshMessageType type = MESH_MSG_DATA, uint8_t ttl = MESH_DEFAULT_TTL);
  uint32_t sendTo(const uint8_t* destMac, const uint8_t* data, size_t len, MeshMessageType type = MESH_MSG_DATA);
  uint32_t sendEmergency(const char* message, const GPSCoordinates* gps = nullptr);
  bool storeForDelivery(const uint8_t* destMac, const uint8_t* data, size_t len);

  void onMessage(MeshMessageCallback callback);
  void onPeerUpdate(MeshPeerCallback callback);

  std::vector<MeshPeerInfo> getPeers() const;
  int getOnlinePeerCount() const;
  void getMyMac(uint8_t* mac) const;
  bool isRunning() const { return _running; }

  uint32_t getMessagesSent() const { return _msgsSent; }
  uint32_t getMessagesReceived() const { return _msgsReceived; }
  uint32_t getMessagesRelayed() const { return _msgsRelayed; }
  uint32_t getMessagesDropped() const { return _msgsDropped; }

  void setGPS(const GPSCoordinates& gps);
  GPSCoordinates getGPS() const { return _myGPS; }
  int getStoredMessageCount() const;

  // Called from static ESP-NOW callbacks
  void handleReceive(uint8_t* senderMac, uint8_t* data, uint8_t len);
  void handleSendResult(uint8_t* mac, uint8_t status);

private:
  void processPacket(const MeshPacket* packet, const uint8_t* senderMac, int8_t rssi);
  void handleDataMessage(const MeshPacket* packet, const uint8_t* senderMac, int8_t rssi);
  void handleEmergency(const MeshPacket* packet, const uint8_t* senderMac);
  void handleAck(const MeshPacket* packet);
  void handleHeartbeat(const MeshPacket* packet, const uint8_t* senderMac, int8_t rssi);

  bool shouldRelay(const MeshPacket* packet);
  void relayPacket(MeshPacket* packet);
  bool isMessageSeen(uint32_t messageId);
  void markMessageSeen(uint32_t messageId);

  void updatePeer(const uint8_t* mac, int8_t rssi, const char* unitName, uint8_t hopDistance);
  void pruneOfflinePeers();
  MeshPeerInfo* findPeer(const uint8_t* mac);

  void processStoreQueue();
  void deliverStoredMessages(const uint8_t* peerMac);

  void sendHeartbeat();
  uint32_t generateMessageId();
  bool addEspNowPeer(const uint8_t* mac);

  // State
  bool _running;
  char _unitName[17];
  uint8_t _myMac[6];
  GPSCoordinates _myGPS;

  // Callbacks
  MeshMessageCallback _messageCallback;
  MeshPeerCallback _peerCallback;

  // Peer tracking
  std::map<uint64_t, MeshPeerInfo> _peerInfo;

  // Message deduplication
  uint32_t _seenMessages[MESH_MSG_CACHE_SIZE];
  int _seenIndex;

  // Store-and-forward queue
  StoreForwardEntry _storeQueue[MESH_STORE_QUEUE_SIZE];

  // Timing
  uint32_t _lastHeartbeat;
  uint32_t _lastPeerPrune;

  // Statistics
  uint32_t _msgsSent;
  uint32_t _msgsReceived;
  uint32_t _msgsRelayed;
  uint32_t _msgsDropped;
  uint32_t _messageIdCounter;

  // Emergency rate limiting
  std::map<uint64_t, uint32_t> _emergencyRateLimit;
};

// Global instance
extern MeshManager meshMgr;

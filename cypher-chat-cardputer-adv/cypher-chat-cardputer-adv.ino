#include "Config_Cardputer.h"
#include <CypherPuterReturn.h>
#include "MessageAuth.h"
#include "OutputManager.h"
#if BLE_UART_ENABLED
#include "BLEUARTManager.h"
#endif
#include "TerminalManager.h"
#include "MeshManager.h"
#include "StateManager.h"
#include "CardputerUI.h"
#include "DeviceSettings.h"
#include "MessageStore.h"
#include "PeerDirectory.h"

bool isServer = false;
String unitName = DEFAULT_UNIT_NAME;
uint32_t currentPasskey = DEFAULT_PASSKEY;
String messageHistory[10];
int historyCount = 0;

const char* BUTTON_LABELS[] = { "ACK", "ENROUTE", "NEED HELP", "ALL GOOD" };

StateManager connectionState;

bool emergencyActive = false;
unsigned long emergencyStartTime = 0;
const unsigned long EMERGENCY_DURATION = 30000;

static char composeBuffer[CARDPUTER_INPUT_MAX] = "";
static bool emergencyConfirm = false;
static bool passkeyLoadedFromFlash = false;

enum FieldMode : uint8_t {
  FIELD_MODE_NORMAL,
  FIELD_MODE_QUIET,
  FIELD_MODE_MONITOR,
  FIELD_MODE_BASE_RELAY
};

uint8_t fieldBrightness = 180;
bool speakerMuted = false;
uint8_t savedDefaultScreen = CARD_SCREEN_STATUS;
uint8_t savedMeshChannel = MESH_CHANNEL;
FieldMode fieldMode = FIELD_MODE_NORMAL;
MessageFilter messageFilter = MSG_FILTER_ALL;

enum CardputerMenuPage {
  MENU_PAGE_HOME,
  MENU_PAGE_STATUS,
  MENU_PAGE_MESSAGES,
  MENU_PAGE_MESH,
  MENU_PAGE_PEERS,
  MENU_PAGE_SETTINGS,
  MENU_PAGE_SYSTEM,
  MENU_PAGE_MODES,
  MENU_PAGE_DIAGNOSTICS,
  MENU_PAGE_EMERGENCY,
  MENU_PAGE_EDIT_NAME,
  MENU_PAGE_EDIT_PASSKEY
};

static CardputerMenuPage menuPage = MENU_PAGE_HOME;
static int menuSelected = 0;
static int menuScroll = 0;
static char editBuffer[MAX_MESSAGE_SIZE] = "";

void sendTypedMessage(const char* text);
void handleCardputerKeyboard();
void configurePasskey();
void updateComposeUI();
void setEmergencyConfirm(bool active);
void renderMenu();
void resetMenu(CardputerMenuPage page);
void menuMove(int delta);
void menuEnter();
void menuBack();
void handleMenuKey(char c, bool fnHeld);
bool isCardputerArrowUp();
bool isCardputerArrowDown();
bool saveEditedName();
bool saveEditedPasskey();
const char* terminalModeLabel();
const char* fieldModeLabel();
void applyFieldMode(bool save);
void applyBrightness(uint8_t brightness, bool save);
void setSpeakerMuted(bool muted, bool save);
void renderBootCheck(const char* step, const char* detail);
void fieldConsoleDump();
void fieldConsoleLogs();
void fieldConsoleResetSettings();
void fieldConsoleSetBrightness(const char* args);
void fieldConsoleSetSpeaker(const char* args);
void fieldConsoleSetRelayMode(const char* args);
void fieldConsoleSetPeerName(const char* args);
void fieldConsoleSetPeerTrust(const char* args);

#if MESH_ENABLED
void onMeshMessage(const MeshPacket* packet, const uint8_t* senderMac, int8_t rssi) {
  (void)senderMac;

  char msg[MESH_MAX_PAYLOAD + 1];
  size_t len = packet->header.payloadLen;
  if (len > MESH_MAX_PAYLOAD) len = MESH_MAX_PAYLOAD;

  if (packet->header.type == MESH_MSG_EMERGENCY) {
    if (len == 0) {
      return;
    }

    size_t offset = 1;
    if ((packet->payload[0] & 0x01) && len >= 13) {
      offset = 13;
    }
    if (offset > len) offset = len;

    size_t msgLen = len - offset;
    if (msgLen > sizeof(msg) - 1) msgLen = sizeof(msg) - 1;
    memcpy(msg, &packet->payload[offset], msgLen);
    msg[msgLen] = '\0';

    output.println();
    output.printf("[EMERGENCY] Mesh RX: %s | RSSI %d | hops %d\n", msg, rssi, packet->header.hopCount);
    cardputerUI.updateStatus("MESH EMERGENCY", msg);
    addToHistory(msg);
    beep();
    return;
  }

  if (len > sizeof(msg) - 1) len = sizeof(msg) - 1;
  memcpy(msg, packet->payload, len);
  msg[len] = '\0';

  output.printf("Mesh RX: %s\n", msg);
  cardputerUI.updateStatus("Mesh message", msg);
  addToHistory(msg);
  beep();
}

void onMeshPeerUpdate(const MeshPeerInfo* peer, bool isNew) {
  if (peer) {
    peerDirectory.observe(*peer);
  }
  if (isNew) {
    if (strlen(peer->unitName) > 0) {
      output.printf("+ Mesh peer: %s (RSSI: %d dBm)\n", peer->unitName, peer->rssi);
    } else {
      output.printf("+ Mesh peer: %02X:%02X (RSSI: %d dBm)\n", peer->mac[4], peer->mac[5], peer->rssi);
    }
    cardputerUI.updateStatus("Peer joined", strlen(peer->unitName) > 0 ? peer->unitName : "mesh node");
  } else if (!peer->isOnline && strlen(peer->unitName) > 0) {
    output.printf("- Mesh peer offline: %s\n", peer->unitName);
  }
}
#endif

void beep() {
#if CARDPUTER_SPEAKER_ENABLED
  if (speakerMuted) {
    return;
  }
  M5Cardputer.Speaker.tone(4000, 35);
#endif
}

void oledPrint(const char* line1, const char* line2) {
  cardputerUI.updateStatus(line1, line2);
}

void addToHistory(const char* msg) {
  if (!msg) return;

  messageHistory[historyCount % 10] = String(msg);
  historyCount++;

  bool outgoing = strstr(msg, unitName.c_str()) == msg;
  bool emergency = strstr(msg, "EMERGENCY") != nullptr;
  messageStore.add(msg, outgoing, emergency);
  cardputerUI.addMessage(msg, outgoing, emergency);
  terminalMgr.logMessage(msg, outgoing, false);
}

void sendTypedMessage(const char* text) {
  if (!text || strlen(text) == 0) {
    return;
  }

  char baseMsg[MAX_MESSAGE_SIZE];
  snprintf(baseMsg, sizeof(baseMsg), "%s:0:%s", unitName.c_str(), text);

#if MESH_ENABLED
  uint32_t msgId = meshMgr.broadcast((const uint8_t*)baseMsg, strlen(baseMsg), MESH_MSG_DATA, MESH_DEFAULT_TTL);
  if (msgId > 0) {
    output.printf("Sent chat: %s (mesh id: %08X)\n", baseMsg, msgId);
  } else {
    output.println("Failed to send chat message");
  }
#else
  output.printf("Sent locally: %s\n", baseMsg);
#endif

  addToHistory(baseMsg);
  cardputerUI.updateStatus("Sent chat", text);
  beep();
}

void simulateButtonPress(int buttonIndex) {
  if (buttonIndex < 0 || buttonIndex >= NUM_BUTTONS) {
    return;
  }

  char baseMsg[MAX_MESSAGE_SIZE];
  snprintf(baseMsg, sizeof(baseMsg), "%s:%d:%s", unitName.c_str(), buttonIndex + 1, BUTTON_LABELS[buttonIndex]);

#if MESH_ENABLED
  uint32_t msgId = meshMgr.broadcast((const uint8_t*)baseMsg, strlen(baseMsg), MESH_MSG_DATA, MESH_DEFAULT_TTL);
  if (msgId > 0) {
    output.printf("Sent preset: %s (mesh id: %08X)\n", baseMsg, msgId);
  } else {
    output.println("Failed to send preset");
  }
#else
  output.printf("Sent locally: %s\n", baseMsg);
#endif

  addToHistory(baseMsg);
  cardputerUI.updateStatus("Preset sent", BUTTON_LABELS[buttonIndex]);
  beep();
}

void broadcastEmergency() {
  emergencyActive = true;
  emergencyStartTime = millis();

  char baseMsg[MAX_MESSAGE_SIZE];
  snprintf(baseMsg, sizeof(baseMsg), "%s:0:EMERGENCY", unitName.c_str());

#if MESH_ENABLED
  uint32_t meshMsgId = meshMgr.sendEmergency(baseMsg, nullptr);
  if (meshMsgId > 0) {
    output.printf("[EMERGENCY] Broadcast sent (TTL: %d hops)\n", MESH_MAX_TTL);
  } else {
    output.println("[EMERGENCY] Broadcast failed");
  }
#endif

  addToHistory(baseMsg);
  cardputerUI.updateStatus("EMERGENCY ACTIVE", "Broadcasting for 30 sec");
  setEmergencyConfirm(false);

#if CARDPUTER_SPEAKER_ENABLED
  if (!speakerMuted) {
    M5Cardputer.Speaker.tone(1200, 90);
    delay(25);
    M5Cardputer.Speaker.tone(1800, 90);
  }
#endif
}

void cancelEmergency() {
  emergencyActive = false;
  output.println("[EMERGENCY] Emergency canceled");
  cardputerUI.updateStatus("Emergency canceled", "");
  setEmergencyConfirm(false);
}

void handleEmergency() {
  if (!emergencyActive) {
    return;
  }

  if (millis() - emergencyStartTime >= EMERGENCY_DURATION) {
    output.println("[EMERGENCY] Auto-canceled after 30 seconds");
    cancelEmergency();
    return;
  }

  unsigned long remaining = (EMERGENCY_DURATION - (millis() - emergencyStartTime)) / 1000;
  char line[40];
  snprintf(line, sizeof(line), "Auto-cancel in %lus", remaining);
  cardputerUI.updateStatus("EMERGENCY ACTIVE", line);
}

void setEmergencyConfirm(bool active) {
  emergencyConfirm = active;
  cardputerUI.setEmergencyConfirm(active);
  if (active) {
    cardputerUI.setScreen(CARD_SCREEN_MENU);
    cardputerUI.updateStatus("Confirm SOS", "Ent sends, Del cancels");
  }
  cardputerUI.refresh(true);
}

void updateComposeUI() {
  cardputerUI.setInputBuffer(composeBuffer);
}

const char* terminalModeLabel() {
  switch (terminalMgr.getMode()) {
    case TERM_QUIET: return "quiet";
    case TERM_NORMAL: return "normal";
    case TERM_VERBOSE: return "verbose";
    case TERM_MONITOR: return "monitor";
    default: return "unknown";
  }
}

const char* fieldModeLabel() {
  switch (fieldMode) {
    case FIELD_MODE_QUIET: return "quiet";
    case FIELD_MODE_MONITOR: return "monitor";
    case FIELD_MODE_BASE_RELAY: return "base relay";
    case FIELD_MODE_NORMAL:
    default: return "normal";
  }
}

void applyBrightness(uint8_t brightness, bool save) {
  if (brightness < 40) brightness = 40;
  fieldBrightness = brightness;
  M5Cardputer.Display.setBrightness(fieldBrightness);
  if (save) {
    DeviceSettings::saveBrightness(fieldBrightness);
  }
}

void setSpeakerMuted(bool muted, bool save) {
  speakerMuted = muted;
  if (save) {
    DeviceSettings::saveSpeakerMuted(speakerMuted);
  }
}

void applyFieldMode(bool save) {
  switch (fieldMode) {
    case FIELD_MODE_QUIET:
      setSpeakerMuted(true, save);
      applyBrightness(70, save);
      terminalMgr.setMode(TERM_QUIET);
      break;
    case FIELD_MODE_MONITOR:
      terminalMgr.setMode(TERM_MONITOR);
      break;
    case FIELD_MODE_BASE_RELAY:
      setSpeakerMuted(false, save);
      applyBrightness(220, save);
      terminalMgr.setMode(TERM_VERBOSE);
      break;
    case FIELD_MODE_NORMAL:
    default:
      terminalMgr.setMode(TERM_NORMAL);
      break;
  }
  if (save) {
    DeviceSettings::saveFieldMode((uint8_t)fieldMode);
  }
  char line[40];
  snprintf(line, sizeof(line), "Mode: %s", fieldModeLabel());
  cardputerUI.updateStatus("Field mode set", line);
}

void renderBootCheck(const char* step, const char* detail) {
  cardputerUI.updateStatus(step, detail);
  cardputerUI.refresh(true);
  output.print("[BOOT] ");
  output.print(step);
  output.print(": ");
  output.println(detail);
  delay(180);
}

int menuItemCount(CardputerMenuPage page) {
  switch (page) {
    case MENU_PAGE_HOME: return 9;
    case MENU_PAGE_STATUS: return 8;
    case MENU_PAGE_MESSAGES: return 9;
    case MENU_PAGE_MESH: return 4;
    case MENU_PAGE_PEERS: {
      int count = meshMgr.getPeers().size();
      return count > 0 ? count : 2;
    }
    case MENU_PAGE_SETTINGS: return 9;
    case MENU_PAGE_SYSTEM: return 8;
    case MENU_PAGE_MODES: return 5;
    case MENU_PAGE_DIAGNOSTICS: return 5;
    case MENU_PAGE_EMERGENCY: return 3;
    default: return 0;
  }
}

void resetMenu(CardputerMenuPage page) {
  menuPage = page;
  menuSelected = 0;
  menuScroll = 0;
  cardputerUI.setScreen(CARD_SCREEN_MENU);
  renderMenu();
}

void ensureMenuScroll() {
  const int visibleRows = 6;
  if (menuSelected < menuScroll) {
    menuScroll = menuSelected;
  } else if (menuSelected >= menuScroll + visibleRows) {
    menuScroll = menuSelected - visibleRows + 1;
  }
  if (menuScroll < 0) menuScroll = 0;
}

void menuMove(int delta) {
  if (menuPage == MENU_PAGE_STATUS) {
    int count = menuItemCount(menuPage);
    const int visibleRows = 6;
    int maxScroll = max(0, count - visibleRows);
    menuScroll += delta;
    if (menuScroll < 0) menuScroll = maxScroll;
    if (menuScroll > maxScroll) menuScroll = 0;
    renderMenu();
    return;
  }

  int count = menuItemCount(menuPage);
  if (count <= 0) {
    return;
  }
  menuSelected += delta;
  if (menuSelected < 0) menuSelected = count - 1;
  if (menuSelected >= count) menuSelected = 0;
  ensureMenuScroll();
  renderMenu();
}

bool saveEditedName() {
  if (strlen(editBuffer) == 0 || strlen(editBuffer) > MAX_UNIT_NAME_LEN) {
    cardputerUI.updateStatus("Name not saved", "1-16 chars required");
    return false;
  }
  unitName = String(editBuffer);
  DeviceSettings::saveName(unitName.c_str());
  cardputerUI.updateStatus("Name saved", "Restart refreshes mesh/BLE");
  return true;
}

bool saveEditedPasskey() {
  if (strlen(editBuffer) != PASSKEY_DIGITS) {
    cardputerUI.updateStatus("PIN not saved", "Use exactly 6 digits");
    return false;
  }
  uint32_t passkey = atoi(editBuffer);
  if (passkey < MIN_PASSKEY || passkey > MAX_PASSKEY) {
    cardputerUI.updateStatus("PIN not saved", "Range 100000-999999");
    return false;
  }
  currentPasskey = passkey;
  DeviceSettings::savePasskey(currentPasskey);
  cardputerUI.updateStatus("PIN saved", "Restart mesh to apply");
  return true;
}

void clearLocalHistory() {
  for (int i = 0; i < 10; i++) {
    messageHistory[i] = "";
  }
  historyCount = 0;
  composeBuffer[0] = '\0';
  updateComposeUI();
  cardputerUI.clearMessages();
  messageStore.clear();
  cardputerUI.updateStatus("Local history cleared", "");
}

void renderMenu() {
  static const char* const homeItems[] = {
    "Status", "Messages", "Mesh", "Peers", "Settings", "Modes", "System", "Diagnostics", "Emergency"
  };
  static const char* const messageItems[] = {
    "Recent history", "Filter messages", "Resend last", "Send ACK", "Send ENROUTE", "Send NEED HELP", "Send ALL GOOD", "Open chat", "Back"
  };
  static const char* const meshItems[] = {
    "Mesh stats", "Peers", "Broadcast typed msg", "Back"
  };
  static const char* const modeItems[] = {
    "Normal", "Quiet", "Monitor", "Base Relay", "Back"
  };
  static const char* const diagnosticsItems[] = {
    "Dump status", "Show logs", "Factory reset settings", "Restart", "Back"
  };
  static const char* const emergencyItems[] = {
    "Confirm SOS", "Cancel emergency", "Back"
  };

  if (menuPage == MENU_PAGE_HOME) {
    cardputerUI.drawMenuList("Menu", homeItems, 9, menuSelected, menuScroll, "Up/Dn W/S | Ent");
    return;
  }

  if (menuPage == MENU_PAGE_MESSAGES) {
    cardputerUI.drawMenuList("Messages", messageItems, 9, menuSelected, menuScroll, "3-6 presets | Ent");
    return;
  }

  if (menuPage == MENU_PAGE_MESH) {
    cardputerUI.drawMenuList("Mesh", meshItems, 4, menuSelected, menuScroll, "Ent run | Del back");
    return;
  }

  if (menuPage == MENU_PAGE_PEERS) {
    auto peers = meshMgr.getPeers();
    if (peers.empty() || menuSelected < 0 || menuSelected >= (int)peers.size()) {
      cardputerUI.updateStatus("No peer detail", "Wait for heartbeat");
      renderMenu();
      return;
    }

    const MeshPeerInfo& peer = peers[menuSelected];
    static char line0[40], line1[40], line2[40], line3[40], line4[40], line5[40];
    char name[MAX_UNIT_NAME_LEN + 1];
    peerDirectory.displayName(peer, name, sizeof(name));
    unsigned long age = (millis() - peer.lastSeen) / 1000;
    snprintf(line0, sizeof(line0), "Name: %s", name);
    snprintf(line1, sizeof(line1), "MAC: %02X:%02X:%02X", peer.mac[3], peer.mac[4], peer.mac[5]);
    snprintf(line2, sizeof(line2), "Signal: %ddBm h%d", peer.rssi, peer.hopDistance);
    snprintf(line3, sizeof(line3), "Last seen: %lus", age);
    snprintf(line4, sizeof(line4), "State: %s", peer.isOnline ? "online" : "offline");
    snprintf(line5, sizeof(line5), "Trust: %s", peerDirectory.trustLabel(peer.mac));
    const char* lines[] = { line0, line1, line2, line3, line4, line5 };
    cardputerUI.drawInfoList("Peer Detail", lines, 6, 0, "Del back");
    return;
  }

  if (menuPage == MENU_PAGE_SETTINGS) {
    char modeLine[32];
    char ansiLine[32];
    char brightLine[32];
    char speakerLine[32];
    char screenLine[32];
    char channelLine[32];
    snprintf(modeLine, sizeof(modeLine), "Terminal mode: %s", terminalModeLabel());
    snprintf(ansiLine, sizeof(ansiLine), "ANSI: %s", terminalMgr.isAnsiEnabled() ? "on" : "off");
    snprintf(brightLine, sizeof(brightLine), "Brightness: %u", fieldBrightness);
    snprintf(speakerLine, sizeof(speakerLine), "Speaker: %s", speakerMuted ? "mute" : "on");
    snprintf(screenLine, sizeof(screenLine), "Default screen: %u", savedDefaultScreen);
    snprintf(channelLine, sizeof(channelLine), "Mesh channel: %u", savedMeshChannel);
    const char* items[] = { "Edit device name", "Edit passkey", brightLine, speakerLine, modeLine, ansiLine, screenLine, channelLine, "Back" };
    cardputerUI.drawMenuList("Settings", items, 9, menuSelected, menuScroll, "Ent edit | Del back");
    return;
  }

  if (menuPage == MENU_PAGE_SYSTEM) {
    static const char* const systemItems[] = {
      "Version", "Memory + uptime", "Clear local history", "Dump status", "Factory reset settings", "Restart", "Return to Launcher", "Back"
    };
    cardputerUI.drawMenuList("System", systemItems, 8, menuSelected, menuScroll, "Ent run | Del back");
    return;
  }

  if (menuPage == MENU_PAGE_MODES) {
    cardputerUI.drawMenuList("Modes", modeItems, 5, menuSelected, menuScroll, "Ent set | Del back");
    return;
  }

  if (menuPage == MENU_PAGE_DIAGNOSTICS) {
    cardputerUI.drawMenuList("Diagnostics", diagnosticsItems, 5, menuSelected, menuScroll, "Ent run | Del back");
    return;
  }

  if (menuPage == MENU_PAGE_EMERGENCY) {
    cardputerUI.drawMenuList("Emergency", emergencyItems, 3, menuSelected, menuScroll, "Ent run | Del back");
    return;
  }

  if (menuPage == MENU_PAGE_STATUS) {
    char line0[40], line1[40], line2[40], line3[40], line4[40], line5[40], line6[40], line7[40];
    uint8_t mac[6];
    meshMgr.getMyMac(mac);
    char uptime[18];
    unsigned long total = millis() / 1000;
    snprintf(uptime, sizeof(uptime), "%luh%02lum", total / 3600, (total / 60) % 60);
    snprintf(line0, sizeof(line0), "Name: %s", unitName.c_str());
    snprintf(line1, sizeof(line1), "Mesh: %s peers:%d", meshMgr.isRunning() ? "on" : "off", meshMgr.getOnlinePeerCount());
    snprintf(line2, sizeof(line2), "Mode: %s BLE:%s", fieldModeLabel(), bleUARTMgr.isConnected() ? "on" : "wait");
    snprintf(line3, sizeof(line3), "Battery: %d%% bright:%u", M5Cardputer.Power.getBatteryLevel(), fieldBrightness);
    snprintf(line4, sizeof(line4), "Uptime: %s", uptime);
    snprintf(line5, sizeof(line5), "Heap: %luKB", ESP.getFreeHeap() / 1024);
    snprintf(line6, sizeof(line6), "MAC: %02X:%02X:%02X", mac[3], mac[4], mac[5]);
    snprintf(line7, sizeof(line7), "PIN: %06lu", (unsigned long)currentPasskey);
    const char* lines[] = { line0, line1, line2, line3, line4, line5, line6, line7 };
    cardputerUI.drawInfoList("Status", lines, 8, menuScroll, "Up/Dn scroll | Del");
    return;
  }

  if (menuPage == MENU_PAGE_PEERS) {
    static char peerLines[8][56];
    const char* lines[8];
    int count = 0;
    auto peers = meshMgr.getPeers();
    if (peers.empty()) {
      lines[count++] = "No peers discovered";
      lines[count++] = "Wait for heartbeat";
      cardputerUI.drawInfoList("Peers", lines, count, menuScroll, "Up/Dn scroll | Del");
      return;
    } else {
      for (const auto& peer : peers) {
        if (count >= 8) break;
        peerDirectory.formatPeerLine(peer, peerLines[count], sizeof(peerLines[count]));
        lines[count] = peerLines[count];
        count++;
      }
    }
    cardputerUI.drawMenuList("Peers", lines, count, menuSelected, menuScroll, "Ent detail | Del");
    return;
  }

  if (menuPage == MENU_PAGE_EDIT_NAME) {
    cardputerUI.drawEditView("Device Name", editBuffer, false, "Ent save | Del");
    return;
  }

  if (menuPage == MENU_PAGE_EDIT_PASSKEY) {
    cardputerUI.drawEditView("Passkey", editBuffer, true, "6 digits | Ent save");
  }
}

void menuEnter() {
  if (menuPage == MENU_PAGE_HOME) {
    switch (menuSelected) {
      case 0: resetMenu(MENU_PAGE_STATUS); break;
      case 1: resetMenu(MENU_PAGE_MESSAGES); break;
      case 2: resetMenu(MENU_PAGE_MESH); break;
      case 3: resetMenu(MENU_PAGE_PEERS); break;
      case 4: resetMenu(MENU_PAGE_SETTINGS); break;
      case 5: resetMenu(MENU_PAGE_MODES); break;
      case 6: resetMenu(MENU_PAGE_SYSTEM); break;
      case 7: resetMenu(MENU_PAGE_DIAGNOSTICS); break;
      case 8: resetMenu(MENU_PAGE_EMERGENCY); break;
    }
    return;
  }

  if (menuPage == MENU_PAGE_MESSAGES) {
    if (menuSelected == 0) {
      static char linesBuf[10][64];
      const char* lines[10];
      int count = min(messageStore.count(messageFilter), 10);
      for (int i = 0; i < count; i++) {
        messageStore.formatNewest(i, messageFilter, linesBuf[i], sizeof(linesBuf[i]));
        lines[i] = linesBuf[i];
      }
      if (count == 0) {
        lines[0] = "No messages yet";
        count = 1;
      }
      cardputerUI.drawInfoList("History", lines, count, 0, "Del back");
      return;
    } else if (menuSelected == 1) {
      messageFilter = (MessageFilter)(((uint8_t)messageFilter + 1) % 4);
      cardputerUI.updateStatus("Filter changed", messageStore.filterName(messageFilter));
      renderMenu();
      return;
    } else if (menuSelected == 2) {
      char last[MAX_MESSAGE_SIZE];
      if (messageStore.lastOutgoing(last, sizeof(last))) {
        const char* payload = strrchr(last, ':');
        sendTypedMessage(payload && *(payload + 1) ? payload + 1 : last);
      } else {
        cardputerUI.updateStatus("No sent message", "Nothing to resend");
      }
      renderMenu();
      return;
    } else if (menuSelected >= 3 && menuSelected <= 6) {
      simulateButtonPress(menuSelected - 3);
    } else if (menuSelected == 7) {
      cardputerUI.setScreen(CARD_SCREEN_CHAT);
      cardputerUI.refresh(true);
      return;
    } else {
      resetMenu(MENU_PAGE_HOME);
      return;
    }
    renderMenu();
    return;
  }

  if (menuPage == MENU_PAGE_MESH) {
    if (menuSelected == 0) {
      resetMenu(MENU_PAGE_STATUS);
    } else if (menuSelected == 1) {
      resetMenu(MENU_PAGE_PEERS);
    } else if (menuSelected == 2) {
      if (strlen(composeBuffer) > 0) {
        sendTypedMessage(composeBuffer);
        composeBuffer[0] = '\0';
        updateComposeUI();
      } else {
        cardputerUI.updateStatus("Type message in Chat", "Then return to broadcast");
        cardputerUI.setScreen(CARD_SCREEN_CHAT);
        cardputerUI.refresh(true);
      }
    } else {
      resetMenu(MENU_PAGE_HOME);
    }
    return;
  }

  if (menuPage == MENU_PAGE_SETTINGS) {
    if (menuSelected == 0) {
      strncpy(editBuffer, unitName.c_str(), sizeof(editBuffer) - 1);
      editBuffer[sizeof(editBuffer) - 1] = '\0';
      resetMenu(MENU_PAGE_EDIT_NAME);
    } else if (menuSelected == 1) {
      snprintf(editBuffer, sizeof(editBuffer), "%06lu", (unsigned long)currentPasskey);
      resetMenu(MENU_PAGE_EDIT_PASSKEY);
    } else if (menuSelected == 2) {
      uint8_t next = fieldBrightness >= 220 ? 70 : (fieldBrightness >= 140 ? 220 : 140);
      applyBrightness(next, true);
      renderMenu();
    } else if (menuSelected == 3) {
      setSpeakerMuted(!speakerMuted, true);
      cardputerUI.updateStatus("Speaker", speakerMuted ? "muted" : "on");
      renderMenu();
    } else if (menuSelected == 4) {
      TerminalMode next = TERM_NORMAL;
      switch (terminalMgr.getMode()) {
        case TERM_QUIET: next = TERM_NORMAL; break;
        case TERM_NORMAL: next = TERM_VERBOSE; break;
        case TERM_VERBOSE: next = TERM_MONITOR; break;
        case TERM_MONITOR: next = TERM_QUIET; break;
      }
      terminalMgr.setMode(next);
      renderMenu();
    } else if (menuSelected == 5) {
      terminalMgr.setAnsiEnabled(!terminalMgr.isAnsiEnabled());
      renderMenu();
    } else if (menuSelected == 6) {
      savedDefaultScreen = (savedDefaultScreen + 1) % 4;
      DeviceSettings::saveDefaultScreen(savedDefaultScreen);
      cardputerUI.updateStatus("Default screen saved", "Applies after restart");
      renderMenu();
    } else if (menuSelected == 7) {
      savedMeshChannel++;
      if (savedMeshChannel > 13) savedMeshChannel = 1;
      DeviceSettings::saveMeshChannel(savedMeshChannel);
      cardputerUI.updateStatus("Channel saved", "Restart needed");
      renderMenu();
    } else {
      resetMenu(MENU_PAGE_HOME);
    }
    return;
  }

  if (menuPage == MENU_PAGE_SYSTEM) {
    if (menuSelected == 0) {
      const char* lines[] = { "Cypher Cardputer", FIRMWARE_VERSION, __DATE__, __TIME__ };
      cardputerUI.drawInfoList("Version", lines, 4, 0, "Del back");
    } else if (menuSelected == 1) {
      resetMenu(MENU_PAGE_STATUS);
    } else if (menuSelected == 2) {
      clearLocalHistory();
      renderMenu();
    } else if (menuSelected == 3) {
      fieldConsoleDump();
      cardputerUI.updateStatus("Dump sent", "USB/BLE terminal");
      renderMenu();
    } else if (menuSelected == 4) {
      fieldConsoleResetSettings();
    } else if (menuSelected == 5) {
      cardputerUI.updateStatus("Restarting", "3 seconds");
      cardputerUI.refresh(true);
      delay(3000);
      ESP.restart();
    } else if (menuSelected == 6) {
      cardputerUI.updateStatus("Returning", "Cypher Putter OS");
      cardputerUI.refresh(true);
      cypherPuterReturnToLauncher(750);
    } else {
      resetMenu(MENU_PAGE_HOME);
    }
    return;
  }

  if (menuPage == MENU_PAGE_MODES) {
    if (menuSelected <= 3) {
      fieldMode = (FieldMode)menuSelected;
      applyFieldMode(true);
      renderMenu();
    } else {
      resetMenu(MENU_PAGE_HOME);
    }
    return;
  }

  if (menuPage == MENU_PAGE_DIAGNOSTICS) {
    if (menuSelected == 0) {
      fieldConsoleDump();
      cardputerUI.updateStatus("Dump sent", "USB/BLE terminal");
      renderMenu();
    } else if (menuSelected == 1) {
      fieldConsoleLogs();
      cardputerUI.updateStatus("Logs sent", "USB/BLE terminal");
      renderMenu();
    } else if (menuSelected == 2) {
      fieldConsoleResetSettings();
    } else if (menuSelected == 3) {
      cardputerUI.updateStatus("Restarting", "3 seconds");
      cardputerUI.refresh(true);
      delay(3000);
      ESP.restart();
    } else {
      resetMenu(MENU_PAGE_HOME);
    }
    return;
  }

  if (menuPage == MENU_PAGE_EMERGENCY) {
    if (menuSelected == 0) {
      setEmergencyConfirm(true);
    } else if (menuSelected == 1) {
      cancelEmergency();
      renderMenu();
    } else {
      resetMenu(MENU_PAGE_HOME);
    }
    return;
  }

  if (menuPage == MENU_PAGE_EDIT_NAME) {
    if (saveEditedName()) resetMenu(MENU_PAGE_SETTINGS);
    else renderMenu();
    return;
  }

  if (menuPage == MENU_PAGE_EDIT_PASSKEY) {
    if (saveEditedPasskey()) resetMenu(MENU_PAGE_SETTINGS);
    else renderMenu();
  }
}

void menuBack() {
  if (menuPage == MENU_PAGE_HOME) {
    cardputerUI.setScreen(CARD_SCREEN_STATUS);
    cardputerUI.refresh(true);
    return;
  }
  if (menuPage == MENU_PAGE_EDIT_NAME || menuPage == MENU_PAGE_EDIT_PASSKEY) {
    resetMenu(MENU_PAGE_SETTINGS);
    return;
  }
  resetMenu(MENU_PAGE_HOME);
}

bool keyPositionPressed(int x, int y) {
  const auto& pressed = M5Cardputer.Keyboard.keyList();
  for (const auto& key : pressed) {
    if (key.x == x && key.y == y) {
      return true;
    }
  }
  return false;
}

bool isCardputerArrowUp() {
  auto& keys = M5Cardputer.Keyboard.keysState();
  if (!keys.fn) {
    return false;
  }

  for (char c : keys.word) {
    if (c == '9') {
      return true;
    }
  }

  // Cardputer blue up-arrow legend is handled as Fn + 9 on the key matrix.
  return keyPositionPressed(9, 0);
}

bool isCardputerArrowDown() {
  auto& keys = M5Cardputer.Keyboard.keysState();
  if (!keys.fn) {
    return false;
  }

  for (char c : keys.word) {
    if (c == '5') {
      return true;
    }
  }

  // Cardputer blue down-arrow legend is handled as Fn + 5 on the key matrix.
  return keyPositionPressed(5, 0);
}

void handleMenuKey(char c, bool fnHeld) {
  if (menuPage == MENU_PAGE_EDIT_NAME || menuPage == MENU_PAGE_EDIT_PASSKEY) {
    if (c == '\b') {
      size_t len = strlen(editBuffer);
      if (len > 0) editBuffer[len - 1] = '\0';
      renderMenu();
      return;
    }
    if (c == '\n') {
      menuEnter();
      return;
    }
    if (c == 27) {
      menuBack();
      return;
    }
    size_t len = strlen(editBuffer);
    bool isPasskey = menuPage == MENU_PAGE_EDIT_PASSKEY;
    if (len < sizeof(editBuffer) - 1 && c >= 0x20 && c <= 0x7E) {
      if (!isPasskey || isdigit(c)) {
        editBuffer[len] = c;
        editBuffer[len + 1] = '\0';
        renderMenu();
      }
    }
    return;
  }

  if (c == 'w' || c == 'W' || c == 'k' || c == 'K' || (fnHeld && c == '9')) {
    menuMove(-1);
  } else if (c == 's' || c == 'S' || c == 'j' || c == 'J' || (fnHeld && c == '5')) {
    menuMove(1);
  } else if (c == '\n') {
    menuEnter();
  } else if (c == '\b' || c == 27 || c == 'q' || c == 'Q') {
    menuBack();
  } else if (menuPage == MENU_PAGE_MESSAGES && c >= '1' && c <= '4') {
    simulateButtonPress(c - '1');
    renderMenu();
  }
}

void handleCardputerKeyboard() {
  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
    return;
  }

  auto& keys = M5Cardputer.Keyboard.keysState();

  if (emergencyConfirm) {
    if (keys.enter) {
      broadcastEmergency();
    } else if (keys.del) {
      setEmergencyConfirm(false);
      cardputerUI.updateStatus("Emergency canceled", "Confirmation cleared");
    }
    return;
  }

  if (cardputerUI.getScreen() == CARD_SCREEN_MENU) {
    if (isCardputerArrowUp()) {
      menuMove(-1);
      return;
    }
    if (isCardputerArrowDown()) {
      menuMove(1);
      return;
    }
    if (keys.enter) {
      handleMenuKey('\n', keys.fn);
      return;
    }
    if (keys.del) {
      handleMenuKey('\b', keys.fn);
      return;
    }
    if (keys.tab) {
      cardputerUI.nextScreen();
      cardputerUI.refresh(true);
      return;
    }
    for (char c : keys.word) {
      handleMenuKey(c, keys.fn);
    }
    return;
  }

  if (keys.tab || (keys.fn && keys.word.empty())) {
    cardputerUI.nextScreen();
    cardputerUI.refresh(true);
    beep();
    return;
  }

  if (keys.del) {
    size_t len = strlen(composeBuffer);
    if (len > 0) {
      composeBuffer[len - 1] = '\0';
      updateComposeUI();
    }
    return;
  }

  if (keys.enter) {
    sendTypedMessage(composeBuffer);
    composeBuffer[0] = '\0';
    updateComposeUI();
    cardputerUI.setScreen(CARD_SCREEN_CHAT);
    return;
  }

  for (char c : keys.word) {
    if ((c == 'm' || c == 'M') && cardputerUI.getScreen() != CARD_SCREEN_CHAT) {
      resetMenu(MENU_PAGE_HOME);
      continue;
    }

    if (keys.fn && c >= '1' && c <= '4') {
      simulateButtonPress(c - '1');
      continue;
    }

    if (keys.fn && (c == 'e' || c == 'E')) {
      setEmergencyConfirm(true);
      continue;
    }

    if ((c == 'c' || c == 'C') && cardputerUI.getScreen() == CARD_SCREEN_MENU && strlen(composeBuffer) == 0) {
      cancelEmergency();
      continue;
    }

    if (cardputerUI.getScreen() == CARD_SCREEN_MENU && strlen(composeBuffer) == 0 && c >= '1' && c <= '4') {
      simulateButtonPress(c - '1');
      continue;
    }

    if (cardputerUI.getScreen() == CARD_SCREEN_MENU && (c == 'e' || c == 'E') && strlen(composeBuffer) == 0) {
      setEmergencyConfirm(true);
      continue;
    }

    size_t len = strlen(composeBuffer);
    if (len < sizeof(composeBuffer) - 1 && c >= 0x20 && c <= 0x7E) {
      composeBuffer[len] = c;
      composeBuffer[len + 1] = '\0';
      cardputerUI.setScreen(CARD_SCREEN_CHAT);
      updateComposeUI();
    }
  }
}

void fieldConsoleDump() {
  uint8_t mac[6];
  meshMgr.getMyMac(mac);
  char uptime[18];
  unsigned long total = millis() / 1000;
  snprintf(uptime, sizeof(uptime), "%luh%02lum%02lus", total / 3600, (total / 60) % 60, total % 60);

  output.println();
  output.println("===== CYPHER FIELD CONSOLE DUMP =====");
  output.printf("Firmware: %s (%s %s)\n", FIRMWARE_VERSION, __DATE__, __TIME__);
  output.printf("Name: %s  Passkey: %06lu\n", unitName.c_str(), (unsigned long)currentPasskey);
  output.printf("Mode: %s  Brightness: %u  Speaker: %s\n", fieldModeLabel(), fieldBrightness, speakerMuted ? "muted" : "on");
  output.printf("Saved mesh channel: %u  Active channel: %u\n", savedMeshChannel, MESH_CHANNEL);
  output.printf("Battery: %d%%  Uptime: %s  Heap: %luKB\n", M5Cardputer.Power.getBatteryLevel(), uptime, ESP.getFreeHeap() / 1024);
  output.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  output.printf("BLE UART: %s\n", bleUARTMgr.isConnected() ? "connected" : "waiting");
#if MESH_ENABLED
  output.printf("Mesh: %s  Peers: %d  TX:%lu RX:%lu Relay:%lu Drop:%lu Store:%d\n",
                meshMgr.isRunning() ? "running" : "stopped",
                meshMgr.getOnlinePeerCount(),
                meshMgr.getMessagesSent(),
                meshMgr.getMessagesReceived(),
                meshMgr.getMessagesRelayed(),
                meshMgr.getMessagesDropped(),
                meshMgr.getStoredMessageCount());
#endif
  output.printf("Messages: all:%d in:%d out:%d emergency:%d\n",
                messageStore.count(MSG_FILTER_ALL),
                messageStore.count(MSG_FILTER_INCOMING),
                messageStore.count(MSG_FILTER_OUTGOING),
                messageStore.count(MSG_FILTER_EMERGENCY));

  auto peers = meshMgr.getPeers();
  output.println("-- Peers --");
  if (peers.empty()) {
    output.println("(none)");
  } else {
    for (const auto& peer : peers) {
      char line[96];
      peerDirectory.formatPeerLine(peer, line, sizeof(line));
      output.print("  ");
      output.print(line);
      output.print(" trust:");
      output.println(peerDirectory.trustLabel(peer.mac));
    }
  }
  output.println("=====================================");
  output.println();
}

void fieldConsoleLogs() {
  output.println();
  output.println("===== RECENT FIELD LOGS =====");
  int count = min(messageStore.count(messageFilter), 16);
  if (count == 0) {
    output.println("(no messages)");
  }
  for (int i = 0; i < count; i++) {
    char line[140];
    if (messageStore.formatNewest(i, messageFilter, line, sizeof(line))) {
      output.println(line);
    }
  }
  output.println("=============================");
  output.println();
}

void fieldConsoleResetSettings() {
  DeviceSettings::clear();
  cardputerUI.updateStatus("Settings cleared", "Restarting");
  cardputerUI.refresh(true);
  output.println("Device settings cleared. Restarting...");
  delay(1200);
  ESP.restart();
}

void fieldConsoleSetBrightness(const char* args) {
  if (!args || strlen(args) == 0) {
    output.printf("Brightness: %u\nUsage: brightness <40-255>\n", fieldBrightness);
    return;
  }
  int value = atoi(args);
  if (value < 40 || value > 255) {
    output.println("Usage: brightness <40-255>");
    return;
  }
  applyBrightness((uint8_t)value, true);
  output.printf("Brightness saved: %u\n", fieldBrightness);
}

void fieldConsoleSetSpeaker(const char* args) {
  if (!args || strlen(args) == 0) {
    output.printf("Speaker: %s\nUsage: speaker <on|off|mute>\n", speakerMuted ? "muted" : "on");
    return;
  }
  if (strcmp(args, "on") == 0 || strcmp(args, "1") == 0) {
    setSpeakerMuted(false, true);
  } else if (strcmp(args, "off") == 0 || strcmp(args, "mute") == 0 || strcmp(args, "0") == 0) {
    setSpeakerMuted(true, true);
  } else {
    output.println("Usage: speaker <on|off|mute>");
    return;
  }
  output.printf("Speaker saved: %s\n", speakerMuted ? "muted" : "on");
}

void fieldConsoleSetRelayMode(const char* args) {
  if (!args || strlen(args) == 0) {
    output.printf("Field mode: %s\nUsage: relay <normal|quiet|monitor|base>\n", fieldModeLabel());
    return;
  }
  if (strcmp(args, "normal") == 0 || strcmp(args, "off") == 0) {
    fieldMode = FIELD_MODE_NORMAL;
  } else if (strcmp(args, "quiet") == 0) {
    fieldMode = FIELD_MODE_QUIET;
  } else if (strcmp(args, "monitor") == 0) {
    fieldMode = FIELD_MODE_MONITOR;
  } else if (strcmp(args, "base") == 0 || strcmp(args, "relay") == 0) {
    fieldMode = FIELD_MODE_BASE_RELAY;
  } else {
    output.println("Usage: relay <normal|quiet|monitor|base>");
    return;
  }
  applyFieldMode(true);
  output.printf("Field mode saved: %s\n", fieldModeLabel());
}

void fieldConsoleSetPeerName(const char* args) {
  if (!args || strlen(args) == 0) {
    output.println("Usage: peername <mac-suffix> <nickname>");
    return;
  }
  char suffix[20] = "";
  char nickname[MAX_UNIT_NAME_LEN + 1] = "";
  sscanf(args, "%19s %16s", suffix, nickname);
  if (strlen(suffix) == 0 || strlen(nickname) == 0) {
    output.println("Usage: peername <mac-suffix> <nickname>");
    return;
  }
  if (peerDirectory.setNicknameBySuffix(suffix, nickname)) {
    output.println("Peer nickname saved.");
  } else {
    output.println("Peer not found. Use peers/dump after discovery, then try suffix like 3FA4.");
  }
}

void fieldConsoleSetPeerTrust(const char* args) {
  if (!args || strlen(args) == 0) {
    output.println("Usage: trust <mac-suffix> <unknown|known|trusted>");
    return;
  }
  char suffix[20] = "";
  char label[12] = "";
  sscanf(args, "%19s %11s", suffix, label);
  PeerTrust trust = PEER_TRUST_KNOWN;
  if (strcmp(label, "unknown") == 0) trust = PEER_TRUST_UNKNOWN;
  else if (strcmp(label, "known") == 0) trust = PEER_TRUST_KNOWN;
  else if (strcmp(label, "trusted") == 0) trust = PEER_TRUST_TRUSTED;
  else {
    output.println("Usage: trust <mac-suffix> <unknown|known|trusted>");
    return;
  }
  if (peerDirectory.setTrustBySuffix(suffix, trust)) {
    output.println("Peer trust saved.");
  } else {
    output.println("Peer not found. Use peers/dump after discovery, then try suffix like 3FA4.");
  }
}

void configurePasskey() {
  cardputerUI.updateStatus("Enter 6-digit PIN", "Enter=save, 10s default");
  cardputerUI.setScreen(CARD_SCREEN_CHAT);
  cardputerUI.refresh(true);
  output.println("Enter 6-digit passkey on USB serial or Cardputer keyboard.");
  output.println("Press Enter for default 123456. Timeout: 10 seconds.");

  unsigned long start = millis();
  char input[PASSKEY_DIGITS + 1] = "";
  size_t inputLen = 0;
  bool done = false;

  while (!done && millis() - start < 10000) {
    M5Cardputer.update();

    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        done = true;
        break;
      }
      if (isdigit(c) && inputLen < PASSKEY_DIGITS) {
        input[inputLen++] = c;
        input[inputLen] = '\0';
      }
    }

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      auto& keys = M5Cardputer.Keyboard.keysState();
      if (keys.enter) {
        done = true;
        break;
      }
      if (keys.del && inputLen > 0) {
        input[--inputLen] = '\0';
      }
      for (char c : keys.word) {
        if (isdigit(c) && inputLen < PASSKEY_DIGITS) {
          input[inputLen++] = c;
          input[inputLen] = '\0';
        }
      }

      char masked[16] = "";
      for (size_t i = 0; i < inputLen && i < sizeof(masked) - 1; i++) {
        masked[i] = '*';
      }
      cardputerUI.setInputBuffer(masked);
      cardputerUI.refresh(true);
    }

    delay(5);
  }

  if (inputLen == PASSKEY_DIGITS) {
    uint32_t newPasskey = atoi(input);
    if (newPasskey >= MIN_PASSKEY && newPasskey <= MAX_PASSKEY) {
      currentPasskey = newPasskey;
      DeviceSettings::savePasskey(currentPasskey);
      output.printf("Passkey set to: %06lu\n", (unsigned long)currentPasskey);
      cardputerUI.updateStatus("PIN configured", "Secure mesh passkey set");
      composeBuffer[0] = '\0';
      updateComposeUI();
      return;
    }
  }

  currentPasskey = DEFAULT_PASSKEY;
  DeviceSettings::savePasskey(currentPasskey);
  output.println("Using default passkey: 123456");
  cardputerUI.updateStatus("Default PIN active", "Change with passkey command");
  composeBuffer[0] = '\0';
  updateComposeUI();
}

void setup() {
  Serial.begin(TERMINAL_BAUD);
  delay(100);

  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);

  output.begin();

  isServer = false;
  DeviceSettingsState settings = DeviceSettings::load();
  passkeyLoadedFromFlash = settings.hasPasskey;
  fieldBrightness = settings.brightness;
  speakerMuted = settings.speakerMuted;
  savedDefaultScreen = settings.defaultScreen;
  if (savedDefaultScreen > CARD_SCREEN_MENU) savedDefaultScreen = CARD_SCREEN_STATUS;
  savedMeshChannel = settings.meshChannel;
  fieldMode = settings.fieldMode <= FIELD_MODE_BASE_RELAY ? (FieldMode)settings.fieldMode : FIELD_MODE_NORMAL;

  if (!settings.hasName) {
    uint64_t chip = ESP.getEfuseMac();
    char generatedName[MAX_UNIT_NAME_LEN + 1];
    snprintf(generatedName, sizeof(generatedName), "CARD_%04X", (uint16_t)(chip & 0xFFFF));
    unitName = String(generatedName);
    DeviceSettings::saveName(unitName.c_str());
  }

  peerDirectory.begin();
  cardputerUI.begin();
  applyBrightness(fieldBrightness, false);
  renderBootCheck("Display", "ST7789 ready");
  renderBootCheck("Keyboard", "Cardputer keys ready");
  renderBootCheck("Settings", settings.firstRun ? "first run defaults" : "flash loaded");
  terminalMgr.begin();
  applyFieldMode(false);

  if (passkeyLoadedFromFlash) {
    output.printf("Using saved passkey: %06lu\n", (unsigned long)currentPasskey);
    cardputerUI.updateStatus("Saved PIN loaded", "Mesh passkey from flash");
  } else {
    configurePasskey();
  }

#if BLE_UART_ENABLED
  renderBootCheck("BLE UART", "starting Nordic UART");
  bleUARTMgr.begin(unitName.c_str());
#endif

  terminalMgr.printConfiguration();

#if MESH_ENABLED
  renderBootCheck("Mesh", "starting ESP-NOW");
  cardputerUI.updateStatus("Starting mesh", "ESP-NOW channel 1");
  if (meshMgr.begin(unitName.c_str(), currentPasskey)) {
    output.println("Mesh networking enabled (ESP-NOW)");
    meshMgr.onMessage(onMeshMessage);
    meshMgr.onPeerUpdate(onMeshPeerUpdate);
    connectionState.setState(STATE_CONNECTED);
  } else {
    output.println("WARNING: Mesh init failed");
    connectionState.setState(STATE_ERROR);
  }
#endif

  cardputerUI.setConnectionState(connectionState.getState());
  cardputerUI.setScreen((CardputerScreen)savedDefaultScreen);
  cardputerUI.refresh(true);
}

void loop() {
  M5Cardputer.update();
  terminalMgr.poll();
  handleEmergency();
  handleCardputerKeyboard();

#if MESH_ENABLED
  meshMgr.update();
#endif

  cardputerUI.setConnectionState(connectionState.getState());
  terminalMgr.update();
  if (cardputerUI.getScreen() != CARD_SCREEN_MENU) {
    cardputerUI.refresh();
  }
  delay(5);
}

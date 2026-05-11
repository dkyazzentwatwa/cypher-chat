#include "Config.h"
#include "MessageAuth.h"
#include "OutputManager.h"
#if BLE_UART_ENABLED
#include "BLEUARTManager.h"
#endif
#include "TerminalManager.h"
#include "MeshManager.h"
#include "StateManager.h"
#include "CardputerUI.h"
#include "InputPort.h"
#include "PowerPort.h"
#include "GpsPort.h"
#include "DeviceSettings.h"
#include "UsbConsole.h"

#if BOARD_HAS_CARDPUTER_INPUT
#include <M5Cardputer.h>
#endif

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

enum CardputerMenuPage {
  MENU_PAGE_HOME,
  MENU_PAGE_STATUS,
  MENU_PAGE_MESSAGES,
  MENU_PAGE_MESH,
  MENU_PAGE_PEERS,
  MENU_PAGE_SETTINGS,
  MENU_PAGE_SYSTEM,
  MENU_PAGE_EMERGENCY,
  MENU_PAGE_EDIT_NAME,
  MENU_PAGE_EDIT_PASSKEY
};

static CardputerMenuPage menuPage = MENU_PAGE_HOME;
static int menuSelected = 0;
static int menuScroll = 0;
static char editBuffer[MAX_MESSAGE_SIZE] = "";

void sendTypedMessage(const char* text);
void handleInputEvents();
bool handleVisualDisplayInputEvent(const InputEvent& event);
void configurePasskey();
void updateComposeUI();
void setEmergencyConfirm(bool active);
void renderMenu();
void resetMenu(CardputerMenuPage page);
void menuMove(int delta);
void menuEnter();
void menuBack();
void handleMenuKey(char c, bool fnHeld);
bool saveEditedName();
bool saveEditedPasskey();
const char* terminalModeLabel();

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
#if BOARD_HAS_CARDPUTER_INPUT && CARDPUTER_SPEAKER_ENABLED
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
  cardputerUI.addMessage(msg, outgoing, strstr(msg, "EMERGENCY") != nullptr);
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
  GPSCoordinates gps = gpsPort.coordinates();
  uint32_t meshMsgId = meshMgr.sendEmergency(baseMsg, gps.valid ? &gps : nullptr);
  if (meshMsgId > 0) {
    output.printf("[EMERGENCY] Broadcast sent (TTL: %d hops)\n", MESH_MAX_TTL);
  } else {
    output.println("[EMERGENCY] Broadcast failed");
  }
#endif

  addToHistory(baseMsg);
  cardputerUI.updateStatus("EMERGENCY ACTIVE", "Broadcasting for 30 sec");
  cardputerUI.setEmergencyStatus(true, EMERGENCY_DURATION / 1000);
  setEmergencyConfirm(false);

#if BOARD_HAS_CARDPUTER_INPUT && CARDPUTER_SPEAKER_ENABLED
  M5Cardputer.Speaker.tone(1200, 90);
  delay(25);
  M5Cardputer.Speaker.tone(1800, 90);
#endif
}

void cancelEmergency() {
  emergencyActive = false;
  output.println("[EMERGENCY] Emergency canceled");
  cardputerUI.updateStatus("Emergency canceled", "");
  cardputerUI.setEmergencyStatus(false, 0);
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
  cardputerUI.setEmergencyStatus(true, remaining);
}

void setEmergencyConfirm(bool active) {
  emergencyConfirm = active;
  cardputerUI.setEmergencyConfirm(active);
  if (active) {
#if BOARD_IS_WAVESHARE_VISUAL_DISPLAY
    cardputerUI.setScreen(CARD_SCREEN_EMERGENCY);
    cardputerUI.updateStatus("SOS display only", "Use terminal: emergency");
#else
    cardputerUI.setScreen(CARD_SCREEN_MENU);
    cardputerUI.updateStatus("Confirm SOS", "Enter sends, Backspace cancels");
#endif
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

int menuItemCount(CardputerMenuPage page) {
  switch (page) {
    case MENU_PAGE_HOME: return 6;
    case MENU_PAGE_STATUS: return 8;
    case MENU_PAGE_MESSAGES: return 7;
    case MENU_PAGE_MESH: return 4;
    case MENU_PAGE_PEERS: {
      int count = meshMgr.getPeers().size();
      return count > 0 ? count : 2;
    }
    case MENU_PAGE_SETTINGS: return 5;
    case MENU_PAGE_SYSTEM: return 5;
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
  if (menuPage == MENU_PAGE_STATUS || menuPage == MENU_PAGE_PEERS) {
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
  cardputerUI.updateStatus("Local history cleared", "");
}

void renderMenu() {
#if BOARD_IS_WAVESHARE_VISUAL_DISPLAY
  cardputerUI.setScreen(CARD_SCREEN_STATUS);
  cardputerUI.updateStatus("Display only", "Use USB/BLE terminal for controls");
  cardputerUI.refresh(true);
  return;
#endif

  static const char* const homeItems[] = {
    "Status", "Messages", "Mesh", "Settings", "System", "Emergency"
  };
  static const char* const messageItems[] = {
    "Recent history", "Send ACK", "Send ENROUTE", "Send NEED HELP", "Send ALL GOOD", "Open chat", "Back"
  };
  static const char* const meshItems[] = {
    "Mesh stats", "Peers", "Broadcast typed msg", "Back"
  };
  static const char* const emergencyItems[] = {
    "Confirm SOS", "Cancel emergency", "Back"
  };

  if (menuPage == MENU_PAGE_HOME) {
    cardputerUI.drawMenuList("Menu", homeItems, 6, menuSelected, menuScroll, "Up/Dn or W/S  Enter");
    return;
  }

  if (menuPage == MENU_PAGE_MESSAGES) {
    cardputerUI.drawMenuList("Messages", messageItems, 7, menuSelected, menuScroll, "Enter run  Bksp back");
    return;
  }

  if (menuPage == MENU_PAGE_MESH) {
    cardputerUI.drawMenuList("Mesh", meshItems, 4, menuSelected, menuScroll, "Enter run  Bksp back");
    return;
  }

  if (menuPage == MENU_PAGE_SETTINGS) {
    char modeLine[32];
    char ansiLine[32];
    snprintf(modeLine, sizeof(modeLine), "Terminal mode: %s", terminalModeLabel());
    snprintf(ansiLine, sizeof(ansiLine), "ANSI: %s", terminalMgr.isAnsiEnabled() ? "on" : "off");
    const char* items[] = { "Edit device name", "Edit passkey", modeLine, ansiLine, "Back" };
    cardputerUI.drawMenuList("Settings", items, 5, menuSelected, menuScroll, "Enter edit/toggle");
    return;
  }

  if (menuPage == MENU_PAGE_SYSTEM) {
    static const char* const systemItems[] = {
      "Version", "Memory + uptime", "Clear local history", "Restart", "Back"
    };
    cardputerUI.drawMenuList("System", systemItems, 5, menuSelected, menuScroll, "Enter run  Bksp back");
    return;
  }

  if (menuPage == MENU_PAGE_EMERGENCY) {
    cardputerUI.drawMenuList("Emergency", emergencyItems, 3, menuSelected, menuScroll, "Enter run  Bksp back");
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
    snprintf(line2, sizeof(line2), "BLE: %s", bleUARTMgr.isConnected() ? "connected" : "waiting");
    String power = powerPort.menuLabel();
    if (power.length() == 0) {
      power = "Power: n/a";
    }
    snprintf(line3, sizeof(line3), "%s", power.c_str());
    snprintf(line4, sizeof(line4), "Uptime: %s", uptime);
    snprintf(line5, sizeof(line5), "Heap: %luKB", ESP.getFreeHeap() / 1024);
    snprintf(line6, sizeof(line6), "MAC: %02X:%02X:%02X", mac[3], mac[4], mac[5]);
    snprintf(line7, sizeof(line7), "PIN: %06lu", (unsigned long)currentPasskey);
    const char* lines[] = { line0, line1, line2, line3, line4, line5, line6, line7 };
    cardputerUI.drawInfoList("Status", lines, 8, menuScroll, "Up/Dn scroll  Bksp");
    return;
  }

  if (menuPage == MENU_PAGE_PEERS) {
    static char peerLines[8][40];
    const char* lines[8];
    int count = 0;
    auto peers = meshMgr.getPeers();
    if (peers.empty()) {
      lines[count++] = "No peers discovered";
      lines[count++] = "Wait for heartbeat";
    } else {
      for (const auto& peer : peers) {
        if (count >= 8) break;
        if (strlen(peer.unitName) > 0) {
          snprintf(peerLines[count], sizeof(peerLines[count]), "%s %ddBm h%d", peer.unitName, peer.rssi, peer.hopDistance);
        } else {
          snprintf(peerLines[count], sizeof(peerLines[count]), "%02X:%02X %ddBm h%d", peer.mac[4], peer.mac[5], peer.rssi, peer.hopDistance);
        }
        lines[count] = peerLines[count];
        count++;
      }
    }
    cardputerUI.drawInfoList("Peers", lines, count, menuScroll, "Up/Dn scroll  Bksp");
    return;
  }

  if (menuPage == MENU_PAGE_EDIT_NAME) {
    cardputerUI.drawEditView("Device Name", editBuffer, false, "Enter save  Bksp del");
    return;
  }

  if (menuPage == MENU_PAGE_EDIT_PASSKEY) {
    cardputerUI.drawEditView("Passkey", editBuffer, true, "6 digits  Enter save");
  }
}

void menuEnter() {
  if (menuPage == MENU_PAGE_HOME) {
    switch (menuSelected) {
      case 0: resetMenu(MENU_PAGE_STATUS); break;
      case 1: resetMenu(MENU_PAGE_MESSAGES); break;
      case 2: resetMenu(MENU_PAGE_MESH); break;
      case 3: resetMenu(MENU_PAGE_SETTINGS); break;
      case 4: resetMenu(MENU_PAGE_SYSTEM); break;
      case 5: resetMenu(MENU_PAGE_EMERGENCY); break;
    }
    return;
  }

  if (menuPage == MENU_PAGE_MESSAGES) {
    if (menuSelected >= 1 && menuSelected <= 4) {
      simulateButtonPress(menuSelected - 1);
    } else if (menuSelected == 0) {
      static char linesBuf[10][40];
      const char* lines[10];
      int count = min(historyCount, 10);
      int first = historyCount - count;
      for (int i = 0; i < count; i++) {
        int idx = (first + i) % 10;
        snprintf(linesBuf[i], sizeof(linesBuf[i]), "%d %s", i + 1, messageHistory[idx].c_str());
        lines[i] = linesBuf[i];
      }
      if (count == 0) {
        lines[0] = "No messages yet";
        count = 1;
      }
      cardputerUI.drawInfoList("History", lines, count, 0, "Bksp back");
      return;
    } else if (menuSelected == 5) {
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
      TerminalMode next = TERM_NORMAL;
      switch (terminalMgr.getMode()) {
        case TERM_QUIET: next = TERM_NORMAL; break;
        case TERM_NORMAL: next = TERM_VERBOSE; break;
        case TERM_VERBOSE: next = TERM_MONITOR; break;
        case TERM_MONITOR: next = TERM_QUIET; break;
      }
      terminalMgr.setMode(next);
      renderMenu();
    } else if (menuSelected == 3) {
      terminalMgr.setAnsiEnabled(!terminalMgr.isAnsiEnabled());
      renderMenu();
    } else {
      resetMenu(MENU_PAGE_HOME);
    }
    return;
  }

  if (menuPage == MENU_PAGE_SYSTEM) {
    if (menuSelected == 0) {
      const char* lines[] = { "Cypher Chat", BOARD_PROFILE_NAME, FIRMWARE_VERSION, __DATE__, __TIME__ };
      cardputerUI.drawInfoList("Version", lines, 5, 0, "Bksp back");
    } else if (menuSelected == 1) {
      resetMenu(MENU_PAGE_STATUS);
    } else if (menuSelected == 2) {
      clearLocalHistory();
      renderMenu();
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
  } else if (menuPage == MENU_PAGE_HOME && c >= '1' && c <= '6') {
    menuSelected = c - '1';
    menuEnter();
  }
}

bool handleVisualDisplayInputEvent(const InputEvent& event) {
#if BOARD_IS_WAVESHARE_VISUAL_DISPLAY
  switch (event.type) {
    case INPUT_NEXT_SCREEN:
    case INPUT_SWIPE_LEFT:
      cardputerUI.nextScreen();
      cardputerUI.refresh(true);
      beep();
      return true;

    case INPUT_PREV_SCREEN:
    case INPUT_SWIPE_RIGHT:
      cardputerUI.previousScreen();
      cardputerUI.refresh(true);
      beep();
      return true;

    case INPUT_SWIPE_UP:
      cardputerUI.scrollCurrentPage(1);
      cardputerUI.refresh(true);
      return true;

    case INPUT_SWIPE_DOWN:
      cardputerUI.scrollCurrentPage(-1);
      cardputerUI.refresh(true);
      return true;

    case INPUT_TAP:
      if (cardputerUI.getScreen() == CARD_SCREEN_CHAT ||
          cardputerUI.getScreen() == CARD_SCREEN_MESH ||
          cardputerUI.getScreen() == CARD_SCREEN_EMERGENCY ||
          cardputerUI.getScreen() == CARD_SCREEN_STATUS) {
        if (cardputerUI.getScreen() == CARD_SCREEN_CHAT) {
          cardputerUI.scrollCurrentPage(-1000);
        } else if (cardputerUI.getScreen() == CARD_SCREEN_EMERGENCY) {
          cardputerUI.updateStatus("SOS display only", "Use terminal: emergency");
        } else {
          cardputerUI.updateStatus("Display only", "Use USB/BLE terminal");
        }
        cardputerUI.refresh(true);
        return true;
      }
      return true;

    case INPUT_MENU:
    case INPUT_ENTER:
    case INPUT_BACK:
    case INPUT_UP:
    case INPUT_DOWN:
      cardputerUI.refresh(true);
      return true;

    case INPUT_QUICK_1:
    case INPUT_QUICK_2:
    case INPUT_QUICK_3:
    case INPUT_QUICK_4:
    case INPUT_EMERGENCY:
      cardputerUI.updateStatus("Display only", "Use terminal commands");
      cardputerUI.refresh(true);
      return true;

    default:
      return false;
  }
#else
  (void)event;
  return false;
#endif
}

void handleInputEvents() {
  InputEvent event;
  while (inputPort.nextEvent(event)) {
#if BOARD_IS_WAVESHARE_VISUAL_DISPLAY
    if (handleVisualDisplayInputEvent(event)) {
      continue;
    }
#endif
    if (emergencyConfirm) {
      if (event.type == INPUT_ENTER) {
        broadcastEmergency();
      } else if (event.type == INPUT_BACK) {
        setEmergencyConfirm(false);
        cardputerUI.updateStatus("Emergency canceled", "Confirmation cleared");
      }
      continue;
    }

    if (event.type >= INPUT_QUICK_1 && event.type <= INPUT_QUICK_4) {
      simulateButtonPress(event.type - INPUT_QUICK_1);
      continue;
    }
    if (event.type == INPUT_EMERGENCY) {
      setEmergencyConfirm(true);
      continue;
    }

    if (cardputerUI.getScreen() == CARD_SCREEN_MENU) {
      if (event.type == INPUT_UP) {
        menuMove(-1);
      } else if (event.type == INPUT_DOWN) {
        menuMove(1);
      } else if (event.type == INPUT_ENTER) {
        handleMenuKey('\n', event.fn);
      } else if (event.type == INPUT_BACK) {
        handleMenuKey('\b', event.fn);
      } else if (event.type == INPUT_PREV_SCREEN) {
        menuMove(-1);
      } else if (event.type == INPUT_NEXT_SCREEN) {
        menuMove(1);
      } else if (event.type == INPUT_TAB) {
        cardputerUI.nextScreen();
        cardputerUI.refresh(true);
      } else if (event.type == INPUT_CHAR) {
        handleMenuKey(event.ch, event.fn);
      } else if (event.type == INPUT_MENU) {
        cardputerUI.setScreen(CARD_SCREEN_STATUS);
        cardputerUI.refresh(true);
      }
      continue;
    }

    if (event.type == INPUT_PREV_SCREEN) {
      cardputerUI.previousScreen();
      cardputerUI.refresh(true);
      beep();
      continue;
    }
    if (event.type == INPUT_TAB || event.type == INPUT_UP || event.type == INPUT_DOWN ||
        event.type == INPUT_NEXT_SCREEN) {
      cardputerUI.nextScreen();
      cardputerUI.refresh(true);
      beep();
      continue;
    }
    if (event.type == INPUT_MENU) {
      resetMenu(MENU_PAGE_HOME);
      continue;
    }
    if (event.type == INPUT_BACK) {
      size_t len = strlen(composeBuffer);
      if (len > 0) {
        composeBuffer[len - 1] = '\0';
        updateComposeUI();
      } else {
        resetMenu(MENU_PAGE_HOME);
      }
      continue;
    }
    if (event.type == INPUT_ENTER) {
      if (inputPort.hasTextInput() && strlen(composeBuffer) > 0) {
        sendTypedMessage(composeBuffer);
        composeBuffer[0] = '\0';
        updateComposeUI();
        cardputerUI.setScreen(CARD_SCREEN_CHAT);
      } else {
        resetMenu(MENU_PAGE_HOME);
      }
      continue;
    }
    if (event.type == INPUT_CHAR) {
      if ((event.ch == 'm' || event.ch == 'M') && cardputerUI.getScreen() != CARD_SCREEN_CHAT) {
        resetMenu(MENU_PAGE_HOME);
        continue;
      }
      if ((event.ch == 'e' || event.ch == 'E') && strlen(composeBuffer) == 0) {
        setEmergencyConfirm(true);
        continue;
      }
      size_t len = strlen(composeBuffer);
      if (len < sizeof(composeBuffer) - 1 && event.ch >= 0x20 && event.ch <= 0x7E) {
        composeBuffer[len] = event.ch;
        composeBuffer[len + 1] = '\0';
        cardputerUI.setScreen(CARD_SCREEN_CHAT);
        updateComposeUI();
      }
    }
  }
}

void configurePasskey() {
  cardputerUI.updateStatus("Enter 6-digit PIN", "Enter=save, 10s default");
  cardputerUI.setScreen(CARD_SCREEN_CHAT);
  cardputerUI.refresh(true);
  output.printf("Enter 6-digit passkey on USB serial%s.\n",
                inputPort.hasTextInput() ? " or board keyboard" : "");
  output.println("Press Enter for default 123456. Timeout: 10 seconds.");

  unsigned long start = millis();
  char input[PASSKEY_DIGITS + 1] = "";
  size_t inputLen = 0;
  bool done = false;

  while (!done && millis() - start < 10000) {
    inputPort.update();

    while (usbConsole.available()) {
      int next = usbConsole.read();
      if (next < 0) {
        break;
      }
      char c = static_cast<char>(next);
      if (c == '\n' || c == '\r') {
        done = true;
        break;
      }
      if (isdigit(c) && inputLen < PASSKEY_DIGITS) {
        input[inputLen++] = c;
        input[inputLen] = '\0';
      }
    }

    InputEvent event;
    while (inputPort.nextEvent(event)) {
      if (event.type == INPUT_ENTER) {
        done = true;
        break;
      }
      if (event.type == INPUT_BACK && inputLen > 0) {
        input[--inputLen] = '\0';
      }
      if (event.type == INPUT_CHAR && isdigit(event.ch) && inputLen < PASSKEY_DIGITS) {
        input[inputLen++] = event.ch;
        input[inputLen] = '\0';
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
  usbConsole.begin(TERMINAL_BAUD);
  delay(100);
  usbConsole.println();
  usbConsole.println("Cypher Chat setup begin");

  output.begin();
  usbConsole.println("Output ready");
  cardputerUI.begin();
  usbConsole.println("UI ready");
  inputPort.begin();
  usbConsole.println("Input ready");
  gpsPort.begin();
  usbConsole.println("GPS ready");

  isServer = false;
  DeviceSettingsState settings = DeviceSettings::load();
  passkeyLoadedFromFlash = settings.hasPasskey;

  if (passkeyLoadedFromFlash) {
    output.printf("Using saved passkey: %06lu\n", (unsigned long)currentPasskey);
    cardputerUI.updateStatus("Saved PIN loaded", "Mesh passkey from flash");
  } else {
    configurePasskey();
  }

#if BLE_UART_ENABLED
  bleUARTMgr.begin(unitName.c_str());
#endif

  terminalMgr.begin();
  usbConsole.println("Terminal ready");

  terminalMgr.printConfiguration();

#if MESH_ENABLED
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
  cardputerUI.setScreen(CARD_SCREEN_STATUS);
  cardputerUI.refresh(true);
}

void loop() {
  inputPort.update();
  gpsPort.update();
  terminalMgr.poll();
  handleEmergency();
  handleInputEvents();

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

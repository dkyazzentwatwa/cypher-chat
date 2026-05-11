#include "CardputerUI.h"
#include "MeshManager.h"
#include "BLEUARTManager.h"
#include "PeerDirectory.h"

CardputerUI cardputerUI;
extern bool emergencyActive;
extern uint8_t fieldBrightness;
extern bool speakerMuted;

static constexpr uint16_t COLOR_BG = TFT_BLACK;
static constexpr uint16_t COLOR_PANEL = 0x2104;
static constexpr uint16_t COLOR_ACCENT = 0x07FF;
static constexpr uint16_t COLOR_GOOD = 0x07E0;
static constexpr uint16_t COLOR_WARN = 0xFFE0;
static constexpr uint16_t COLOR_DANGER = 0xF800;
static constexpr uint16_t COLOR_MUTED = 0x8410;
static constexpr uint16_t COLOR_TEXT = TFT_WHITE;
static constexpr int HEADER_H = 16;
static constexpr int FOOTER_H = 14;
static constexpr int BODY_TOP = HEADER_H + 2;
static constexpr int BODY_BOTTOM_PAD = FOOTER_H + 1;
static constexpr int ROW_H = 16;

CardputerUI::CardputerUI()
  : _screen(CARD_SCREEN_STATUS),
    _messageCount(0),
    _connectionState(STATE_IDLE),
    _confirmEmergency(false),
    _lastRefresh(0) {
  memset(_messages, 0, sizeof(_messages));
  memset(_statusLine1, 0, sizeof(_statusLine1));
  memset(_statusLine2, 0, sizeof(_statusLine2));
  memset(_input, 0, sizeof(_input));
}

void CardputerUI::begin() {
  auto& lcd = M5Cardputer.Display;
  lcd.setRotation(1);
  lcd.setBrightness(180);
  lcd.fillScreen(COLOR_BG);
  lcd.setTextFont(&fonts::Font2);
  lcd.setTextSize(1);
  updateStatus("Cypher Chat", "Cardputer-Adv ready");
  refresh(true);
}

void CardputerUI::updateStatus(const char* line1, const char* line2) {
  strncpy(_statusLine1, line1 ? line1 : "", sizeof(_statusLine1) - 1);
  _statusLine1[sizeof(_statusLine1) - 1] = '\0';
  strncpy(_statusLine2, line2 ? line2 : "", sizeof(_statusLine2) - 1);
  _statusLine2[sizeof(_statusLine2) - 1] = '\0';
}

void CardputerUI::addMessage(const char* text, bool outgoing, bool emergency) {
  if (_messageCount >= CARDPUTER_HISTORY_SIZE) {
    for (int i = 0; i < CARDPUTER_HISTORY_SIZE - 1; i++) {
      _messages[i] = _messages[i + 1];
    }
    _messageCount = CARDPUTER_HISTORY_SIZE - 1;
  }

  CardputerMessage& msg = _messages[_messageCount++];
  strncpy(msg.text, text ? text : "", sizeof(msg.text) - 1);
  msg.text[sizeof(msg.text) - 1] = '\0';
  msg.timestamp = millis();
  msg.outgoing = outgoing;
  msg.emergency = emergency;
}

void CardputerUI::clearMessages() {
  memset(_messages, 0, sizeof(_messages));
  _messageCount = 0;
}

void CardputerUI::setConnectionState(ConnectionState state) {
  _connectionState = state;
}

void CardputerUI::setScreen(CardputerScreen screen) {
  _screen = screen;
}

void CardputerUI::nextScreen() {
  _screen = static_cast<CardputerScreen>((static_cast<int>(_screen) + 1) % 4);
}

CardputerScreen CardputerUI::getScreen() const {
  return _screen;
}

void CardputerUI::setInputBuffer(const char* text) {
  strncpy(_input, text ? text : "", sizeof(_input) - 1);
  _input[sizeof(_input) - 1] = '\0';
}

void CardputerUI::setEmergencyConfirm(bool active) {
  _confirmEmergency = active;
}

int CardputerUI::messageCount() const {
  return _messageCount;
}

bool CardputerUI::getMessageLine(int visibleIndex, char* out, size_t outSize) const {
  if (!out || outSize == 0 || visibleIndex < 0 || visibleIndex >= _messageCount) {
    return false;
  }
  const CardputerMessage& msg = _messages[visibleIndex];
  snprintf(out, outSize, "%c %s", msg.outgoing ? '>' : '<', msg.text);
  return true;
}

void CardputerUI::refresh(bool force) {
  unsigned long now = millis();
  if (!force && now - _lastRefresh < 120) {
    return;
  }
  _lastRefresh = now;

  switch (_screen) {
    case CARD_SCREEN_STATUS: drawStatus(); break;
    case CARD_SCREEN_CHAT: drawChat(); break;
    case CARD_SCREEN_PEERS: drawPeers(); break;
    case CARD_SCREEN_MENU: drawMenu(); break;
  }
}

void CardputerUI::drawHeader(const char* title) {
  auto& lcd = M5Cardputer.Display;
  lcd.fillScreen(COLOR_BG);
  lcd.setTextFont(&fonts::Font2);
  lcd.setTextSize(1);
  lcd.fillRect(0, 0, lcd.width(), HEADER_H, COLOR_PANEL);
  lcd.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  lcd.setCursor(4, 2);
  lcd.print(title);

  int battery = M5Cardputer.Power.getBatteryLevel();
  lcd.setTextColor(COLOR_TEXT, COLOR_PANEL);
  lcd.setCursor(lcd.width() - 98, 2);
  lcd.printf("M%d", meshMgr.getOnlinePeerCount());
  lcd.setCursor(lcd.width() - 74, 2);
  lcd.print(bleUARTMgr.isConnected() ? "B+" : "B-");
  lcd.setCursor(lcd.width() - 52, 2);
  lcd.print(emergencyActive ? "SOS" : (speakerMuted ? "Q" : " "));
  lcd.setCursor(lcd.width() - 32, 2);
  if (battery >= 0) {
    lcd.printf("%d%%", battery);
  } else {
    lcd.print("USB");
  }
}

void CardputerUI::drawStatus() {
  auto& lcd = M5Cardputer.Display;
  drawHeader("Cypher Chat");

  lcd.setTextColor(COLOR_TEXT, COLOR_BG);
  lcd.setCursor(6, 22);
  lcd.printf("%s  %s", unitName.c_str(), emergencyActive ? "SOS" : stateLabel());
  lcd.setCursor(6, 38);
  lcd.printf("Mesh: %s  Peers: %d", meshMgr.isRunning() ? "ON" : "OFF", meshMgr.getOnlinePeerCount());
  lcd.setCursor(6, 54);
  lcd.printf("BLE:%s  Audio:%s", bleUARTMgr.isConnected() ? "on" : "wait", speakerMuted ? "mute" : "on");

  fitPrint(6, 74, lcd.width() - 12, _statusLine1, COLOR_ACCENT);
  fitPrint(6, 90, lcd.width() - 12, _statusLine2, COLOR_TEXT);

  drawFooter("Tab/Fn screens | M menu");
}

void CardputerUI::drawChat() {
  auto& lcd = M5Cardputer.Display;
  drawHeader("Chat");

  int first = _messageCount > 4 ? _messageCount - 4 : 0;
  int y = BODY_TOP;
  for (int i = first; i < _messageCount; i++) {
    drawMessageLine(y, _messages[i]);
    y += ROW_H;
  }

  lcd.fillRect(0, 99, lcd.width(), 19, COLOR_PANEL);
  lcd.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  lcd.setCursor(4, 103);
  lcd.print("> ");
  lcd.setTextColor(COLOR_TEXT, COLOR_PANEL);
  lcd.setCursor(18, 103);
  const int maxChars = max(1, (int)((lcd.width() - 22) / 6));
  char clipped[96];
  strncpy(clipped, _input, sizeof(clipped) - 1);
  clipped[sizeof(clipped) - 1] = '\0';
  if ((int)strlen(clipped) > maxChars) clipped[maxChars] = '\0';
  lcd.print(clipped);

  drawFooter("Ent send | Del");
}

void CardputerUI::drawPeers() {
  auto& lcd = M5Cardputer.Display;
  drawHeader("Peers");

  auto peers = meshMgr.getPeers();
  lcd.setTextColor(COLOR_TEXT, COLOR_BG);
  if (peers.empty()) {
    lcd.setCursor(8, 46);
    lcd.print("No mesh peers yet.");
    lcd.setCursor(8, 64);
    lcd.print("Keep nearby nodes on channel 1.");
  } else {
    int y = BODY_TOP;
    int shown = 0;
    for (const auto& peer : peers) {
      if (shown >= 5) break;
      lcd.setCursor(6, y);
      lcd.setTextColor(peer.isOnline ? COLOR_GOOD : COLOR_MUTED, COLOR_BG);
      char name[MAX_UNIT_NAME_LEN + 1];
      peerDirectory.displayName(peer, name, sizeof(name));
      lcd.printf("%-12s", name);
      lcd.setTextColor(COLOR_TEXT, COLOR_BG);
      lcd.printf(" %ddBm h%d", peer.rssi, peer.hopDistance);
      y += ROW_H;
      shown++;
    }
  }

  drawFooter("Tab/Fn next | M menu");
}

void CardputerUI::drawMenu() {
  static const char* const fallbackItems[] = {
    "Status", "Messages", "Mesh", "Settings", "System", "Emergency"
  };
  drawMenuList("Menu", fallbackItems, 6, 0, 0, "Up/Dn W/S | Ent");
}

void CardputerUI::drawFooter(const char* hint) {
  auto& lcd = M5Cardputer.Display;
  lcd.fillRect(0, lcd.height() - FOOTER_H, lcd.width(), FOOTER_H, COLOR_PANEL);
  lcd.setTextFont(&fonts::Font0);
  lcd.setTextSize(1);
  lcd.setTextColor(COLOR_MUTED, COLOR_PANEL);
  lcd.setCursor(4, lcd.height() - FOOTER_H + 3);
  const int maxChars = max(1, (int)((lcd.width() - 8) / 5));
  char buffer[64];
  strncpy(buffer, hint ? hint : "", sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  if ((int)strlen(buffer) > maxChars) buffer[maxChars] = '\0';
  lcd.print(buffer);
}

void CardputerUI::drawMessageLine(int y, const CardputerMessage& msg) {
  auto& lcd = M5Cardputer.Display;
  uint16_t color = msg.emergency ? COLOR_DANGER : (msg.outgoing ? COLOR_ACCENT : COLOR_GOOD);
  lcd.setTextColor(color, COLOR_BG);
  lcd.setCursor(5, y);
  lcd.print(msg.outgoing ? ">" : "<");
  fitPrint(18, y, lcd.width() - 22, msg.text, COLOR_TEXT);
}

void CardputerUI::drawMenuList(const char* title, const char* const* items, int itemCount, int selected, int scroll, const char* hint) {
  auto& lcd = M5Cardputer.Display;
  drawHeader(title);

  const int visibleRows = (lcd.height() - BODY_TOP - BODY_BOTTOM_PAD) / ROW_H;
  for (int row = 0; row < visibleRows; row++) {
    int idx = scroll + row;
    if (idx >= itemCount) break;

    int y = BODY_TOP + row * ROW_H;
    bool active = idx == selected;
    uint16_t bg = active ? COLOR_ACCENT : COLOR_BG;
    uint16_t fg = active ? TFT_BLACK : COLOR_TEXT;
    lcd.fillRect(2, y - 1, lcd.width() - 12, ROW_H, bg);
    lcd.setTextColor(fg, bg);
    lcd.setCursor(7, y + 2);
    lcd.print(items[idx]);
  }

  if (itemCount > visibleRows) {
    lcd.setTextColor(COLOR_MUTED, COLOR_BG);
    lcd.setCursor(lcd.width() - 8, BODY_TOP + 2);
    lcd.print(scroll > 0 ? "^" : " ");
    lcd.setCursor(lcd.width() - 8, lcd.height() - BODY_BOTTOM_PAD - 10);
    lcd.print(scroll + visibleRows < itemCount ? "v" : " ");
  }

  drawFooter(hint);
}

void CardputerUI::drawInfoList(const char* title, const char* const* lines, int lineCount, int scroll, const char* hint) {
  auto& lcd = M5Cardputer.Display;
  drawHeader(title);

  const int visibleRows = (lcd.height() - BODY_TOP - BODY_BOTTOM_PAD) / ROW_H;
  for (int row = 0; row < visibleRows; row++) {
    int idx = scroll + row;
    if (idx >= lineCount) break;
    fitPrint(6, BODY_TOP + row * ROW_H + 2, lcd.width() - 16, lines[idx], idx == 0 ? COLOR_ACCENT : COLOR_TEXT);
  }

  if (lineCount > visibleRows) {
    lcd.setTextColor(COLOR_MUTED, COLOR_BG);
    lcd.setCursor(lcd.width() - 8, BODY_TOP + 2);
    lcd.print(scroll > 0 ? "^" : " ");
    lcd.setCursor(lcd.width() - 8, lcd.height() - BODY_BOTTOM_PAD - 10);
    lcd.print(scroll + visibleRows < lineCount ? "v" : " ");
  }

  drawFooter(hint);
}

void CardputerUI::drawEditView(const char* title, const char* value, bool masked, const char* hint) {
  auto& lcd = M5Cardputer.Display;
  drawHeader(title);

  lcd.setTextColor(COLOR_TEXT, COLOR_BG);
  lcd.setCursor(8, 35);
  lcd.print("Edit value:");

  lcd.fillRect(6, 58, lcd.width() - 12, 24, COLOR_PANEL);
  lcd.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  lcd.setCursor(12, 64);
  if (masked) {
    for (size_t i = 0; value && i < strlen(value); i++) {
      lcd.print('*');
    }
  } else {
    lcd.print(value ? value : "");
  }
  lcd.print("_");

  drawFooter(hint);
}

const char* CardputerUI::stateLabel() const {
  switch (_connectionState) {
    case STATE_IDLE: return "idle";
    case STATE_SCANNING: return "scan";
    case STATE_CONNECTING: return "conn";
    case STATE_CONNECTED: return "mesh";
    case STATE_DISCONNECTED: return "disc";
    case STATE_ERROR: return "err";
    default: return "?";
  }
}

const char* CardputerUI::screenLabel() const {
  switch (_screen) {
    case CARD_SCREEN_STATUS: return "STA";
    case CARD_SCREEN_CHAT: return "CHT";
    case CARD_SCREEN_PEERS: return "PEER";
    case CARD_SCREEN_MENU: return "MENU";
    default: return "---";
  }
}

void CardputerUI::fitPrint(int x, int y, int w, const char* text, uint16_t color) {
  auto& lcd = M5Cardputer.Display;
  lcd.setTextColor(color, COLOR_BG);
  lcd.setCursor(x, y);

  const int maxChars = max(1, w / 6);
  char buffer[96];
  strncpy(buffer, text ? text : "", sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  if ((int)strlen(buffer) > maxChars) {
    buffer[maxChars] = '\0';
  }
  lcd.print(buffer);
}

#include "CardputerUI.h"

#include "BLEUARTManager.h"
#include "MeshManager.h"
#include "PowerStatus.h"

CardputerUI cardputerUI;
DisplayPort displayPort;

static constexpr uint16_t COLOR_BG = ST77XX_BLACK;
static constexpr uint16_t COLOR_PANEL = 0x2104;
static constexpr uint16_t COLOR_ACCENT = ST77XX_CYAN;
static constexpr uint16_t COLOR_GOOD = ST77XX_GREEN;
static constexpr uint16_t COLOR_WARN = ST77XX_YELLOW;
static constexpr uint16_t COLOR_DANGER = ST77XX_RED;
static constexpr uint16_t COLOR_MUTED = 0x8410;
static constexpr uint16_t COLOR_TEXT = ST77XX_WHITE;

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
  displayPort.begin();
  powerStatus.begin();
  auto &lcd = displayPort.gfx();
  lcd.fillScreen(COLOR_BG);
  lcd.setTextSize(textScale());
  lcd.setTextWrap(false);
  updateStatus("Cypher Chat", "S3 profile ready");
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
  auto &lcd = displayPort.gfx();
  const int headerH = headerHeight();
  lcd.fillScreen(COLOR_BG);
  lcd.fillRect(0, 0, lcd.width(), headerH, COLOR_PANEL);
  lcd.setTextSize(textScale());
  lcd.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  lcd.setCursor(6, (headerH - 8 * textScale()) / 2);
  lcd.print(title);

  String power = powerStatus.headerLabel();
  if (power.length() == 0) {
    power = BOARD_PROFILE_NAME;
  }

  lcd.setTextColor(COLOR_TEXT, COLOR_PANEL);
  const int charW = 6 * textScale();
  int rightChars = power.length() + 1 + strlen(screenLabel());
  int rightX = lcd.width() - rightChars * charW - 6;
  if (rightX < lcd.width() / 2) {
    rightX = lcd.width() - strlen(screenLabel()) * charW - 6;
    power = "";
  }
  lcd.setCursor(rightX, (headerH - 8 * textScale()) / 2);
  if (power.length() > 0) {
    lcd.print(power);
    lcd.print(" ");
  }
  lcd.print(screenLabel());
}

void CardputerUI::drawStatus() {
  auto &lcd = displayPort.gfx();
  drawHeader("Cypher Chat");
  const int scale = textScale();
  const int row = rowHeight();
  int y = headerHeight() + row / 2;

  lcd.setTextSize(scale);
  lcd.setTextColor(COLOR_TEXT, COLOR_BG);
  lcd.setCursor(8, y);
  lcd.printf("Unit: %s", unitName.c_str());
  y += row;
  lcd.setCursor(8, y);
  lcd.printf("Mesh: %s  Peers: %d", meshMgr.isRunning() ? "ON" : "OFF", meshMgr.getOnlinePeerCount());
  y += row;
  lcd.setCursor(8, y);
  lcd.printf("BLE UART: %s", bleUARTMgr.isConnected() ? "connected" : "waiting");
  y += row;
  fitPrint(8, y, lcd.width() - 16, _statusLine1, COLOR_ACCENT);
  y += row;
  fitPrint(8, y, lcd.width() - 16, _statusLine2, COLOR_TEXT);

  drawFooter(BOARD_HAS_CARDPUTER_INPUT ? "Tab/Fn screens  M menu" : "Top/Bot move  Left back  Right enter");
}

void CardputerUI::drawChat() {
  auto &lcd = displayPort.gfx();
  drawHeader("Chat");

  const int row = rowHeight();
  const int footerH = footerHeight();
  const int maxRows = max(1, (int)((lcd.height() - headerHeight() - footerH - row) / row));
  int first = _messageCount > maxRows ? _messageCount - maxRows : 0;
  int y = headerHeight() + 4;
  for (int i = first; i < _messageCount; i++) {
    drawMessageLine(y, _messages[i]);
    y += row;
  }

  const int composeH = max(20, row + 4);
  lcd.fillRect(0, lcd.height() - footerH - composeH, lcd.width(), composeH, COLOR_PANEL);
  lcd.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  lcd.setCursor(6, lcd.height() - footerH - composeH + 5);
  lcd.print("> ");
  fitPrint(20, lcd.height() - footerH - composeH + 5, lcd.width() - 26, _input, COLOR_TEXT);

  drawFooter(BOARD_HAS_CARDPUTER_INPUT ? "Enter send  Bksp del" : "Use terminal or quick-send menu");
}

void CardputerUI::drawPeers() {
  auto &lcd = displayPort.gfx();
  drawHeader("Peers");

  auto peers = meshMgr.getPeers();
  lcd.setTextSize(textScale());
  if (peers.empty()) {
    fitPrint(8, headerHeight() + rowHeight(), lcd.width() - 16, "No mesh peers yet.", COLOR_TEXT);
    fitPrint(8, headerHeight() + rowHeight() * 2, lcd.width() - 16, "Keep nodes on channel 1.", COLOR_MUTED);
  } else {
    int y = headerHeight() + 4;
    int shown = 0;
    for (const auto& peer : peers) {
      if (shown >= 8) break;
      char line[56];
      if (strlen(peer.unitName) > 0) {
        snprintf(line, sizeof(line), "%s %ddBm h%d", peer.unitName, peer.rssi, peer.hopDistance);
      } else {
        snprintf(line, sizeof(line), "%02X:%02X %ddBm h%d", peer.mac[4], peer.mac[5], peer.rssi, peer.hopDistance);
      }
      fitPrint(8, y, lcd.width() - 16, line, peer.isOnline ? COLOR_GOOD : COLOR_MUTED);
      y += rowHeight();
      shown++;
    }
  }

  drawFooter(BOARD_HAS_CARDPUTER_INPUT ? "Tab/Fn next  M menu" : "Tap center menu");
}

void CardputerUI::drawMenu() {
  static const char* const fallbackItems[] = {
    "Status", "Messages", "Mesh", "Settings", "System", "Emergency"
  };
  drawMenuList("Menu", fallbackItems, 6, 0, 0, "Move/Enter or touch zones");
}

void CardputerUI::drawFooter(const char* hint) {
  auto &lcd = displayPort.gfx();
  const int footerH = footerHeight();
  lcd.fillRect(0, lcd.height() - footerH, lcd.width(), footerH, COLOR_PANEL);
  lcd.setTextSize(textScale());
  lcd.setTextColor(COLOR_MUTED, COLOR_PANEL);
  fitPrint(6, lcd.height() - footerH + max(2, (footerH - 8 * textScale()) / 2), lcd.width() - 12, hint, COLOR_MUTED);
}

void CardputerUI::drawMessageLine(int y, const CardputerMessage& msg) {
  auto &lcd = displayPort.gfx();
  uint16_t color = msg.emergency ? COLOR_DANGER : (msg.outgoing ? COLOR_ACCENT : COLOR_GOOD);
  lcd.setTextSize(textScale());
  lcd.setTextColor(color, COLOR_BG);
  lcd.setCursor(8, y);
  lcd.print(msg.outgoing ? ">" : "<");
  fitPrint(8 + 12 * textScale(), y, lcd.width() - 20, msg.text, COLOR_TEXT);
}

void CardputerUI::drawMenuList(const char* title, const char* const* items, int itemCount, int selected, int scroll, const char* hint) {
  auto &lcd = displayPort.gfx();
  drawHeader(title);

  const int visibleRows = max(1, (int)((lcd.height() - headerHeight() - footerHeight()) / rowHeight()));
  for (int row = 0; row < visibleRows; row++) {
    int idx = scroll + row;
    if (idx >= itemCount) break;
    int y = headerHeight() + row * rowHeight();
    bool active = idx == selected;
    uint16_t bg = active ? COLOR_ACCENT : COLOR_BG;
    uint16_t fg = active ? COLOR_BG : COLOR_TEXT;
    lcd.fillRect(4, y + 2, lcd.width() - 8, rowHeight() - 4, bg);
    lcd.setTextColor(fg, bg);
    lcd.setTextSize(textScale());
    lcd.setCursor(10, y + max(3, (rowHeight() - 8 * textScale()) / 2));
    lcd.print(items[idx]);
  }
  drawFooter(hint);
}

void CardputerUI::drawInfoList(const char* title, const char* const* lines, int lineCount, int scroll, const char* hint) {
  auto &lcd = displayPort.gfx();
  drawHeader(title);
  const int visibleRows = max(1, (int)((lcd.height() - headerHeight() - footerHeight()) / rowHeight()));
  for (int row = 0; row < visibleRows; row++) {
    int idx = scroll + row;
    if (idx >= lineCount) break;
    fitPrint(8, headerHeight() + row * rowHeight() + 4, lcd.width() - 16, lines[idx], idx == 0 ? COLOR_ACCENT : COLOR_TEXT);
  }
  drawFooter(hint);
}

void CardputerUI::drawEditView(const char* title, const char* value, bool masked, const char* hint) {
  auto &lcd = displayPort.gfx();
  drawHeader(title);
  lcd.setTextSize(textScale());
  fitPrint(10, headerHeight() + rowHeight(), lcd.width() - 20, "Edit value:", COLOR_TEXT);
  const int boxY = headerHeight() + rowHeight() * 2;
  lcd.fillRect(8, boxY, lcd.width() - 16, rowHeight() + 8, COLOR_PANEL);
  lcd.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  lcd.setCursor(14, boxY + 5);
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
  auto &lcd = displayPort.gfx();
  lcd.setTextSize(textScale());
  lcd.setTextColor(color, COLOR_BG);
  lcd.setCursor(x, y);
  const int maxChars = max(1, w / (6 * textScale()));
  char buffer[128];
  strncpy(buffer, text ? text : "", sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  if ((int)strlen(buffer) > maxChars) {
    buffer[maxChars] = '\0';
  }
  lcd.print(buffer);
}

uint8_t CardputerUI::textScale() const {
  return DISPLAY_TEXT_SIZE;
}

int CardputerUI::headerHeight() const {
  return BOARD_DISPLAY_IS_SH8601 ? 38 : 18;
}

int CardputerUI::footerHeight() const {
  return BOARD_DISPLAY_IS_SH8601 ? 34 : 14;
}

int CardputerUI::rowHeight() const {
  return BOARD_DISPLAY_IS_SH8601 ? 34 : 17;
}


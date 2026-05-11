#include "CardputerUI.h"

#include "BLEUARTManager.h"
#include "MeshManager.h"
#include "PowerPort.h"

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
static constexpr unsigned long SLOW_REFRESH_MS = 1000;

CardputerUI::CardputerUI()
  : _screen(CARD_SCREEN_STATUS),
    _messageCount(0),
    _connectionState(STATE_IDLE),
    _confirmEmergency(false),
    _emergencyActive(false),
    _emergencySecondsRemaining(0),
    _messageScrollBack(0),
    _meshScroll(0),
    _lastSlowRefresh(0),
    _dirty(true) {
  memset(_messages, 0, sizeof(_messages));
  memset(_statusLine1, 0, sizeof(_statusLine1));
  memset(_statusLine2, 0, sizeof(_statusLine2));
  memset(_input, 0, sizeof(_input));
  memset(_liveSignature, 0, sizeof(_liveSignature));
}

void CardputerUI::begin() {
  displayPort.begin();
  powerPort.begin();
  auto &lcd = displayPort.gfx();
  lcd.fillScreen(COLOR_BG);
  lcd.setTextSize(textScale());
  lcd.setTextWrap(false);
  updateStatus("Cypher Chat", "unified ready");
  refresh(true);
}

void CardputerUI::updateStatus(const char* line1, const char* line2) {
  char nextLine1[sizeof(_statusLine1)];
  char nextLine2[sizeof(_statusLine2)];
  strncpy(nextLine1, line1 ? line1 : "", sizeof(nextLine1) - 1);
  nextLine1[sizeof(nextLine1) - 1] = '\0';
  strncpy(nextLine2, line2 ? line2 : "", sizeof(nextLine2) - 1);
  nextLine2[sizeof(nextLine2) - 1] = '\0';
  if (strcmp(_statusLine1, nextLine1) == 0 && strcmp(_statusLine2, nextLine2) == 0) {
    return;
  }
  strcpy(_statusLine1, nextLine1);
  strcpy(_statusLine2, nextLine2);
  markDirty();
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
  _messageScrollBack = 0;
  markDirty();
}

void CardputerUI::clearMessages() {
  memset(_messages, 0, sizeof(_messages));
  _messageCount = 0;
  _messageScrollBack = 0;
  markDirty();
}

void CardputerUI::setConnectionState(ConnectionState state) {
  if (_connectionState == state) {
    return;
  }
  _connectionState = state;
  markDirty();
}

void CardputerUI::setScreen(CardputerScreen screen) {
  if (_screen == screen) {
    return;
  }
  _screen = screen;
  markDirty();
}

void CardputerUI::nextScreen() {
#if BOARD_IS_WAVESHARE_VISUAL_DISPLAY
  switch (_screen) {
    case CARD_SCREEN_STATUS: _screen = CARD_SCREEN_CHAT; break;
    case CARD_SCREEN_CHAT: _screen = CARD_SCREEN_MESH; break;
    case CARD_SCREEN_MESH:
    case CARD_SCREEN_PEERS: _screen = CARD_SCREEN_EMERGENCY; break;
    default: _screen = CARD_SCREEN_STATUS; break;
  }
#else
  _screen = static_cast<CardputerScreen>((static_cast<int>(_screen) + 1) % 4);
#endif
  markDirty();
}

void CardputerUI::previousScreen() {
#if BOARD_IS_WAVESHARE_VISUAL_DISPLAY
  switch (_screen) {
    case CARD_SCREEN_STATUS: _screen = CARD_SCREEN_EMERGENCY; break;
    case CARD_SCREEN_EMERGENCY: _screen = CARD_SCREEN_MESH; break;
    case CARD_SCREEN_MESH:
    case CARD_SCREEN_PEERS: _screen = CARD_SCREEN_CHAT; break;
    default: _screen = CARD_SCREEN_STATUS; break;
  }
#else
  _screen = static_cast<CardputerScreen>((static_cast<int>(_screen) + 3) % 4);
#endif
  markDirty();
}

CardputerScreen CardputerUI::getScreen() const {
  return _screen;
}

void CardputerUI::setInputBuffer(const char* text) {
  char nextInput[sizeof(_input)];
  strncpy(nextInput, text ? text : "", sizeof(nextInput) - 1);
  nextInput[sizeof(nextInput) - 1] = '\0';
  if (strcmp(_input, nextInput) == 0) {
    return;
  }
  strcpy(_input, nextInput);
  markDirty();
}

void CardputerUI::setEmergencyConfirm(bool active) {
  if (_confirmEmergency == active) {
    return;
  }
  _confirmEmergency = active;
  markDirty();
}

void CardputerUI::setEmergencyStatus(bool active, unsigned long secondsRemaining) {
  if (_emergencyActive == active && _emergencySecondsRemaining == secondsRemaining) {
    return;
  }
  _emergencyActive = active;
  _emergencySecondsRemaining = secondsRemaining;
  markDirty();
}

void CardputerUI::scrollCurrentPage(int delta) {
  if (delta == 0) {
    return;
  }

  if (_screen == CARD_SCREEN_CHAT) {
    _messageScrollBack += delta;
    if (_messageScrollBack < 0) {
      _messageScrollBack = 0;
    }
    if (_messageScrollBack >= _messageCount) {
      _messageScrollBack = max(0, _messageCount - 1);
    }
    markDirty();
    return;
  }

  if (_screen == CARD_SCREEN_MESH || _screen == CARD_SCREEN_PEERS) {
    _meshScroll += delta;
    if (_meshScroll < 0) {
      _meshScroll = 0;
    }
    int maxScroll = max(0, (int)meshMgr.getPeers().size() - 4);
    if (_meshScroll > maxScroll) {
      _meshScroll = maxScroll;
    }
    markDirty();
  }
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
  bool slowRefreshDue = now - _lastSlowRefresh >= SLOW_REFRESH_MS;
  if (!force && !_dirty) {
    if (!slowRefreshDue) {
      return;
    }
    _lastSlowRefresh = now;
    if (!liveStateChanged()) {
      return;
    }
  }
  if (force || slowRefreshDue) {
    _lastSlowRefresh = now;
  }

  switch (_screen) {
    case CARD_SCREEN_STATUS: drawStatus(); break;
    case CARD_SCREEN_CHAT: drawChat(); break;
    case CARD_SCREEN_PEERS: drawPeers(); break;
    case CARD_SCREEN_MENU: drawMenu(); break;
    case CARD_SCREEN_MESH: drawMesh(); break;
    case CARD_SCREEN_EMERGENCY: drawEmergency(); break;
  }
  updateLiveSignature();
  _dirty = false;
}

void CardputerUI::markDirty() {
  _dirty = true;
}

void CardputerUI::buildLiveSignature(char* out, size_t outSize) {
  if (!out || outSize == 0) {
    return;
  }
  String power = powerPort.headerLabel();
  snprintf(out, outSize, "%u|%u|%d|%d|%d|%lu|%s",
           static_cast<unsigned>(_screen),
           static_cast<unsigned>(_connectionState),
           meshMgr.getOnlinePeerCount(),
           bleUARTMgr.isConnected() ? 1 : 0,
           _emergencyActive ? 1 : 0,
           _emergencySecondsRemaining,
           power.c_str());
}

void CardputerUI::updateLiveSignature() {
  buildLiveSignature(_liveSignature, sizeof(_liveSignature));
}

bool CardputerUI::liveStateChanged() {
  char nextSignature[sizeof(_liveSignature)];
  buildLiveSignature(nextSignature, sizeof(nextSignature));
  if (strcmp(_liveSignature, nextSignature) == 0) {
    return false;
  }
  strcpy(_liveSignature, nextSignature);
  return true;
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

  String power = powerPort.headerLabel();
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
#if BOARD_IS_WAVESHARE_VISUAL_DISPLAY
  y = wrapPrint(8, y, lcd.width() - 16, lcd.height() - footerHeight() - 4, _statusLine1, COLOR_ACCENT, 2);
  wrapPrint(8, y + 4, lcd.width() - 16, lcd.height() - footerHeight() - 4, _statusLine2, COLOR_TEXT, 3);
  drawFooter("Swipe pages  Up/Down scroll  Boot next");
#else
  fitPrint(8, y, lcd.width() - 16, _statusLine1, COLOR_ACCENT);
  y += row;
  fitPrint(8, y, lcd.width() - 16, _statusLine2, COLOR_TEXT);

  drawFooter(BOARD_HAS_CARDPUTER_INPUT ? "Tab/Fn screens  M menu" :
             (BOARD_HAS_GPIO_BUTTONS ? "S1/S2 screen  S3 menu" : "Top/Bot move  Left back  Right enter"));
#endif
}

void CardputerUI::drawChat() {
  auto &lcd = displayPort.gfx();
#if BOARD_IS_WAVESHARE_VISUAL_DISPLAY
  drawHeader("Messages");

  const int bottomY = lcd.height() - footerHeight() - 4;
  int y = headerHeight() + 8;

  if (_messageCount == 0) {
    y = wrapPrint(8, y, lcd.width() - 16, bottomY, "No messages yet.", COLOR_TEXT, 2);
    wrapPrint(8, y + 6, lcd.width() - 16, bottomY, "Send from USB/BLE terminal or wait for mesh traffic.", COLOR_MUTED, 3);
    drawFooter("Swipe pages  Boot next");
    return;
  }

  int newest = _messageCount - 1 - _messageScrollBack;
  if (newest < 0) newest = 0;
  if (newest >= _messageCount) newest = _messageCount - 1;

  const int usableLines = max(1, (bottomY - y) / contentLineHeight());
  int start = newest;
  int usedLines = 0;
  while (start > 0 && usedLines < usableLines - 2) {
    usedLines += 1 + wrappedLineCount(_messages[start].text, lcd.width() - 44);
    if (usedLines >= usableLines - 1) {
      break;
    }
    start--;
  }

  if (start > 0) {
    fitPrint(8, y, lcd.width() - 16, "^ older messages above", COLOR_MUTED);
    y += contentLineHeight();
  }

  for (int i = start; i <= newest && y < bottomY; i++) {
    const CardputerMessage& msg = _messages[i];
    uint16_t markerColor = msg.emergency ? COLOR_DANGER : (msg.outgoing ? COLOR_ACCENT : COLOR_GOOD);
    lcd.setTextSize(textScale());
    lcd.setTextColor(markerColor, COLOR_BG);
    lcd.setCursor(8, y);
    lcd.print(msg.outgoing ? ">" : "<");
    y = wrapPrint(32, y, lcd.width() - 40, bottomY, msg.text, msg.emergency ? COLOR_DANGER : COLOR_TEXT, 4);
    y += 6;
  }

  if (_messageScrollBack > 0) {
    fitPrint(8, bottomY - contentLineHeight(), lcd.width() - 16, "v swipe down/newer", COLOR_MUTED);
  }
  drawFooter("Messages  Swipe up/down history");
#else
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

  drawFooter(BOARD_HAS_CARDPUTER_INPUT ? "Enter send  Bksp del" :
             (BOARD_HAS_GPIO_BUTTONS ? "S1/S2 screen  S3 menu" : "Use terminal or quick-send menu"));
#endif
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

  drawFooter(BOARD_IS_WAVESHARE_VISUAL_DISPLAY ? "Swipe pages  Up/Down peers" :
             (BOARD_HAS_CARDPUTER_INPUT ? "Tab/Fn next  M menu" :
              (BOARD_HAS_GPIO_BUTTONS ? "S1/S2 screen  S3 menu" : "Tap center menu")));
}

void CardputerUI::drawMenu() {
#if BOARD_IS_WAVESHARE_VISUAL_DISPLAY
  drawStatus();
#else
  static const char* const fallbackItems[] = {
    "Status", "Messages", "Mesh", "Settings", "System", "Emergency"
  };
  drawMenuList("Menu", fallbackItems, 6, 0, 0,
               BOARD_HAS_GPIO_BUTTONS ? "S1/S2 move  S3 select" : "Move/Enter or touch zones");
#endif
}

void CardputerUI::drawMesh() {
  auto &lcd = displayPort.gfx();
  drawHeader("Mesh");

  const int bottomY = lcd.height() - footerHeight() - 4;
  int y = headerHeight() + 8;

  char line[64];
  snprintf(line, sizeof(line), "Mesh: %s  Peers: %d", meshMgr.isRunning() ? "ON" : "OFF", meshMgr.getOnlinePeerCount());
  y = wrapPrint(8, y, lcd.width() - 16, bottomY, line, COLOR_ACCENT, 1);
  snprintf(line, sizeof(line), "Sent:%lu  Rx:%lu  Relay:%lu",
           (unsigned long)meshMgr.getMessagesSent(),
           (unsigned long)meshMgr.getMessagesReceived(),
           (unsigned long)meshMgr.getMessagesRelayed());
  y = wrapPrint(8, y + 4, lcd.width() - 16, bottomY, line, COLOR_TEXT, 1);
  snprintf(line, sizeof(line), "Store queue: %d/%d", meshMgr.getStoredMessageCount(), MESH_STORE_QUEUE_SIZE);
  y = wrapPrint(8, y + 4, lcd.width() - 16, bottomY, line, COLOR_TEXT, 1);
  y += 8;

  auto peers = meshMgr.getPeers();
  if (peers.empty()) {
    y = wrapPrint(8, y, lcd.width() - 16, bottomY, "No mesh peers discovered yet.", COLOR_TEXT, 2);
    wrapPrint(8, y + 6, lcd.width() - 16, bottomY, "Keep nodes on channel 1 and wait for heartbeat.", COLOR_MUTED, 3);
  } else {
    int shown = 0;
    for (int i = _meshScroll; i < (int)peers.size() && y < bottomY - contentLineHeight(); i++) {
      const auto& peer = peers[i];
      if (strlen(peer.unitName) > 0) {
        snprintf(line, sizeof(line), "%s  %ddBm  hop %d", peer.unitName, peer.rssi, peer.hopDistance);
      } else {
        snprintf(line, sizeof(line), "%02X:%02X:%02X  %ddBm  hop %d",
                 peer.mac[3], peer.mac[4], peer.mac[5], peer.rssi, peer.hopDistance);
      }
      y = wrapPrint(8, y, lcd.width() - 16, bottomY, line, peer.isOnline ? COLOR_GOOD : COLOR_MUTED, 1);
      y += 4;
      shown++;
      if (shown >= 8) {
        break;
      }
    }
  }

  drawFooter("Mesh  Swipe up/down peers");
}

void CardputerUI::drawEmergency() {
  auto &lcd = displayPort.gfx();
  drawHeader("SOS");

  const int bottomY = lcd.height() - footerHeight() - 4;
  int y = headerHeight() + 10;

  if (_emergencyActive) {
    char line[48];
    snprintf(line, sizeof(line), "ACTIVE: auto-cancel in %lus", _emergencySecondsRemaining);
    y = wrapPrint(8, y, lcd.width() - 16, bottomY, line, COLOR_DANGER, 2);
    y = wrapPrint(8, y + 10, lcd.width() - 16, bottomY, "Use terminal command 'cancel' to stop early.", COLOR_TEXT, 3);
  } else {
    y = wrapPrint(8, y, lcd.width() - 16, bottomY, "Emergency broadcast is idle.", COLOR_GOOD, 2);
    y = wrapPrint(8, y + 10, lcd.width() - 16, bottomY, "To send SOS, use the USB/BLE terminal command:", COLOR_TEXT, 3);
    wrapPrint(8, y + 10, lcd.width() - 16, bottomY, "emergency", COLOR_DANGER, 1);
  }

  if (_confirmEmergency) {
    wrapPrint(8, bottomY - contentLineHeight() * 2, lcd.width() - 16, bottomY, "Confirmation armed on non-touch input.", COLOR_WARN, 2);
  }

  drawFooter("SOS display only  Terminal sends");
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
    case CARD_SCREEN_MESH: return "MESH";
    case CARD_SCREEN_EMERGENCY: return "SOS";
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

int CardputerUI::wrapPrint(int x, int y, int w, int maxY, const char* text, uint16_t color, int maxLines) {
  auto &lcd = displayPort.gfx();
  lcd.setTextSize(textScale());
  lcd.setTextColor(color, COLOR_BG);

  const int maxChars = max(1, w / (6 * textScale()));
  const int lineH = contentLineHeight();
  const char* src = text ? text : "";
  int printed = 0;

  while (*src && y + lineH <= maxY && (maxLines <= 0 || printed < maxLines)) {
    while (*src == ' ') {
      src++;
    }

    char line[96];
    int len = 0;
    int lastSpace = -1;
    const char* scan = src;
    while (*scan && *scan != '\n' && len < maxChars && len < (int)sizeof(line) - 1) {
      line[len] = *scan;
      if (*scan == ' ') {
        lastSpace = len;
      }
      len++;
      scan++;
    }

    bool forcedBreak = *scan && *scan != '\n';
    if (forcedBreak && lastSpace > 0) {
      len = lastSpace;
      scan = src + lastSpace + 1;
    }
    if (*scan == '\n') {
      scan++;
    }

    line[len] = '\0';
    lcd.setCursor(x, y);
    lcd.print(line);
    y += lineH;
    printed++;
    src = scan;
  }

  return y;
}

int CardputerUI::wrappedLineCount(const char* text, int w) const {
  const int maxChars = max(1, w / (6 * textScale()));
  const char* src = text ? text : "";
  int lines = 0;

  while (*src) {
    while (*src == ' ') {
      src++;
    }
    int len = 0;
    int lastSpace = -1;
    const char* scan = src;
    while (*scan && *scan != '\n' && len < maxChars) {
      if (*scan == ' ') {
        lastSpace = len;
      }
      len++;
      scan++;
    }
    if (*scan && *scan != '\n' && lastSpace > 0) {
      scan = src + lastSpace + 1;
    } else if (*scan == '\n') {
      scan++;
    }
    src = scan;
    lines++;
  }

  return max(1, lines);
}

int CardputerUI::contentLineHeight() const {
  return 8 * textScale() + (BOARD_DISPLAY_IS_SH8601 ? 6 : 3);
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

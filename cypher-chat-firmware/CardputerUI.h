#pragma once

#include "Config.h"
#include "DisplayPort.h"
#include "StateManager.h"

enum CardputerScreen {
  CARD_SCREEN_STATUS,
  CARD_SCREEN_CHAT,
  CARD_SCREEN_PEERS,
  CARD_SCREEN_MENU,
  CARD_SCREEN_MESH,
  CARD_SCREEN_EMERGENCY
};

struct CardputerMessage {
  char text[MAX_MESSAGE_SIZE];
  unsigned long timestamp;
  bool outgoing;
  bool emergency;
};

class CardputerUI {
public:
  CardputerUI();

  void begin();
  void updateStatus(const char* line1, const char* line2 = "");
  void addMessage(const char* text, bool outgoing, bool emergency = false);
  void clearMessages();
  void setConnectionState(ConnectionState state);
  void setScreen(CardputerScreen screen);
  void nextScreen();
  void previousScreen();
  CardputerScreen getScreen() const;
  void setInputBuffer(const char* text);
  void setEmergencyConfirm(bool active);
  void setEmergencyStatus(bool active, unsigned long secondsRemaining = 0);
  void scrollCurrentPage(int delta);
  int messageCount() const;
  bool getMessageLine(int visibleIndex, char* out, size_t outSize) const;
  void drawMenuList(const char* title, const char* const* items, int itemCount, int selected, int scroll, const char* hint);
  void drawInfoList(const char* title, const char* const* lines, int lineCount, int scroll, const char* hint);
  void drawEditView(const char* title, const char* value, bool masked, const char* hint);
  void refresh(bool force = false);

private:
  CardputerScreen _screen;
  CardputerMessage _messages[CARDPUTER_HISTORY_SIZE];
  int _messageCount;
  char _statusLine1[40];
  char _statusLine2[56];
  char _input[CARDPUTER_INPUT_MAX];
  ConnectionState _connectionState;
  bool _confirmEmergency;
  bool _emergencyActive;
  unsigned long _emergencySecondsRemaining;
  int _messageScrollBack;
  int _meshScroll;
  unsigned long _lastSlowRefresh;
  char _liveSignature[72];
  bool _dirty;

  void markDirty();
  void buildLiveSignature(char* out, size_t outSize);
  void updateLiveSignature();
  bool liveStateChanged();
  void drawHeader(const char* title);
  void drawStatus();
  void drawChat();
  void drawPeers();
  void drawMenu();
  void drawMesh();
  void drawEmergency();
  void drawFooter(const char* hint);
  void drawMessageLine(int y, const CardputerMessage& msg);
  const char* stateLabel() const;
  const char* screenLabel() const;
  void fitPrint(int x, int y, int w, const char* text, uint16_t color);
  int wrapPrint(int x, int y, int w, int maxY, const char* text, uint16_t color, int maxLines = 0);
  int wrappedLineCount(const char* text, int w) const;
  int contentLineHeight() const;
  uint8_t textScale() const;
  int headerHeight() const;
  int footerHeight() const;
  int rowHeight() const;
};

extern CardputerUI cardputerUI;
extern DisplayPort displayPort;

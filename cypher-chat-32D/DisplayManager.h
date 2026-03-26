#pragma once

#include "Config.h"
#include "StateManager.h"

// Display screens
enum DisplayScreen {
  SCREEN_STATUS,    // Current status + last message
  SCREEN_MESSAGES,  // Recent 3 messages with indicators
  SCREEN_HISTORY    // Scrollable 10-message buffer
};

// Message structure for history
struct Message {
  char text[MAX_MESSAGE_SIZE];
  unsigned long timestamp;
  bool isOutgoing;
  bool isDelivered;
  int retryCount;
};

#define MAX_HISTORY_SIZE 10
#define STATUS_BAR_HEIGHT 12
#define FOOTER_HEIGHT 9
#define MESSAGE_DISPLAY_COUNT 2

class DisplayManager {
public:
  DisplayManager();

  // Initialize display manager
  void begin();

  // Update status information
  void updateStatus(const char* line1, const char* line2 = "");

  // Add message to history
  void addMessage(const char* text, bool isOutgoing);

  // Mark message as delivered
  void markDelivered(const char* text);

  // Update message retry count
  void updateRetryCount(const char* text, int retryCount);

  // Screen management
  void setScreen(DisplayScreen screen);
  DisplayScreen getCurrentScreen();

  // Scroll history
  void scrollUp();
  void scrollDown();

  // Update connection state for status bar
  void setConnectionState(ConnectionState state);
  void setRetryCount(int count);
  void setMessageCount(int count);

  // Refresh display (call from main loop)
  void refresh();

private:
  DisplayScreen currentScreen;
  Message messageHistory[MAX_HISTORY_SIZE];
  int historyCount;
  int historyScrollOffset;

  ConnectionState connectionState;
  int retryCount;
  int messageCount;

  char statusLine1[32];
  char statusLine2[32];

  unsigned long lastRefresh;
  static const unsigned long REFRESH_INTERVAL = 50; // 20 FPS

  // Drawing functions
  void drawStatusBar();
  void drawStatusScreen();
  void drawMessagesScreen();
  void drawHistoryScreen();
  void drawFooterHints();
  void drawWrappedLastMessage(int yStart, int maxLines);

  // Helper to get connection state abbreviation
  const char* getStateAbbrev();

  // Helper to format timestamp
  void formatTimestamp(unsigned long timestamp, char* buffer, size_t bufferSize);
};

extern DisplayManager displayMgr;

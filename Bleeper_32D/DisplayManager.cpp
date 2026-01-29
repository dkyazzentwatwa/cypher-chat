#include "DisplayManager.h"
#include "TerminalManager.h"

DisplayManager displayMgr;
extern TerminalManager terminalMgr;

DisplayManager::DisplayManager()
  : currentScreen(SCREEN_STATUS),
    historyCount(0),
    historyScrollOffset(0),
    connectionState(STATE_IDLE),
    retryCount(0),
    messageCount(0),
    lastRefresh(0) {
  memset(messageHistory, 0, sizeof(messageHistory));
  memset(statusLine1, 0, sizeof(statusLine1));
  memset(statusLine2, 0, sizeof(statusLine2));
}

void DisplayManager::begin() {
  // Display is already initialized in main sketch
  currentScreen = SCREEN_STATUS;
  historyCount = 0;
  historyScrollOffset = 0;
  updateStatus("Display Ready", "");
}

void DisplayManager::updateStatus(const char* line1, const char* line2) {
  strncpy(statusLine1, line1, sizeof(statusLine1) - 1);
  statusLine1[sizeof(statusLine1) - 1] = '\0';

  strncpy(statusLine2, line2, sizeof(statusLine2) - 1);
  statusLine2[sizeof(statusLine2) - 1] = '\0';
}

void DisplayManager::addMessage(const char* text, bool isOutgoing) {
  // Shift history if full
  if (historyCount >= MAX_HISTORY_SIZE) {
    for (int i = 0; i < MAX_HISTORY_SIZE - 1; i++) {
      messageHistory[i] = messageHistory[i + 1];
    }
    historyCount = MAX_HISTORY_SIZE - 1;
  }

  // Add new message
  Message& msg = messageHistory[historyCount];
  strncpy(msg.text, text, MAX_MESSAGE_SIZE - 1);
  msg.text[MAX_MESSAGE_SIZE - 1] = '\0';
  msg.timestamp = millis();
  msg.isOutgoing = isOutgoing;
  msg.isDelivered = false;
  msg.retryCount = 0;

  historyCount++;
  messageCount = historyCount;

  // Log to terminal
  terminalMgr.logMessage(text, isOutgoing, false);
}

void DisplayManager::markDelivered(const char* text) {
  for (int i = historyCount - 1; i >= 0; i--) {
    if (strcmp(messageHistory[i].text, text) == 0) {
      messageHistory[i].isDelivered = true;

      // Log to terminal
      terminalMgr.logMessage(text, true, true);
      break;
    }
  }
}

void DisplayManager::updateRetryCount(const char* text, int retryCount) {
  for (int i = historyCount - 1; i >= 0; i--) {
    if (strcmp(messageHistory[i].text, text) == 0) {
      messageHistory[i].retryCount = retryCount;
      break;
    }
  }
}

void DisplayManager::setScreen(DisplayScreen screen) {
  currentScreen = screen;
  historyScrollOffset = 0; // Reset scroll when changing screens
}

DisplayScreen DisplayManager::getCurrentScreen() {
  return currentScreen;
}

void DisplayManager::scrollUp() {
  if (currentScreen == SCREEN_HISTORY && historyScrollOffset > 0) {
    historyScrollOffset--;
  }
}

void DisplayManager::scrollDown() {
  if (currentScreen == SCREEN_HISTORY) {
    int maxScroll = historyCount - MESSAGE_DISPLAY_COUNT;
    if (historyScrollOffset < maxScroll && maxScroll > 0) {
      historyScrollOffset++;
    }
  }
}

void DisplayManager::setConnectionState(ConnectionState state) {
  connectionState = state;
}

void DisplayManager::setRetryCount(int count) {
  retryCount = count;
}

void DisplayManager::setMessageCount(int count) {
  messageCount = count;
}

void DisplayManager::refresh() {
  unsigned long now = millis();
  if (now - lastRefresh < REFRESH_INTERVAL) {
    return; // Throttle refresh rate to 10 FPS
  }
  lastRefresh = now;

  display.clearDisplay();

  // Always draw status bar
  drawStatusBar();

  // Draw current screen content below status bar
  switch (currentScreen) {
    case SCREEN_STATUS:
      drawStatusScreen();
      break;
    case SCREEN_MESSAGES:
      drawMessagesScreen();
      break;
    case SCREEN_HISTORY:
      drawHistoryScreen();
      break;
  }

  display.display();
}

void DisplayManager::drawStatusBar() {
  // Draw status bar background
  display.fillRect(0, 0, OLED_WIDTH, STATUS_BAR_HEIGHT, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 1);

  // Connection state abbreviation
  display.print(getStateAbbrev());

  // Retry counter if > 0
  if (retryCount > 0) {
    display.print(" R:");
    display.print(retryCount);
  }

  // Message count
  display.print(" M:");
  display.print(messageCount);

  // Power indicator (wall-powered)
  display.setCursor(OLED_WIDTH - 24, 1);
  display.print("PWR");

  // Reset text color for content area
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
}

void DisplayManager::drawStatusScreen() {
  display.setTextSize(1);

  // Line 1 (below status bar)
  display.setCursor(0, STATUS_BAR_HEIGHT + 2);
  display.print(statusLine1);

  // Line 2
  display.setCursor(0, STATUS_BAR_HEIGHT + 12);
  display.print(statusLine2);

  // Last message (if any)
  if (historyCount > 0) {
    Message& lastMsg = messageHistory[historyCount - 1];

    display.setCursor(0, STATUS_BAR_HEIGHT + 24);
    display.print("Last msg:");

    display.setCursor(0, STATUS_BAR_HEIGHT + 34);

    // Direction indicator
    if (lastMsg.isOutgoing) {
      display.print(lastMsg.isDelivered ? "> " : ">>");
    } else {
      display.print("< ");
    }

    // Truncate message to fit
    char truncated[20];
    strncpy(truncated, lastMsg.text, 19);
    truncated[19] = '\0';
    display.print(truncated);

    // Timestamp
    display.setCursor(0, STATUS_BAR_HEIGHT + 44);
    char timeStr[16];
    formatTimestamp(lastMsg.timestamp, timeStr, sizeof(timeStr));
    display.print(timeStr);
  }
}

void DisplayManager::drawMessagesScreen() {
  display.setTextSize(1);

  // Display last 3 messages
  int startIdx = (historyCount > MESSAGE_DISPLAY_COUNT) ? (historyCount - MESSAGE_DISPLAY_COUNT) : 0;
  int y = STATUS_BAR_HEIGHT + 2;

  for (int i = startIdx; i < historyCount; i++) {
    Message& msg = messageHistory[i];

    display.setCursor(0, y);

    // Direction and delivery indicator
    if (msg.isOutgoing) {
      if (msg.isDelivered) {
        display.print("> ");
      } else if (msg.retryCount > 0) {
        display.print("X ");
      } else {
        display.print(">>");
      }
    } else {
      display.print("< ");
    }

    // Message text (truncated)
    char truncated[20];
    strncpy(truncated, msg.text, 19);
    truncated[19] = '\0';
    display.print(truncated);

    y += 18; // Space for next message
  }
}

void DisplayManager::drawHistoryScreen() {
  display.setTextSize(1);

  // Display scrollable history
  int y = STATUS_BAR_HEIGHT + 2;
  int displayCount = min(MESSAGE_DISPLAY_COUNT, historyCount);

  for (int i = 0; i < displayCount; i++) {
    int idx = historyScrollOffset + i;
    if (idx >= historyCount) break;

    Message& msg = messageHistory[idx];

    display.setCursor(0, y);

    // Direction indicator
    display.print(msg.isOutgoing ? ">" : "<");
    display.print(" ");

    // Message text (truncated)
    char truncated[18];
    strncpy(truncated, msg.text, 17);
    truncated[17] = '\0';
    display.print(truncated);

    y += 18;
  }

  // Scroll indicator
  if (historyCount > MESSAGE_DISPLAY_COUNT) {
    display.setCursor(OLED_WIDTH - 12, OLED_HEIGHT - 8);
    display.print(historyScrollOffset + 1);
    display.print("/");
    display.print(historyCount);
  }
}

const char* DisplayManager::getStateAbbrev() {
  switch (connectionState) {
    case STATE_IDLE:        return "IDL";
    case STATE_SCANNING:    return "SCN";
    case STATE_CONNECTING:  return "CON";
    case STATE_CONNECTED:   return "CON";
    case STATE_DISCONNECTED:return "DIS";
    case STATE_ERROR:       return "ERR";
    default:                return "???";
  }
}

void DisplayManager::formatTimestamp(unsigned long timestamp, char* buffer, size_t bufferSize) {
  unsigned long seconds = (millis() - timestamp) / 1000;

  if (seconds < 60) {
    snprintf(buffer, bufferSize, "%lus ago", seconds);
  } else if (seconds < 3600) {
    snprintf(buffer, bufferSize, "%lum ago", seconds / 60);
  } else {
    snprintf(buffer, bufferSize, "%luh ago", seconds / 3600);
  }
}

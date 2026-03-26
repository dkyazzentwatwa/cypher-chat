#include "DisplayManager.h"
#include "TerminalManager.h"

DisplayManager displayMgr;
extern TerminalManager terminalMgr;

static const char* screenLabel(DisplayScreen screen) {
  switch (screen) {
    case SCREEN_STATUS: return "STAT";
    case SCREEN_MESSAGES: return "MSGS";
    case SCREEN_HISTORY: return "HIST";
    default: return "----";
  }
}

static const int kCharsPerLine = 21;

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
  display.setTextWrap(false);
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
  (void)screen;
  currentScreen = SCREEN_STATUS;  // Single-screen UI
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

  // Single-screen mode for better readability on 0.96" OLEDs.
  drawStatusScreen();

  drawFooterHints();

  display.display();
}

void DisplayManager::drawStatusBar() {
  // Draw status bar background
  display.fillRect(0, 0, OLED_WIDTH, STATUS_BAR_HEIGHT, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 2);

  // Connection state abbreviation
  display.print(getStateAbbrev());
  display.print(" MSG");

  char right[16];
  snprintf(right, sizeof(right), "M%02d R%02d", messageCount, retryCount);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(right, 0, 0, &x1, &y1, &w, &h);
  int x = OLED_WIDTH - (int)w - 1;
  if (x < 52) x = 52;
  display.setCursor(x, 2);
  display.print(right);

  // Reset text color for content area
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
}

void DisplayManager::drawStatusScreen() {
  display.setTextSize(1);

  // Header lines below status bar
  display.setCursor(0, STATUS_BAR_HEIGHT + 1);
  display.print(statusLine1);

  display.setCursor(0, STATUS_BAR_HEIGHT + 9);
  display.print(statusLine2);

  display.setCursor(0, STATUS_BAR_HEIGHT + 17);
  display.print("Last:");
  drawWrappedLastMessage(STATUS_BAR_HEIGHT + 25, 3);
}

void DisplayManager::drawMessagesScreen() {
  display.setTextSize(1);

  // Display recent messages
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
    char truncated[19];
    strncpy(truncated, msg.text, 18);
    truncated[18] = '\0';
    display.print(truncated);

    y += 20;
  }

  if (historyCount == 0) {
    display.setCursor(0, STATUS_BAR_HEIGHT + 22);
    display.print("No messages yet");
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
    char truncated[19];
    strncpy(truncated, msg.text, 18);
    truncated[18] = '\0';
    display.print(truncated);

    y += 20;
  }

  // Scroll indicator
  if (historyCount > MESSAGE_DISPLAY_COUNT) {
    display.setCursor(OLED_WIDTH - 20, OLED_HEIGHT - FOOTER_HEIGHT - 1);
    display.print(historyScrollOffset + 1);
    display.print("/");
    display.print(historyCount);
  } else if (historyCount == 0) {
    display.setCursor(0, STATUS_BAR_HEIGHT + 22);
    display.print("History empty");
  }
}

void DisplayManager::drawFooterHints() {
  display.fillRect(0, OLED_HEIGHT - FOOTER_HEIGHT, OLED_WIDTH, FOOTER_HEIGHT, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, OLED_HEIGHT - FOOTER_HEIGHT + 0);
  display.print("K1 ACK  K2 !  K3 HELP");
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
}

void DisplayManager::drawWrappedLastMessage(int yStart, int maxLines) {
  if (historyCount <= 0) {
    display.setCursor(0, yStart);
    display.print("No messages yet");
    return;
  }

  const Message& lastMsg = messageHistory[historyCount - 1];
  char text[MAX_MESSAGE_SIZE];
  strncpy(text, lastMsg.text, sizeof(text) - 1);
  text[sizeof(text) - 1] = '\0';

  char prefix[4];
  if (lastMsg.isOutgoing) {
    snprintf(prefix, sizeof(prefix), "%s", lastMsg.isDelivered ? "> " : ">>");
  } else {
    snprintf(prefix, sizeof(prefix), "< ");
  }

  int line = 0;
  int y = yStart;
  int start = 0;
  int len = strlen(text);

  while (line < maxLines && start < len) {
    int remain = len - start;
    int capacity = (line == 0) ? (kCharsPerLine - (int)strlen(prefix)) : kCharsPerLine;
    if (capacity < 6) capacity = 6;
    int take = (remain > capacity) ? capacity : remain;

    if (take < remain) {
      int split = take;
      while (split > 6 && text[start + split] != ' ') {
        split--;
      }
      if (split > 6) {
        take = split;
      }
    }

    char lineBuf[24];
    memset(lineBuf, 0, sizeof(lineBuf));
    strncpy(lineBuf, &text[start], take);
    lineBuf[take] = '\0';

    display.setCursor(0, y);
    if (line == 0) {
      display.print(prefix);
    }
    display.print(lineBuf);

    start += take;
    while (text[start] == ' ') start++;
    line++;
    y += 8;
  }
}

const char* DisplayManager::getStateAbbrev() {
  switch (connectionState) {
    case STATE_IDLE:        return "IDL";
    case STATE_SCANNING:    return "SCN";
    case STATE_CONNECTING:  return "CTG";
    case STATE_CONNECTED:   return "ONL";
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

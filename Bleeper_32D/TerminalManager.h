#pragma once

#include <Arduino.h>
#include "Config.h"
#include "StateManager.h"
#include "ButtonManager.h"

/**
 * TerminalManager - Serial/Terminal interface for headless operation
 *
 * Provides dual-mode terminal interface (menu + command-line) for full device
 * control without OLED display. Supports real-time monitoring, command execution,
 * and event logging with ANSI color support.
 */

enum TerminalMode {
  TERM_QUIET,       // Minimal output (errors only)
  TERM_NORMAL,      // Status updates + messages (default)
  TERM_VERBOSE,     // Full debug output
  TERM_MONITOR      // Real-time monitoring (live updates at 1 Hz)
};

enum MenuState {
  MENU_NONE,        // Command mode
  MENU_MAIN,
  MENU_SEND,
  MENU_CONFIG,
  MENU_CONFIRM
};

class TerminalManager {
private:
  TerminalMode mode;
  bool enabled;
  unsigned long lastUpdate;
  static const unsigned long UPDATE_INTERVAL_MS = 1000;  // 1 Hz for monitor mode

  char inputBuffer[128];
  size_t inputPos;
  MenuState menuState;
  bool ansiEnabled;
  bool menuOnStartup;

  // ANSI color codes
  const char* COLOR_RESET = "\033[0m";
  const char* COLOR_RED = "\033[31m";
  const char* COLOR_GREEN = "\033[32m";
  const char* COLOR_YELLOW = "\033[33m";
  const char* COLOR_BLUE = "\033[34m";
  const char* COLOR_CYAN = "\033[36m";
  const char* COLOR_BOLD = "\033[1m";

  // Input processing (non-blocking)
  void processCommand(const char* cmd);
  void parseCommand(const char* cmd, char* verb, char* args);
  void executeCommand(const char* verb, const char* args);
  void handleMenuInput(char input);

  // Command handlers
  void cmdSend(const char* args);
  void cmdMsg(const char* args);
  void cmdEmergency();
  void cmdCancel();
  void cmdStatus();
  void cmdMessages();
  void cmdConfig();
  void cmdName(const char* newName);
  void cmdPasskey(const char* newKey);
  void cmdMode(const char* modeStr);
  void cmdRestart();
  void cmdUptime();
  void cmdMemory();
  void cmdHelp();
  void cmdClear();
  void cmdMenu();

  // Display functions
  void displayMainMenu();
  void displaySendMenu();
  void displayConfigMenu();
  void displayMonitorDashboard();
  void printColored(const char* text, const char* color);
  void printBanner();
  void printPrompt();

  // Utility functions
  const char* getStateString(ConnectionState state);
  void formatUptime(char* buffer, size_t bufSize);
  void detectANSISupport();

  // Configuration persistence
  void saveConfig();
  void loadConfig();

public:
  void begin();
  void setMode(TerminalMode newMode);
  bool isMenuMode();
  void toggleMode();

  // Non-blocking I/O (called from main loop)
  void poll();    // Check for serial input
  void update();  // Refresh display in monitor mode

  // Status updates (non-blocking)
  void printStatus();
  void printMessageHistory();
  void printConfiguration();

  // Event logging (called from other modules)
  void logEvent(const char* level, const char* message);
  void logConnection(ConnectionState state);
  void logMessage(const char* msg, bool isOutgoing, bool delivered);
  void logHMAC(bool verified, const char* message);
  void logEmergency(bool active);
  void logButtonPress(int buttonIndex, ButtonEvent event);
};

// Global instance
extern TerminalManager terminalMgr;

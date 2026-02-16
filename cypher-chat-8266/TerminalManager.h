#pragma once

#include <Arduino.h>
#include "Config_8266.h"
#include "StateManager.h"

/**
 * TerminalManager - Serial terminal interface for ESP8266
 *
 * USB Serial only (no BLE on ESP8266).
 * Same command set and UI as the ESP32 basic version.
 */

// Terminal configuration - reduced for ESP8266 RAM
#ifndef TERM_HISTORY_SIZE
#define TERM_HISTORY_SIZE 5
#endif
#define TERM_MAX_CMD_LEN 128
#define TERM_NUM_COMMANDS 24

enum TerminalMode {
  TERM_QUIET,
  TERM_NORMAL,
  TERM_VERBOSE,
  TERM_MONITOR
};

enum MenuState {
  MENU_NONE,
  MENU_MAIN,
  MENU_SEND,
  MENU_CONFIG,
  MENU_MESH,
  MENU_CONFIRM
};

enum CmdCategory {
  CAT_MESSAGE,
  CAT_MESH,
  CAT_DISPLAY,
  CAT_CONFIG,
  CAT_SYSTEM
};

struct CommandDesc {
  const char* name;
  const char* alias;
  const char* args;
  const char* description;
  CmdCategory category;
};

enum InputSource {
  INPUT_SOURCE_USB
};

enum ButtonEvent {
  BUTTON_NONE,
  BUTTON_PRESS,
  BUTTON_LONG_PRESS,
  BUTTON_RELEASE
};

class TerminalManager {
private:
  TerminalMode mode;
  bool enabled;
  unsigned long lastUpdate;
  static const unsigned long UPDATE_INTERVAL_MS = 1000;

  char inputBuffer[TERM_MAX_CMD_LEN];
  size_t inputPos;
  MenuState menuState;
  bool ansiEnabled;
  bool menuOnStartup;

  // Command history
  char history[TERM_HISTORY_SIZE][TERM_MAX_CMD_LEN];
  int historyCount;
  int historyIndex;
  bool browsingHistory;

  // Escape sequence parsing
  int escapeState;
  char escapeBuffer[8];
  int escapePos;

  // ANSI color codes
  const char* COLOR_RESET = "\033[0m";
  const char* COLOR_RED = "\033[31m";
  const char* COLOR_GREEN = "\033[32m";
  const char* COLOR_YELLOW = "\033[33m";
  const char* COLOR_BLUE = "\033[34m";
  const char* COLOR_MAGENTA = "\033[35m";
  const char* COLOR_CYAN = "\033[36m";
  const char* COLOR_WHITE = "\033[37m";
  const char* COLOR_BOLD = "\033[1m";
  const char* COLOR_DIM = "\033[2m";

  // Command registry
  static const CommandDesc commands[];
  static const int numCommands;

  void processInput(char c);
  void processCommand(const char* cmd);
  void parseCommand(const char* cmd, char* verb, char* args);
  void executeCommand(const char* verb, const char* args);
  void handleMenuInput(char input);
  void handleEscapeSequence(char c);
  void handleSpecialKey(const char* seq);

  void addToHistory(const char* cmd);
  void navigateHistory(int direction);
  void clearInputLine();

  void handleTabCompletion();
  int findMatchingCommands(const char* prefix, const CommandDesc** matches, int maxMatches);
  const char* resolveAlias(const char* cmd);

  // Command handlers
  void cmdSend(const char* args);
  void cmdEmergency();
  void cmdCancel();
  void cmdMesh();
  void cmdPeers();
  void cmdMeshSend(const char* args);
  void cmdMeshBroadcast(const char* args);
  void cmdGPS();
  void cmdStatus();
  void cmdMessages();
  void cmdConfig();
  void cmdName(const char* newName);
  void cmdPassphrase(const char* newPhrase);
  void cmdNewKey(const char* args);
  void cmdMode(const char* modeStr);
  void cmdAnsi(const char* args);
  void cmdRestart();
  void cmdUptime();
  void cmdMemory();
  void cmdHelp(const char* args);
  void cmdClear();
  void cmdMenu();
  void cmdHistory();
  void cmdVersion();

  void displayMainMenu();
  void displaySendMenu();
  void displayConfigMenu();
  void displayMeshMenu();
  void displayMonitorDashboard();
  void printColored(const char* text, const char* color);
  void printBanner();
  void printPrompt();
  void printCategoryHelp(CmdCategory cat, const char* title);
  void printCommandHelp(const CommandDesc* cmd);

  const char* getStateString(ConnectionState state);
  void formatUptime(char* buffer, size_t bufSize);

  void saveConfig();
  void loadConfig();

public:
  void begin();
  void setMode(TerminalMode newMode);
  bool isMenuMode();
  void toggleMode();

  void poll();
  void update();

  void printStatus();
  void printMessageHistory();
  void printConfiguration();

  void logEvent(const char* level, const char* message);
  void logConnection(ConnectionState state);
  void logMessage(const char* msg, bool isOutgoing, bool delivered);
  void logHMAC(bool verified, const char* message);
  void logEmergency(bool active);
  void logButtonPress(int buttonIndex, ButtonEvent event);
};

// Global instance
extern TerminalManager terminalMgr;

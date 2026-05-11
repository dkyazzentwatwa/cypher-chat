#pragma once

#include <Arduino.h>
#include "Config_S3.h"
#include "StateManager.h"

/**
 * TerminalManager - Serial/Terminal interface for headless operation
 *
 * Provides dual-mode terminal interface (menu + command-line) for full device
 * control without OLED display. Supports real-time monitoring, command execution,
 * and event logging with ANSI color support.
 *
 * Features:
 * - Command history (up/down arrows)
 * - Tab completion
 * - Command aliases
 * - Categorized help system
 * - ANSI color output
 */

// Terminal configuration
#define TERM_HISTORY_SIZE 10      // Number of commands to remember
#define TERM_MAX_CMD_LEN 128      // Max command length
#define TERM_NUM_COMMANDS 25      // Total registered commands

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
  MENU_MESH,
  MENU_CONFIRM
};

// Command category for help organization
enum CmdCategory {
  CAT_MESSAGE,      // Message sending commands
  CAT_MESH,         // Mesh networking commands
  CAT_DISPLAY,      // Display/status commands
  CAT_CONFIG,       // Configuration commands
  CAT_SYSTEM        // System commands
};

// Command descriptor for registration
struct CommandDesc {
  const char* name;           // Primary command name
  const char* alias;          // Short alias (or nullptr)
  const char* args;           // Argument description (or nullptr)
  const char* description;    // Help text
  CmdCategory category;       // Category for grouping
};

// Input source tracking for dual Serial + Bluetooth input
enum InputSource {
  INPUT_SOURCE_USB,   // USB Serial input
  INPUT_SOURCE_BT     // Bluetooth Serial input
};

// ButtonEvent enum (defined inline since we don't have ButtonManager in Cardputer version)
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
  static const unsigned long UPDATE_INTERVAL_MS = 1000;  // 1 Hz for monitor mode

  char inputBuffer[TERM_MAX_CMD_LEN];
  size_t inputPos;
  MenuState menuState;
  bool ansiEnabled;
  bool menuOnStartup;

  // Command history
  char history[TERM_HISTORY_SIZE][TERM_MAX_CMD_LEN];
  int historyCount;
  int historyIndex;       // Current position when navigating
  bool browsingHistory;

  // Escape sequence parsing (for arrow keys)
  int escapeState;        // 0=none, 1=got ESC, 2=got [
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

  // Input processing (non-blocking)
  InputSource _lastInputSource;  // Track last input source for correct echo
  void processInput(char c, InputSource source);  // Process character from either source
  void processCommand(const char* cmd);
  void parseCommand(const char* cmd, char* verb, char* args);
  void executeCommand(const char* verb, const char* args);
  void handleMenuInput(char input);
  void handleEscapeSequence(char c);
  void handleSpecialKey(const char* seq);

  // Command history
  void addToHistory(const char* cmd);
  void navigateHistory(int direction);  // -1 = older, +1 = newer
  void clearInputLine();

  // Tab completion
  void handleTabCompletion();
  int findMatchingCommands(const char* prefix, const CommandDesc** matches, int maxMatches);
  const char* resolveAlias(const char* cmd);

  // Command handlers - Message
  void cmdSend(const char* args);
  void cmdEmergency();
  void cmdCancel();

  // Command handlers - Mesh
  void cmdMesh();
  void cmdPeers();
  void cmdMeshSend(const char* args);
  void cmdMeshBroadcast(const char* args);
  void cmdGPS();

  // Command handlers - Display
  void cmdStatus();
  void cmdMessages();
  void cmdConfig();

  // Command handlers - Configuration
  void cmdName(const char* newName);
  void cmdPasskey(const char* newKey);
  void cmdMode(const char* modeStr);
  void cmdAnsi(const char* args);

  // Command handlers - System
  void cmdRestart();
  void cmdUptime();
  void cmdMemory();
  void cmdHelp(const char* args);
  void cmdClear();
  void cmdMenu();
  void cmdHistory();
  void cmdVersion();

  // Display functions
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
  TerminalMode getMode() const;
  bool isAnsiEnabled() const;
  void setAnsiEnabled(bool enabled);
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

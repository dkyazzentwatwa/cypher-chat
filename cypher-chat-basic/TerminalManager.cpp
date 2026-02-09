#include "TerminalManager.h"
#include "MeshManager.h"
#include "OutputManager.h"
#if BLE_UART_ENABLED
#include "BLEUARTManager.h"
#endif
#include <Preferences.h>

// Global instance
TerminalManager terminalMgr;

// External references
extern String unitName;
extern uint32_t currentPasskey;
extern bool isServer;
extern void simulateButtonPress(int buttonIndex);
extern void broadcastEmergency();
extern void cancelEmergency();
extern bool emergencyActive;

// Command registry - all available commands with metadata
const CommandDesc TerminalManager::commands[] = {
  // Message commands
  {"send",      "s",    "<1-4>",    "Send button message (1=ACK, 2=ENROUTE, 3=HELP, 4=OK)", CAT_MESSAGE},
  {"emergency", "sos",  nullptr,    "Trigger emergency broadcast (BLE + Mesh)",             CAT_MESSAGE},
  {"cancel",    nullptr, nullptr,   "Cancel active emergency",                              CAT_MESSAGE},

  // Mesh commands
  {"mesh",      "m",    nullptr,    "Show mesh network status and statistics",              CAT_MESH},
  {"peers",     "p",    nullptr,    "List discovered mesh peers",                           CAT_MESH},
  {"meshsend",  "ms",   "<msg>",    "Send message via mesh network",                        CAT_MESH},
  {"broadcast", "bc",   "<msg>",    "Broadcast message to all mesh peers",                  CAT_MESH},
  {"gps",       nullptr, nullptr,   "Show GPS status and coordinates",                      CAT_MESH},

  // Display commands
  {"status",    "st",   nullptr,    "Show current device status",                           CAT_DISPLAY},
  {"messages",  "msg",  nullptr,    "Show message history",                                 CAT_DISPLAY},
  {"config",    "cfg",  nullptr,    "Show current configuration",                           CAT_DISPLAY},

  // Configuration commands
  {"name",      nullptr, "<name>",  "Change unit name",                                     CAT_CONFIG},
  {"passkey",   "pk",   "<6dig>",   "Change passkey (requires restart)",                    CAT_CONFIG},
  {"mode",      nullptr, "<mode>",  "Set output mode (quiet/normal/verbose/monitor)",       CAT_CONFIG},
  {"ansi",      nullptr, "<on/off>", "Enable/disable ANSI colors",                          CAT_CONFIG},

  // System commands
  {"help",      "h",    "[cmd]",    "Show help (optionally for specific command)",          CAT_SYSTEM},
  {"clear",     "cls",  nullptr,    "Clear terminal screen",                                CAT_SYSTEM},
  {"menu",      nullptr, nullptr,   "Switch to interactive menu mode",                      CAT_SYSTEM},
  {"history",   "hist", nullptr,    "Show command history",                                 CAT_SYSTEM},
  {"restart",   "reboot", nullptr,  "Restart the device",                                   CAT_SYSTEM},
  {"uptime",    nullptr, nullptr,   "Show system uptime",                                   CAT_SYSTEM},
  {"memory",    "mem",  nullptr,    "Show memory usage",                                    CAT_SYSTEM},
  {"version",   "ver",  nullptr,    "Show firmware version",                                CAT_SYSTEM},
};

const int TerminalManager::numCommands = sizeof(commands) / sizeof(commands[0]);

void TerminalManager::begin() {
  if (!TERMINAL_ENABLED) {
    enabled = false;
    return;
  }

  enabled = true;
  inputPos = 0;
  menuState = MENU_NONE;
  mode = TERMINAL_DEFAULT_MODE;
  ansiEnabled = true;
  lastUpdate = 0;

  // Initialize command history
  historyCount = 0;
  historyIndex = 0;
  browsingHistory = false;
  for (int i = 0; i < TERM_HISTORY_SIZE; i++) {
    history[i][0] = '\0';
  }

  // Initialize escape sequence parser
  escapeState = 0;
  escapePos = 0;

  loadConfig();  // Load saved preferences

  delay(100);
  printBanner();

  output.println();
  output.println("Press 'M' for menu, or wait for command mode...");
  output.println("(Use UP/DOWN arrows for history, TAB for completion)");

  // Wait 3 seconds for mode selection
  unsigned long start = millis();
  while (millis() - start < 3000) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == 'M' || c == 'm') {
        menuState = MENU_MAIN;
        displayMainMenu();
      } else {
        menuState = MENU_NONE;
        output.println("\nCommand mode. Type 'help' for commands.");
        printPrompt();
      }
      return;
    }
  }

  // Default to command mode
  menuState = MENU_NONE;
  output.println("\nCommand mode. Type 'help' for commands.");
  printPrompt();
}

void TerminalManager::setMode(TerminalMode newMode) {
  mode = newMode;
  saveConfig();
}

bool TerminalManager::isMenuMode() {
  return menuState != MENU_NONE;
}

void TerminalManager::toggleMode() {
  if (menuState == MENU_NONE) {
    menuState = MENU_MAIN;
    displayMainMenu();
  } else {
    menuState = MENU_NONE;
    output.println("Switched to command mode.");
    printPrompt();
  }
}

void TerminalManager::poll() {
  if (!enabled) return;

  // Priority: check USB Serial first
  while (Serial.available()) {
    char c = Serial.read();
    processInput(c, INPUT_SOURCE_USB);
  }

#if BLE_UART_ENABLED
  // Then check Bluetooth Serial
  while (bleUARTMgr.isConnected() && bleUARTMgr.available()) {
    char c = bleUARTMgr.read();
    processInput(c, INPUT_SOURCE_BT);
  }
#endif
}

void TerminalManager::processInput(char c, InputSource source) {
  _lastInputSource = source;

  // Determine which stream to echo to (character echo goes back to source only)
  // Command output uses OutputManager and broadcasts to all streams
  bool echoToUSB = (source == INPUT_SOURCE_USB);
  bool echoToBT = (source == INPUT_SOURCE_BT);

  if (menuState != MENU_NONE) {
    // Menu mode - single character input
    if (c >= '0' && c <= '9') {
      handleMenuInput(c);
    }
  } else {
    // Handle escape sequences (arrow keys, etc.)
    if (escapeState > 0) {
      handleEscapeSequence(c);
      return;
    }

    // Check for escape character start
    if (c == 0x1B) {  // ESC
      escapeState = 1;
      escapePos = 0;
      return;
    }

    // Tab completion
    if (c == '\t') {
      handleTabCompletion();
      return;
    }

    // Command mode - line-based input
    if (c == '\n' || c == '\r') {
      if (inputPos > 0) {
        inputBuffer[inputPos] = '\0';

        // Echo newline to source stream only
        if (echoToUSB) Serial.println();
#if BLE_UART_ENABLED
        if (echoToBT) {
          bleUARTMgr.write('\r');
          bleUARTMgr.write('\n');
        }
#endif

        addToHistory(inputBuffer);
        processCommand(inputBuffer);
        inputPos = 0;
        browsingHistory = false;
        printPrompt();
      } else {
        // Empty line - just print prompt
        if (echoToUSB) Serial.println();
#if BLE_UART_ENABLED
        if (echoToBT) {
          bleUARTMgr.write('\r');
          bleUARTMgr.write('\n');
        }
#endif
        printPrompt();
      }
    } else if (c == 0x08 || c == 0x7F) {  // Backspace
      if (inputPos > 0) {
        inputPos--;

        // Echo backspace to source stream only
        if (echoToUSB) Serial.print("\b \b");
#if BLE_UART_ENABLED
        if (echoToBT) {
          bleUARTMgr.write('\b');
          bleUARTMgr.write(' ');
          bleUARTMgr.write('\b');
        }
#endif
      }
    } else if (c == 0x03) {  // Ctrl+C
      // Echo Ctrl+C indicator to source stream
      if (echoToUSB) Serial.println("^C");
#if BLE_UART_ENABLED
      if (echoToBT) {
        bleUARTMgr.write('^');
        bleUARTMgr.write('C');
        bleUARTMgr.write('\r');
        bleUARTMgr.write('\n');
      }
#endif

      inputPos = 0;
      browsingHistory = false;
      printPrompt();
    } else if (c == 0x0C) {  // Ctrl+L (clear screen)
      cmdClear();
      printPrompt();

      // Reprint current input to source stream
      if (echoToUSB) Serial.print(inputBuffer);
#if BLE_UART_ENABLED
      if (echoToBT) bleUARTMgr.write((const uint8_t*)inputBuffer, strlen(inputBuffer));
#endif
    } else if (inputPos < TERM_MAX_CMD_LEN - 1 && c >= 0x20) {
      inputBuffer[inputPos++] = c;
      inputBuffer[inputPos] = '\0';

      // Echo character to source stream only
      if (echoToUSB) Serial.write(c);
#if BLE_UART_ENABLED
      if (echoToBT) bleUARTMgr.write(c);
#endif

      browsingHistory = false;
    }
  }
}

void TerminalManager::handleEscapeSequence(char c) {
  if (escapeState == 1) {
    if (c == '[') {
      escapeState = 2;
      escapeBuffer[escapePos++] = c;
    } else {
      // Not a CSI sequence, reset
      escapeState = 0;
    }
    return;
  }

  if (escapeState == 2) {
    escapeBuffer[escapePos++] = c;
    escapeBuffer[escapePos] = '\0';

    // Check for complete sequences
    if (c >= 'A' && c <= 'D') {
      // Arrow keys: A=up, B=down, C=right, D=left
      handleSpecialKey(escapeBuffer);
      escapeState = 0;
      escapePos = 0;
    } else if (c == '~') {
      // Other keys like Home, End, Delete
      handleSpecialKey(escapeBuffer);
      escapeState = 0;
      escapePos = 0;
    } else if (escapePos >= 6) {
      // Sequence too long, abort
      escapeState = 0;
      escapePos = 0;
    }
  }
}

void TerminalManager::handleSpecialKey(const char* seq) {
  if (strcmp(seq, "[A") == 0) {
    // Up arrow - previous command
    navigateHistory(-1);
  } else if (strcmp(seq, "[B") == 0) {
    // Down arrow - next command
    navigateHistory(1);
  } else if (strcmp(seq, "[C") == 0) {
    // Right arrow - move cursor right (not implemented for simplicity)
  } else if (strcmp(seq, "[D") == 0) {
    // Left arrow - move cursor left (not implemented for simplicity)
  } else if (strcmp(seq, "[3~") == 0) {
    // Delete key - same as backspace for now
    if (inputPos > 0) {
      inputPos--;
      Serial.print("\b \b");
    }
  }
}

void TerminalManager::addToHistory(const char* cmd) {
  if (cmd == nullptr || strlen(cmd) == 0) return;

  // Don't add duplicates of the last command
  if (historyCount > 0) {
    int lastIdx = (historyCount - 1) % TERM_HISTORY_SIZE;
    if (strcmp(history[lastIdx], cmd) == 0) return;
  }

  // Add to circular buffer
  int idx = historyCount % TERM_HISTORY_SIZE;
  strncpy(history[idx], cmd, TERM_MAX_CMD_LEN - 1);
  history[idx][TERM_MAX_CMD_LEN - 1] = '\0';
  historyCount++;
  historyIndex = historyCount;  // Reset to end
}

void TerminalManager::navigateHistory(int direction) {
  if (historyCount == 0) return;

  int newIndex = historyIndex + direction;

  // Clamp to valid range
  int minIndex = (historyCount > TERM_HISTORY_SIZE) ? historyCount - TERM_HISTORY_SIZE : 0;
  if (newIndex < minIndex) newIndex = minIndex;
  if (newIndex > historyCount) newIndex = historyCount;

  if (newIndex == historyIndex) return;  // No change

  historyIndex = newIndex;
  browsingHistory = true;

  // Clear current line
  clearInputLine();

  // Show history entry or empty line
  if (historyIndex < historyCount) {
    int idx = historyIndex % TERM_HISTORY_SIZE;
    strcpy(inputBuffer, history[idx]);
    inputPos = strlen(inputBuffer);
    Serial.print(inputBuffer);
  } else {
    inputBuffer[0] = '\0';
    inputPos = 0;
  }
}

void TerminalManager::clearInputLine() {
  // Move cursor back and clear
  for (size_t i = 0; i < inputPos; i++) {
    Serial.print("\b \b");
  }
  inputPos = 0;
  inputBuffer[0] = '\0';
}

void TerminalManager::handleTabCompletion() {
  if (inputPos == 0) {
    // Show all commands
    Serial.println();
    cmdHelp("");
    printPrompt();
    return;
  }

  inputBuffer[inputPos] = '\0';

  // Find matching commands
  const CommandDesc* matches[10];
  int matchCount = findMatchingCommands(inputBuffer, matches, 10);

  if (matchCount == 0) {
    // No matches - beep (if terminal supports it)
    Serial.print('\a');
  } else if (matchCount == 1) {
    // Single match - complete it
    clearInputLine();
    strcpy(inputBuffer, matches[0]->name);
    inputPos = strlen(inputBuffer);
    Serial.print(inputBuffer);

    // Add space if no arguments expected
    if (matches[0]->args == nullptr) {
      inputBuffer[inputPos++] = ' ';
      inputBuffer[inputPos] = '\0';
      Serial.print(' ');
    }
  } else {
    // Multiple matches - show them
    Serial.println();
    for (int i = 0; i < matchCount; i++) {
      Serial.print("  ");
      printColored(matches[i]->name, COLOR_CYAN);
      if (matches[i]->alias) {
        Serial.print(" (");
        Serial.print(matches[i]->alias);
        Serial.print(")");
      }
      Serial.println();
    }
    printPrompt();
    Serial.print(inputBuffer);  // Restore input
  }
}

int TerminalManager::findMatchingCommands(const char* prefix, const CommandDesc** matches, int maxMatches) {
  int count = 0;
  size_t prefixLen = strlen(prefix);

  for (int i = 0; i < numCommands && count < maxMatches; i++) {
    // Check primary name
    if (strncmp(commands[i].name, prefix, prefixLen) == 0) {
      matches[count++] = &commands[i];
      continue;
    }
    // Check alias
    if (commands[i].alias && strncmp(commands[i].alias, prefix, prefixLen) == 0) {
      matches[count++] = &commands[i];
    }
  }

  return count;
}

const char* TerminalManager::resolveAlias(const char* cmd) {
  for (int i = 0; i < numCommands; i++) {
    if (commands[i].alias && strcmp(commands[i].alias, cmd) == 0) {
      return commands[i].name;
    }
  }
  return cmd;  // Return original if no alias found
}

void TerminalManager::update() {
  if (!enabled) return;

  if (mode == TERM_MONITOR && menuState == MENU_NONE) {
    unsigned long now = millis();
    if (now - lastUpdate >= UPDATE_INTERVAL_MS) {
      displayMonitorDashboard();
      lastUpdate = now;
    }
  }
}

void TerminalManager::processCommand(const char* cmd) {
  char verb[32];
  char args[96];

  parseCommand(cmd, verb, args);
  executeCommand(verb, args);
}

void TerminalManager::parseCommand(const char* cmd, char* verb, char* args) {
  // Skip leading whitespace
  while (*cmd == ' ') cmd++;

  // Extract verb (first word)
  size_t i = 0;
  while (*cmd && *cmd != ' ' && i < 31) {
    verb[i++] = *cmd++;
  }
  verb[i] = '\0';

  // Skip whitespace between verb and args
  while (*cmd == ' ') cmd++;

  // Copy remaining args
  strncpy(args, cmd, 95);
  args[95] = '\0';
}

void TerminalManager::executeCommand(const char* verb, const char* args) {
  if (strlen(verb) == 0) return;

  // Resolve alias to primary command name
  const char* cmd = resolveAlias(verb);

  // Message commands
  if (strcmp(cmd, "send") == 0) {
    cmdSend(args);
  } else if (strcmp(cmd, "emergency") == 0) {
    cmdEmergency();
  } else if (strcmp(cmd, "cancel") == 0) {
    cmdCancel();
  }
  // Mesh commands
  else if (strcmp(cmd, "mesh") == 0) {
    cmdMesh();
  } else if (strcmp(cmd, "peers") == 0) {
    cmdPeers();
  } else if (strcmp(cmd, "meshsend") == 0) {
    cmdMeshSend(args);
  } else if (strcmp(cmd, "broadcast") == 0) {
    cmdMeshBroadcast(args);
  } else if (strcmp(cmd, "gps") == 0) {
    cmdGPS();
  }
  // Display commands
  else if (strcmp(cmd, "status") == 0) {
    cmdStatus();
  } else if (strcmp(cmd, "messages") == 0) {
    cmdMessages();
  } else if (strcmp(cmd, "config") == 0) {
    cmdConfig();
  }
  // Configuration commands
  else if (strcmp(cmd, "name") == 0) {
    cmdName(args);
  } else if (strcmp(cmd, "passkey") == 0) {
    cmdPasskey(args);
  } else if (strcmp(cmd, "mode") == 0) {
    cmdMode(args);
  } else if (strcmp(cmd, "ansi") == 0) {
    cmdAnsi(args);
  }
  // System commands
  else if (strcmp(cmd, "help") == 0) {
    cmdHelp(args);
  } else if (strcmp(cmd, "clear") == 0) {
    cmdClear();
  } else if (strcmp(cmd, "menu") == 0) {
    cmdMenu();
  } else if (strcmp(cmd, "history") == 0) {
    cmdHistory();
  } else if (strcmp(cmd, "restart") == 0) {
    cmdRestart();
  } else if (strcmp(cmd, "uptime") == 0) {
    cmdUptime();
  } else if (strcmp(cmd, "memory") == 0) {
    cmdMemory();
  } else if (strcmp(cmd, "version") == 0) {
    cmdVersion();
  }
  // Unknown command
  else {
    printColored("Unknown command: ", COLOR_RED);
    output.println(verb);
    output.println("Type 'help' for available commands, or use TAB for completion.");
  }
}

void TerminalManager::handleMenuInput(char input) {
  switch (menuState) {
    case MENU_MAIN:
      switch (input) {
        case '1': menuState = MENU_SEND; displaySendMenu(); break;
        case '2': cmdStatus(); delay(2000); displayMainMenu(); break;
        case '3': menuState = MENU_MESH; displayMeshMenu(); break;
        case '4': menuState = MENU_CONFIG; displayConfigMenu(); break;
        case '5': cmdEmergency(); delay(2000); displayMainMenu(); break;
        case '6': cmdMenu(); break;  // Switch to command mode
        case '0': menuState = MENU_NONE; output.println("Exiting menu mode."); printPrompt(); break;
        default: output.println("Invalid choice."); delay(500); displayMainMenu(); break;
      }
      break;

    case MENU_SEND:
      if (input >= '1' && input <= '4') {
        cmdSend(String(input).c_str());
        output.println("Message sent.");
        delay(1000);
        menuState = MENU_MAIN;
        displayMainMenu();
      } else if (input == '0') {
        menuState = MENU_MAIN;
        displayMainMenu();
      }
      break;

    case MENU_MESH:
      switch (input) {
        case '1': cmdMesh(); delay(2000); displayMeshMenu(); break;
        case '2': cmdPeers(); delay(3000); displayMeshMenu(); break;
        case '3': cmdGPS(); delay(2000); displayMeshMenu(); break;
        case '0': menuState = MENU_MAIN; displayMainMenu(); break;
        default: output.println("Invalid choice."); delay(500); displayMeshMenu(); break;
      }
      break;

    case MENU_CONFIG:
      if (input == '0') {
        menuState = MENU_MAIN;
        displayMainMenu();
      } else {
        output.println("Configuration menu not yet implemented.");
        delay(1000);
        menuState = MENU_MAIN;
        displayMainMenu();
      }
      break;

    default:
      break;
  }
}

// Command implementations
void TerminalManager::cmdSend(const char* args) {
  int buttonNum = atoi(args);
  if (buttonNum < 1 || buttonNum > 4) {  // Fixed: use 4 instead of NUM_BUTTONS for basic version
    printColored("Error: Button number must be 1-4", COLOR_RED);
    output.println();
    return;
  }

  simulateButtonPress(buttonNum - 1);

  output.print("Sending: ");
  output.println(BUTTON_LABELS[buttonNum - 1]);
}

void TerminalManager::cmdEmergency() {
  if (!emergencyActive) {
    printColored("[EMERGENCY] Triggering emergency broadcast...", COLOR_RED);
    output.println();
    broadcastEmergency();
  } else {
    printColored("[EMERGENCY] Emergency already active", COLOR_YELLOW);
    output.println();
  }
}

void TerminalManager::cmdCancel() {
  if (emergencyActive) {
    printColored("[EMERGENCY] Canceling emergency broadcast...", COLOR_YELLOW);
    output.println();
    cancelEmergency();
  } else {
    output.println("No active emergency to cancel.");
  }
}

void TerminalManager::cmdStatus() {
  printStatus();
}

void TerminalManager::cmdMessages() {
  printMessageHistory();
}

void TerminalManager::cmdConfig() {
  printConfiguration();
}

void TerminalManager::cmdName(const char* newName) {
  if (strlen(newName) == 0) {
    output.println("Error: Unit name cannot be empty");
    return;
  }

  unitName = String(newName);
  output.print("Unit name changed to: ");
  output.println(unitName);
  output.println("Note: Restart required for BLE advertising to update.");
}

void TerminalManager::cmdPasskey(const char* newKey) {
  if (strlen(newKey) != PASSKEY_DIGITS) {
    output.print("Error: Passkey must be exactly ");
    output.print(PASSKEY_DIGITS);
    output.println(" digits");
    return;
  }

  uint32_t passkey = atoi(newKey);
  if (passkey < MIN_PASSKEY || passkey > MAX_PASSKEY) {
    output.println("Error: Passkey out of range (100000-999999)");
    return;
  }

  currentPasskey = passkey;
  output.print("Passkey changed to: ");
  output.println(currentPasskey);
  output.println("Note: Restart required to apply new passkey.");
}

void TerminalManager::cmdMode(const char* modeStr) {
  if (strcmp(modeStr, "quiet") == 0) {
    setMode(TERM_QUIET);
    output.println("Terminal mode: QUIET (errors only)");
  } else if (strcmp(modeStr, "normal") == 0) {
    setMode(TERM_NORMAL);
    output.println("Terminal mode: NORMAL (status + messages)");
  } else if (strcmp(modeStr, "verbose") == 0) {
    setMode(TERM_VERBOSE);
    output.println("Terminal mode: VERBOSE (full debug)");
  } else if (strcmp(modeStr, "monitor") == 0) {
    setMode(TERM_MONITOR);
    output.println("Terminal mode: MONITOR (live dashboard)");
    output.println("Press any key to exit monitor mode.");
    lastUpdate = 0;  // Force immediate update
  } else {
    output.println("Error: Invalid mode. Use: quiet, normal, verbose, or monitor");
  }
}

void TerminalManager::cmdRestart() {
  output.println("Restarting ESP32 in 3 seconds...");
  delay(3000);
  ESP.restart();
}

void TerminalManager::cmdUptime() {
  char uptime[32];
  formatUptime(uptime, sizeof(uptime));
  output.print("System uptime: ");
  output.println(uptime);
}

void TerminalManager::cmdMemory() {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t totalHeap = ESP.getHeapSize();
  uint32_t usedHeap = totalHeap - freeHeap;

  output.print("Memory usage: ");
  output.print(usedHeap / 1024);
  output.print("KB / ");
  output.print(totalHeap / 1024);
  output.print("KB (");
  output.print((usedHeap * 100) / totalHeap);
  output.println("%)");
  output.print("Free heap: ");
  output.print(freeHeap / 1024);
  output.println("KB");
}

void TerminalManager::cmdHelp(const char* args) {
  // If a specific command is requested, show detailed help
  if (args && strlen(args) > 0) {
    const char* cmdName = resolveAlias(args);

    for (int i = 0; i < numCommands; i++) {
      if (strcmp(commands[i].name, cmdName) == 0) {
        printCommandHelp(&commands[i]);
        return;
      }
    }

    printColored("Unknown command: ", COLOR_RED);
    output.println(args);
    output.println("Use 'help' without arguments to see all commands.");
    return;
  }

  // Show full help organized by category
  output.println("\n================================================");
  output.println("  CYPHER-CHAT BASIC TERMINAL COMMANDS");
  output.println("================================================\n");

  output.println("Shortcuts:");
  output.println("  UP/DOWN - command history");
  output.println("  TAB - auto-complete");
  output.println("\nType 'help <command>' for details\n");

  printCategoryHelp(CAT_MESSAGE, "Message Commands");
  printCategoryHelp(CAT_MESH, "Mesh Networking");
  printCategoryHelp(CAT_DISPLAY, "Display Commands");
  printCategoryHelp(CAT_CONFIG, "Configuration");
  printCategoryHelp(CAT_SYSTEM, "System Commands");
}

void TerminalManager::printCategoryHelp(CmdCategory cat, const char* title) {
  output.println(title);
  output.println("----------------");

  for (int i = 0; i < numCommands; i++) {
    if (commands[i].category == cat) {
      // Command name with arguments
      output.print("  ");
      output.print(commands[i].name);

      if (commands[i].args) {
        output.print(" ");
        output.print(commands[i].args);
      }

      // Show alias on same line
      if (commands[i].alias) {
        output.print(" [");
        output.print(commands[i].alias);
        output.print("]");
      }

      output.println();

      // Description on next line, indented
      output.print("    ");
      output.println(commands[i].description);
    }
  }
  output.println();
}

void TerminalManager::printCommandHelp(const CommandDesc* cmd) {
  output.println();
  printColored("Command: ", COLOR_CYAN);
  printColored(cmd->name, COLOR_GREEN);
  output.println();

  if (cmd->alias) {
    printColored("Alias:   ", COLOR_CYAN);
    printColored(cmd->alias, COLOR_MAGENTA);
    output.println();
  }

  if (cmd->args) {
    printColored("Usage:   ", COLOR_CYAN);
    output.print(cmd->name);
    output.print(" ");
    printColored(cmd->args, COLOR_YELLOW);
    output.println();
  }

  printColored("Info:    ", COLOR_CYAN);
  output.println(cmd->description);

  // Additional usage examples for specific commands
  if (strcmp(cmd->name, "send") == 0) {
    output.println("\nExamples:");
    output.println("  send 1    - Send ACK");
    output.println("  send 2    - Send ENROUTE");
    output.println("  send 3    - Send NEED HELP");
    output.println("  send 4    - Send ALL GOOD");
    output.println("  s 2       - Using alias");
  } else if (strcmp(cmd->name, "mode") == 0) {
    output.println("\nModes:");
    output.println("  quiet   - Errors only");
    output.println("  normal  - Status + messages (default)");
    output.println("  verbose - Full debug output");
    output.println("  monitor - Live dashboard (1Hz refresh)");
  } else if (strcmp(cmd->name, "meshsend") == 0) {
    output.println("\nExamples:");
    output.println("  meshsend Hello everyone");
    output.println("  ms Need assistance at north gate");
  }

  output.println();
}

void TerminalManager::cmdHistory() {
  output.println("\nCommand History:");
  output.println("================");

  if (historyCount == 0) {
    output.println("(no commands in history)");
    return;
  }

  int start = (historyCount > TERM_HISTORY_SIZE) ? historyCount - TERM_HISTORY_SIZE : 0;
  for (int i = start; i < historyCount; i++) {
    int idx = i % TERM_HISTORY_SIZE;
    output.printf("  %3d: %s\n", i + 1, history[idx]);
  }
  output.println();
}

void TerminalManager::cmdVersion() {
  output.println();
  printColored("================================================\n", COLOR_CYAN);
  printColored("     CYPHER-CHAT BASIC Firmware                 \n", COLOR_CYAN);
  printColored("================================================\n", COLOR_CYAN);

  output.print("Version:    ");
  printColored("2.0.0-basic\n", COLOR_GREEN);

  output.print("Build:      ");
  output.println(__DATE__ " " __TIME__);

  output.print("Platform:   ");
  output.println("ESP32 (Barebones)");

  output.print("Features:   ");
#if MESH_ENABLED
  printColored("MESH ", COLOR_GREEN);
#else
  printColored("mesh ", COLOR_DIM);
#endif
#if GPS_ENABLED
  printColored("GPS ", COLOR_GREEN);
#else
  printColored("gps ", COLOR_DIM);
#endif
#if BLE_UART_ENABLED
  printColored("BLE-UART ", COLOR_GREEN);
#else
  printColored("ble-uart ", COLOR_DIM);
#endif
  printColored("HMAC-SHA256\n", COLOR_GREEN);

  output.println();
}

void TerminalManager::cmdAnsi(const char* args) {
  if (args == nullptr || strlen(args) == 0) {
    output.print("ANSI colors: ");
    output.println(ansiEnabled ? "enabled" : "disabled");
    output.println("Usage: ansi <on|off>");
    return;
  }

  if (strcmp(args, "on") == 0 || strcmp(args, "1") == 0) {
    ansiEnabled = true;
    saveConfig();
    printColored("ANSI colors enabled\n", COLOR_GREEN);
  } else if (strcmp(args, "off") == 0 || strcmp(args, "0") == 0) {
    ansiEnabled = false;
    saveConfig();
    output.println("ANSI colors disabled");
  } else {
    output.println("Usage: ansi <on|off>");
  }
}

void TerminalManager::cmdClear() {
  if (ansiEnabled) {
    output.print("\033[2J\033[H");  // Clear screen and move to home
  } else {
    for (int i = 0; i < 50; i++) output.println();  // Fallback
  }
}

void TerminalManager::cmdMenu() {
  toggleMode();
}

// Mesh networking commands
void TerminalManager::cmdMesh() {
#if MESH_ENABLED
  output.println("\n================================================");
  output.println("  Mesh Network Status");
  output.println("================================================");

  if (!meshMgr.isRunning()) {
    output.println("Mesh networking: DISABLED");
    output.println("================================================\n");
    return;
  }

  // Get MAC address
  uint8_t mac[6];
  meshMgr.getMyMac(mac);
  char macStr[20];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  output.print("My MAC: ");
  output.println(macStr);

  output.print("Status: ");
  output.print(meshMgr.isRunning() ? "RUNNING" : "STOPPED");
  output.print("  Peers Online: ");
  output.println(meshMgr.getOnlinePeerCount());

  output.println("------------------------------------------------");

  output.print("Messages Sent: ");
  output.println(meshMgr.getMessagesSent());

  output.print("Messages Received: ");
  output.println(meshMgr.getMessagesReceived());

  output.print("Messages Relayed: ");
  output.println(meshMgr.getMessagesRelayed());

  output.print("Messages Dropped: ");
  output.println(meshMgr.getMessagesDropped());

  output.print("Store Queue: ");
  output.print(meshMgr.getStoredMessageCount());
  output.print(" / ");
  output.print(MESH_STORE_QUEUE_SIZE);
  output.println(" messages");

  output.println("================================================\n");
#else
  output.println("Mesh networking is disabled (MESH_ENABLED=false)");
#endif
}

void TerminalManager::cmdPeers() {
#if MESH_ENABLED
  if (!meshMgr.isRunning()) {
    output.println("Mesh networking not running.");
    return;
  }

  std::vector<MeshPeerInfo> peers = meshMgr.getPeers();

  output.println("\n================================================");
  output.println("  Mesh Peers");
  output.println("================================================");

  if (peers.empty()) {
    output.println("No peers discovered yet.");
    output.println("Peers are discovered via heartbeat (every 15s).");
  } else {
    for (const auto& peer : peers) {
      char macStr[18];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               peer.mac[0], peer.mac[1], peer.mac[2],
               peer.mac[3], peer.mac[4], peer.mac[5]);

      output.println();
      output.print("MAC: ");
      output.println(macStr);

      if (strlen(peer.unitName) > 0) {
        output.print("Name: ");
        output.println(peer.unitName);
      }

      output.print("Signal: ");
      output.print(peer.rssi);
      output.print(" dBm  Hops: ");
      output.print(peer.hopDistance);
      output.print("  Status: ");
      output.println(peer.isOnline ? "ONLINE" : "OFFLINE");
    }
  }

  output.println("\n================================================");
  output.print("Total: ");
  output.print(peers.size());
  output.print(" peers (");
  output.print(meshMgr.getOnlinePeerCount());
  output.println(" online)\n");
#else
  output.println("Mesh networking is disabled (MESH_ENABLED=false)");
#endif
}

void TerminalManager::cmdMeshSend(const char* args) {
#if MESH_ENABLED
  if (!meshMgr.isRunning()) {
    output.println("Mesh networking not running.");
    return;
  }

  if (strlen(args) == 0) {
    output.println("Usage: meshsend <message>");
    return;
  }

  // Build message with unit name prefix
  char msg[128];
  snprintf(msg, sizeof(msg), "%s:%s", unitName.c_str(), args);

  uint32_t msgId = meshMgr.broadcast((uint8_t*)msg, strlen(msg), MESH_MSG_DATA, MESH_DEFAULT_TTL);

  if (msgId > 0) {
    printColored("[MESH TX] ", COLOR_CYAN);
    output.printf("Broadcast sent (id: %08X, TTL: %d)\n", msgId, MESH_DEFAULT_TTL);
    output.print("  Message: ");
    output.println(msg);
  } else {
    printColored("[MESH] ", COLOR_RED);
    output.println("Failed to send broadcast");
  }
#else
  output.println("Mesh networking is disabled (MESH_ENABLED=false)");
#endif
}

void TerminalManager::cmdMeshBroadcast(const char* args) {
  // Alias for meshsend for convenience
  cmdMeshSend(args);
}

// GPS commands
void TerminalManager::cmdGPS() {
#if GPS_ENABLED
  // GPS functionality would go here if enabled
  output.println("GPS is enabled but no GPS manager in basic version");
#else
  output.println("GPS is disabled (GPS_ENABLED=false in Config_Basic.h)");
  output.println("This barebones version does not support GPS.");
#endif
}

// Display functions
void TerminalManager::displayMainMenu() {
  if (ansiEnabled) {
    output.print("\033[2J\033[H");  // Clear screen
  }

  output.println("================================================");
  output.println("        CYPHER-CHAT BASIC Control Menu          ");
  output.println("================================================");
  output.println(" [1] Send Message                               ");
  output.println(" [2] View Status                                ");
  output.println(" [3] Mesh Network                               ");
  output.println(" [4] Configuration                              ");
  output.println(" [5] EMERGENCY Broadcast                        ");
  output.println(" [6] Switch to Command Mode                     ");
  output.println(" [0] Exit Menu                                  ");
  output.println("================================================");
  output.print("Enter choice: ");
}

void TerminalManager::displaySendMenu() {
  if (ansiEnabled) {
    output.print("\033[2J\033[H");
  }

  output.println("================================================");
  output.println("              Send Message                      ");
  output.println("================================================");
  output.println(" [1] ACK                                        ");
  output.println(" [2] ENROUTE                                    ");
  output.println(" [3] NEED HELP                                  ");
  output.println(" [4] ALL GOOD                                   ");
  output.println(" [0] Back to Main Menu                          ");
  output.println("================================================");
  output.print("Enter choice: ");
}

void TerminalManager::displayConfigMenu() {
  if (ansiEnabled) {
    output.print("\033[2J\033[H");
  }

  output.println("================================================");
  output.println("              Configuration                     ");
  output.println("================================================");
  output.print(" Unit Name: ");
  output.println(unitName);
  output.print(" Role: ");
  output.println(isServer ? "SERVER" : "CLIENT");
  output.println("================================================");
  output.println(" Use command mode to change settings:           ");
  output.println("   name <newname>  - Change unit name           ");
  output.println("   passkey <6dig>  - Change passkey             ");
  output.println("   mode <mode>     - Change terminal mode       ");
  output.println("================================================");
  output.println(" [0] Back to Main Menu                          ");
  output.println("================================================");
  output.print("Enter choice: ");
}

void TerminalManager::displayMeshMenu() {
  if (ansiEnabled) {
    output.print("\033[2J\033[H");
  }

  output.println("================================================");
  output.println("              Mesh Network                      ");
  output.println("================================================");

#if MESH_ENABLED
  if (meshMgr.isRunning()) {
    char line[52];
    snprintf(line, sizeof(line), " Status: ACTIVE        Peers: %-3d",
             meshMgr.getOnlinePeerCount());
    output.println(line);
    snprintf(line, sizeof(line), " TX: %-6lu  RX: %-6lu  Relay: %-6lu",
             meshMgr.getMessagesSent(), meshMgr.getMessagesReceived(),
             meshMgr.getMessagesRelayed());
    output.println(line);
  } else {
    output.println(" Status: NOT RUNNING                            ");
  }
#else
  output.println(" Status: DISABLED (compile with MESH_ENABLED)   ");
#endif

  output.println("================================================");
  output.println(" [1] View Mesh Status                           ");
  output.println(" [2] View Peers                                 ");
  output.println(" [3] View GPS                                   ");
  output.println(" [0] Back to Main Menu                          ");
  output.println("================================================");
  output.print("Enter choice: ");
}

void TerminalManager::displayMonitorDashboard() {
  if (ansiEnabled) {
    output.print("\033[2J\033[H");  // Clear and home
  }

  output.println("================================================");
  output.println(" STARBEAM BASIC - LIVE MONITOR                  ");
  output.println("================================================");

  // Status line
  char line[64];
  extern StateManager connectionState;
  ConnectionState state = connectionState.getState();
  const char* stateStr = getStateString(state);

  char uptime[16];
  formatUptime(uptime, sizeof(uptime));

  snprintf(line, sizeof(line), " State: %-12s      Uptime: %8s",
           stateStr, uptime);
  output.println(line);

  // Retry and memory
  int retries = connectionState.getRetryCount();
  uint32_t freeHeap = ESP.getFreeHeap();
  snprintf(line, sizeof(line), " Retry: %-3d                Memory: %3luKB free",
           retries, freeHeap / 1024);
  output.println(line);

  // Role and unit
  const char* role = isServer ? "SERVER" : "CLIENT";
  snprintf(line, sizeof(line), " Role: %-10s          Unit: %-11s",
           role, unitName.c_str());
  output.println(line);

#if MESH_ENABLED
  output.println("------------------------------------------------");
  snprintf(line, sizeof(line), " Mesh Peers: %-3d  TX: %-5lu  RX: %-5lu",
           meshMgr.getOnlinePeerCount(),
           meshMgr.getMessagesSent(),
           meshMgr.getMessagesReceived());
  output.println(line);
#endif

  output.println("================================================");
  output.println("Press any key to exit monitor mode...");
}

void TerminalManager::printColored(const char* text, const char* color) {
  if (ansiEnabled && color) {
    output.print(color);
    output.print(text);
    output.print(COLOR_RESET);
  } else {
    output.print(text);
  }
}

void TerminalManager::printBanner() {
  if (!TERMINAL_BANNER_ENABLED) return;

  output.println();
  output.println("================================================");
  output.println("    STARBEAM BASIC - Emergency Communication    ");
  output.println("    ESP32 Headless Terminal Interface           ");
  output.println("================================================");
}

void TerminalManager::printPrompt() {
  output.print("> ");
}

void TerminalManager::printStatus() {
  output.println("\n================================================");
  output.println("  STARBEAM BASIC - Status");
  output.println("================================================");

  // Role and unit
  const char* role = isServer ? "SERVER" : "CLIENT";
  output.print("Role: ");
  output.print(role);
  output.print("  Unit: ");
  output.println(unitName);

  // State and retry
  extern StateManager connectionState;
  ConnectionState state = connectionState.getState();
  const char* stateStr = getStateString(state);
  int retries = connectionState.getRetryCount();

  output.print("State: ");
  output.print(stateStr);
  output.print("  Retry: ");
  output.println(retries);

  // Uptime
  char uptime[16];
  formatUptime(uptime, sizeof(uptime));
  output.print("Uptime: ");
  output.println(uptime);

  output.println("------------------------------------------------");

#if MESH_ENABLED
  // Mesh status
  if (meshMgr.isRunning()) {
    output.print("Mesh: ACTIVE  Peers Online: ");
    output.println(meshMgr.getOnlinePeerCount());

    output.print("Mesh TX: ");
    output.print(meshMgr.getMessagesSent());
    output.print("  RX: ");
    output.print(meshMgr.getMessagesReceived());
    output.print("  Relay: ");
    output.println(meshMgr.getMessagesRelayed());
  } else {
    output.println("Mesh: DISABLED");
  }
#else
  output.println("Mesh: NOT COMPILED");
#endif

#if BLE_UART_ENABLED
  output.print("BLE UART: ");
  output.println(bleUARTMgr.isConnected() ? "CONNECTED" : "DISCONNECTED");
#endif

  output.println("================================================\n");
}

void TerminalManager::printMessageHistory() {
  output.println("\nMessage History:");
  output.println("================");
  output.println("(Message history not available in basic version)\n");
}

void TerminalManager::printConfiguration() {
  output.println("\nDevice Configuration:");
  output.println("====================");
  output.print("Unit Name: ");
  output.println(unitName);
  output.print("Role: ");
  output.println(isServer ? "SERVER" : "CLIENT");
  output.print("Passkey: ");
  output.print(currentPasskey);
  output.println(" (configured)");
  output.print("Terminal Mode: ");
  switch (mode) {
    case TERM_QUIET: output.println("QUIET"); break;
    case TERM_NORMAL: output.println("NORMAL"); break;
    case TERM_VERBOSE: output.println("VERBOSE"); break;
    case TERM_MONITOR: output.println("MONITOR"); break;
  }
  output.println();
}

// Event logging
void TerminalManager::logEvent(const char* level, const char* message) {
  if (!enabled) return;
  if (mode == TERM_QUIET && strcmp(level, "ERROR") != 0) return;
  if (mode == TERM_MONITOR) return;  // Don't log in monitor mode

  output.print("[");

  if (strcmp(level, "ERROR") == 0) {
    printColored(level, COLOR_RED);
  } else if (strcmp(level, "INFO") == 0) {
    printColored(level, COLOR_BLUE);
  } else if (strcmp(level, "SCAN") == 0 || strcmp(level, "CONN") == 0) {
    printColored(level, COLOR_CYAN);
  } else {
    output.print(level);
  }

  output.print("] ");
  output.println(message);
}

void TerminalManager::logConnection(ConnectionState state) {
  if (!enabled || mode < TERM_NORMAL) return;

  const char* stateStr = getStateString(state);
  char msg[64];
  snprintf(msg, sizeof(msg), "State changed to: %s", stateStr);
  logEvent("CONN", msg);
}

void TerminalManager::logMessage(const char* msg, bool isOutgoing, bool delivered) {
  if (!enabled || mode < TERM_NORMAL) return;
  if (mode == TERM_MONITOR) return;

  // Format timestamp
  unsigned long seconds = millis() / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  seconds %= 60;
  minutes %= 60;

  char timestamp[16];
  snprintf(timestamp, sizeof(timestamp), "%02lu:%02lu:%02lu", hours, minutes, seconds);

  // Format message
  char formatted[128];
  const char* direction = isOutgoing ? "->" : "<-";
  const char* status = delivered ? "OK" : "";

  snprintf(formatted, sizeof(formatted), "[%s] %s %s %s",
           timestamp, direction, msg, status);

  output.println(formatted);
}

void TerminalManager::logHMAC(bool verified, const char* message) {
  if (!enabled || mode < TERM_VERBOSE) return;
  if (mode == TERM_MONITOR) return;

  if (verified) {
    printColored("[HMAC] OK Signature verified: ", COLOR_GREEN);
    output.println(message);
  } else {
    printColored("[HMAC] FAIL Verification FAILED: ", COLOR_RED);
    output.println(message);
  }
}

void TerminalManager::logEmergency(bool active) {
  if (!enabled) return;

  if (active) {
    printColored("[EMERGENCY] Emergency broadcast activated!", COLOR_RED);
  } else {
    printColored("[EMERGENCY] Emergency canceled", COLOR_YELLOW);
  }
  output.println();
}

void TerminalManager::logButtonPress(int buttonIndex, ButtonEvent event) {
  if (!enabled || mode < TERM_VERBOSE) return;
  if (mode == TERM_MONITOR) return;

  char msg[64];
  const char* eventStr = (event == BUTTON_LONG_PRESS) ? "LONG PRESS" : "PRESS";
  snprintf(msg, sizeof(msg), "Button %d (%s): %s",
           buttonIndex + 1, BUTTON_LABELS[buttonIndex], eventStr);
  logEvent("BUTTON", msg);
}

// Utility functions
const char* TerminalManager::getStateString(ConnectionState state) {
  switch (state) {
    case STATE_IDLE: return "IDLE";
    case STATE_SCANNING: return "SCANNING";
    case STATE_CONNECTING: return "CONNECTING";
    case STATE_CONNECTED: return "CONNECTED";
    case STATE_DISCONNECTED: return "DISCONNECTED";
    case STATE_ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}

void TerminalManager::formatUptime(char* buffer, size_t bufSize) {
  unsigned long totalSeconds = millis() / 1000;
  unsigned long seconds = totalSeconds % 60;
  unsigned long minutes = (totalSeconds / 60) % 60;
  unsigned long hours = (totalSeconds / 3600) % 24;
  unsigned long days = totalSeconds / 86400;

  if (days > 0) {
    snprintf(buffer, bufSize, "%lud %02lu:%02lu:%02lu", days, hours, minutes, seconds);
  } else {
    snprintf(buffer, bufSize, "%02lu:%02lu:%02lu", hours, minutes, seconds);
  }
}

void TerminalManager::detectANSISupport() {
  // For now, assume ANSI is supported
  // Could be enhanced to detect based on terminal type
  ansiEnabled = true;
}

// Configuration persistence
void TerminalManager::saveConfig() {
  Preferences prefs;
  prefs.begin("terminal", false);
  prefs.putUChar("mode", (uint8_t)mode);
  prefs.putBool("ansi", ansiEnabled);
  prefs.putBool("menu", menuOnStartup);
  prefs.end();
}

void TerminalManager::loadConfig() {
  Preferences prefs;
  prefs.begin("terminal", true);  // Read-only
  mode = (TerminalMode)prefs.getUChar("mode", TERMINAL_DEFAULT_MODE);
  ansiEnabled = prefs.getBool("ansi", true);
  menuOnStartup = prefs.getBool("menu", false);
  prefs.end();
}

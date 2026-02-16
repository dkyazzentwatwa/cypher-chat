#include "TerminalManager.h"
#include "MeshManager.h"
#include "MeshCrypto.h"
#include "OutputManager.h"
#include <EEPROM.h>

// Global instance
TerminalManager terminalMgr;

// External references
extern String unitName;
extern char currentPassphrase[65];
extern bool isServer;
extern void simulateButtonPress(int buttonIndex);
extern void broadcastEmergency();
extern void cancelEmergency();
extern bool emergencyActive;

// Command registry
const CommandDesc TerminalManager::commands[] = {
  // Message commands
  {"send",      "s",    "<1-4>",    "Send button message (1=ACK, 2=ENROUTE, 3=HELP, 4=OK)", CAT_MESSAGE},
  {"emergency", "sos",  nullptr,    "Trigger emergency broadcast via Mesh",                  CAT_MESSAGE},
  {"cancel",    nullptr, nullptr,   "Cancel active emergency",                               CAT_MESSAGE},

  // Mesh commands
  {"mesh",      "m",    nullptr,    "Show mesh network status and statistics",               CAT_MESH},
  {"peers",     "p",    nullptr,    "List discovered mesh peers",                            CAT_MESH},
  {"meshsend",  "ms",   "<msg>",    "Send message via mesh network",                         CAT_MESH},
  {"broadcast", "bc",   "<msg>",    "Broadcast message to all mesh peers",                   CAT_MESH},
  {"gps",       nullptr, nullptr,   "Show GPS status and coordinates",                       CAT_MESH},

  // Display commands
  {"status",    "st",   nullptr,    "Show current device status",                            CAT_DISPLAY},
  {"messages",  "msg",  nullptr,    "Show message history",                                  CAT_DISPLAY},
  {"config",    "cfg",  nullptr,    "Show current configuration",                            CAT_DISPLAY},

  // Configuration commands
  {"name",      nullptr, "<name>",  "Change unit name",                                      CAT_CONFIG},
  {"passphrase","pp",   "<phrase>", "Change passphrase (min 4 chars, saved to EEPROM)",      CAT_CONFIG},
  {"newkey",    "nk",   nullptr,    "Rotate encryption key (re-derives from passphrase)",    CAT_CONFIG},
  {"mode",      nullptr, "<mode>",  "Set output mode (quiet/normal/verbose/monitor)",        CAT_CONFIG},
  {"ansi",      nullptr, "<on/off>","Enable/disable ANSI colors",                            CAT_CONFIG},

  // System commands
  {"help",      "h",    "[cmd]",    "Show help (optionally for specific command)",           CAT_SYSTEM},
  {"clear",     "cls",  nullptr,    "Clear terminal screen",                                 CAT_SYSTEM},
  {"menu",      nullptr, nullptr,   "Switch to interactive menu mode",                       CAT_SYSTEM},
  {"history",   "hist", nullptr,    "Show command history",                                  CAT_SYSTEM},
  {"restart",   "reboot", nullptr,  "Restart the device",                                    CAT_SYSTEM},
  {"uptime",    nullptr, nullptr,   "Show system uptime",                                    CAT_SYSTEM},
  {"memory",    "mem",  nullptr,    "Show memory usage",                                     CAT_SYSTEM},
  {"version",   "ver",  nullptr,    "Show firmware version",                                 CAT_SYSTEM},
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

  historyCount = 0;
  historyIndex = 0;
  browsingHistory = false;
  for (int i = 0; i < TERM_HISTORY_SIZE; i++) {
    history[i][0] = '\0';
  }

  escapeState = 0;
  escapePos = 0;

  loadConfig();

  delay(100);
  printBanner();

  output.println();
  output.println("Press 'M' for menu, or wait for command mode...");
  output.println("(Use UP/DOWN arrows for history, TAB for completion)");

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

  while (Serial.available()) {
    char c = Serial.read();
    processInput(c);
  }
}

void TerminalManager::processInput(char c) {
  if (menuState != MENU_NONE) {
    if (c >= '0' && c <= '9') {
      handleMenuInput(c);
    }
  } else {
    // Handle escape sequences
    if (escapeState > 0) {
      handleEscapeSequence(c);
      return;
    }

    if (c == 0x1B) {
      escapeState = 1;
      escapePos = 0;
      return;
    }

    if (c == '\t') {
      handleTabCompletion();
      return;
    }

    if (c == '\n' || c == '\r') {
      if (inputPos > 0) {
        inputBuffer[inputPos] = '\0';
        Serial.println();
        addToHistory(inputBuffer);
        processCommand(inputBuffer);
        inputPos = 0;
        browsingHistory = false;
        printPrompt();
      } else {
        Serial.println();
        printPrompt();
      }
    } else if (c == 0x08 || c == 0x7F) {  // Backspace
      if (inputPos > 0) {
        inputPos--;
        Serial.print("\b \b");
      }
    } else if (c == 0x03) {  // Ctrl+C
      Serial.println("^C");
      inputPos = 0;
      browsingHistory = false;
      printPrompt();
    } else if (c == 0x0C) {  // Ctrl+L
      cmdClear();
      printPrompt();
      Serial.print(inputBuffer);
    } else if (inputPos < TERM_MAX_CMD_LEN - 1 && c >= 0x20) {
      inputBuffer[inputPos++] = c;
      inputBuffer[inputPos] = '\0';
      Serial.write(c);
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
      escapeState = 0;
    }
    return;
  }

  if (escapeState == 2) {
    escapeBuffer[escapePos++] = c;
    escapeBuffer[escapePos] = '\0';

    if (c >= 'A' && c <= 'D') {
      handleSpecialKey(escapeBuffer);
      escapeState = 0;
      escapePos = 0;
    } else if (c == '~') {
      handleSpecialKey(escapeBuffer);
      escapeState = 0;
      escapePos = 0;
    } else if (escapePos >= 6) {
      escapeState = 0;
      escapePos = 0;
    }
  }
}

void TerminalManager::handleSpecialKey(const char* seq) {
  if (strcmp(seq, "[A") == 0) {
    navigateHistory(-1);
  } else if (strcmp(seq, "[B") == 0) {
    navigateHistory(1);
  } else if (strcmp(seq, "[3~") == 0) {
    if (inputPos > 0) {
      inputPos--;
      Serial.print("\b \b");
    }
  }
}

void TerminalManager::addToHistory(const char* cmd) {
  if (cmd == nullptr || strlen(cmd) == 0) return;

  if (historyCount > 0) {
    int lastIdx = (historyCount - 1) % TERM_HISTORY_SIZE;
    if (strcmp(history[lastIdx], cmd) == 0) return;
  }

  int idx = historyCount % TERM_HISTORY_SIZE;
  strncpy(history[idx], cmd, TERM_MAX_CMD_LEN - 1);
  history[idx][TERM_MAX_CMD_LEN - 1] = '\0';
  historyCount++;
  historyIndex = historyCount;
}

void TerminalManager::navigateHistory(int direction) {
  if (historyCount == 0) return;

  int newIndex = historyIndex + direction;
  int minIndex = (historyCount > TERM_HISTORY_SIZE) ? historyCount - TERM_HISTORY_SIZE : 0;
  if (newIndex < minIndex) newIndex = minIndex;
  if (newIndex > historyCount) newIndex = historyCount;
  if (newIndex == historyIndex) return;

  historyIndex = newIndex;
  browsingHistory = true;
  clearInputLine();

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
  for (size_t i = 0; i < inputPos; i++) {
    Serial.print("\b \b");
  }
  inputPos = 0;
  inputBuffer[0] = '\0';
}

void TerminalManager::handleTabCompletion() {
  if (inputPos == 0) {
    Serial.println();
    cmdHelp("");
    printPrompt();
    return;
  }

  inputBuffer[inputPos] = '\0';

  const CommandDesc* matches[10];
  int matchCount = findMatchingCommands(inputBuffer, matches, 10);

  if (matchCount == 0) {
    Serial.print('\a');
  } else if (matchCount == 1) {
    clearInputLine();
    strcpy(inputBuffer, matches[0]->name);
    inputPos = strlen(inputBuffer);
    Serial.print(inputBuffer);
    if (matches[0]->args == nullptr) {
      inputBuffer[inputPos++] = ' ';
      inputBuffer[inputPos] = '\0';
      Serial.print(' ');
    }
  } else {
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
    Serial.print(inputBuffer);
  }
}

int TerminalManager::findMatchingCommands(const char* prefix, const CommandDesc** matches, int maxMatches) {
  int count = 0;
  size_t prefixLen = strlen(prefix);

  for (int i = 0; i < numCommands && count < maxMatches; i++) {
    if (strncmp(commands[i].name, prefix, prefixLen) == 0) {
      matches[count++] = &commands[i];
      continue;
    }
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
  return cmd;
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
  while (*cmd == ' ') cmd++;

  size_t i = 0;
  while (*cmd && *cmd != ' ' && i < 31) {
    verb[i++] = *cmd++;
  }
  verb[i] = '\0';

  while (*cmd == ' ') cmd++;
  strncpy(args, cmd, 95);
  args[95] = '\0';
}

void TerminalManager::executeCommand(const char* verb, const char* args) {
  if (strlen(verb) == 0) return;

  const char* cmd = resolveAlias(verb);

  if (strcmp(cmd, "send") == 0) cmdSend(args);
  else if (strcmp(cmd, "emergency") == 0) cmdEmergency();
  else if (strcmp(cmd, "cancel") == 0) cmdCancel();
  else if (strcmp(cmd, "mesh") == 0) cmdMesh();
  else if (strcmp(cmd, "peers") == 0) cmdPeers();
  else if (strcmp(cmd, "meshsend") == 0) cmdMeshSend(args);
  else if (strcmp(cmd, "broadcast") == 0) cmdMeshBroadcast(args);
  else if (strcmp(cmd, "gps") == 0) cmdGPS();
  else if (strcmp(cmd, "status") == 0) cmdStatus();
  else if (strcmp(cmd, "messages") == 0) cmdMessages();
  else if (strcmp(cmd, "config") == 0) cmdConfig();
  else if (strcmp(cmd, "name") == 0) cmdName(args);
  else if (strcmp(cmd, "passphrase") == 0) cmdPassphrase(args);
  else if (strcmp(cmd, "newkey") == 0) cmdNewKey(args);
  else if (strcmp(cmd, "mode") == 0) cmdMode(args);
  else if (strcmp(cmd, "ansi") == 0) cmdAnsi(args);
  else if (strcmp(cmd, "help") == 0) cmdHelp(args);
  else if (strcmp(cmd, "clear") == 0) cmdClear();
  else if (strcmp(cmd, "menu") == 0) cmdMenu();
  else if (strcmp(cmd, "history") == 0) cmdHistory();
  else if (strcmp(cmd, "restart") == 0) cmdRestart();
  else if (strcmp(cmd, "uptime") == 0) cmdUptime();
  else if (strcmp(cmd, "memory") == 0) cmdMemory();
  else if (strcmp(cmd, "version") == 0) cmdVersion();
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
        case '6': cmdMenu(); break;
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

// ============================================================================
// Command implementations
// ============================================================================

void TerminalManager::cmdSend(const char* args) {
  int buttonNum = atoi(args);
  if (buttonNum < 1 || buttonNum > 4) {
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
  output.println("Note: Restart required for mesh to advertise new name.");
}

void TerminalManager::cmdPassphrase(const char* newPhrase) {
  if (strlen(newPhrase) < MIN_PASSPHRASE_LEN) {
    output.print("Error: Passphrase must be at least ");
    output.print(MIN_PASSPHRASE_LEN);
    output.println(" characters");
    return;
  }
  if (strlen(newPhrase) > MAX_PASSPHRASE_LEN) {
    output.print("Error: Passphrase must be at most ");
    output.print(MAX_PASSPHRASE_LEN);
    output.println(" characters");
    return;
  }

  strncpy(currentPassphrase, newPhrase, sizeof(currentPassphrase) - 1);
  currentPassphrase[sizeof(currentPassphrase) - 1] = '\0';
  MeshCrypto::savePassphrase(currentPassphrase);
  output.print("Passphrase changed (");
  output.print(strlen(currentPassphrase));
  output.println(" chars) - saved to EEPROM");
  output.println("Note: Restart required to apply new passphrase.");
}

void TerminalManager::cmdNewKey(const char* args) {
  output.println("Rotating encryption key...");
  MeshCrypto::rotateKey(currentPassphrase);
  output.println("Encryption key rotated from current passphrase.");
  output.println("Note: All peers must use the same passphrase.");
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
    lastUpdate = 0;
  } else {
    output.println("Error: Invalid mode. Use: quiet, normal, verbose, or monitor");
  }
}

void TerminalManager::cmdRestart() {
  output.println("Restarting ESP8266 in 3 seconds...");
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

  output.print("Free heap: ");
  output.print(freeHeap / 1024);
  output.println("KB");
  output.print("Heap fragmentation: ");
  output.print(ESP.getHeapFragmentation());
  output.println("%");
}

void TerminalManager::cmdHelp(const char* args) {
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

  output.println("\n================================================");
  output.println("  CYPHER-CHAT 8266 TERMINAL COMMANDS");
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
      output.print("  ");
      output.print(commands[i].name);
      if (commands[i].args) {
        output.print(" ");
        output.print(commands[i].args);
      }
      if (commands[i].alias) {
        output.print(" [");
        output.print(commands[i].alias);
        output.print("]");
      }
      output.println();
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

  if (strcmp(cmd->name, "send") == 0) {
    output.println("\nExamples:");
    output.println("  send 1    - Send ACK");
    output.println("  send 2    - Send ENROUTE");
    output.println("  send 3    - Send NEED HELP");
    output.println("  send 4    - Send ALL GOOD");
  } else if (strcmp(cmd->name, "mode") == 0) {
    output.println("\nModes:");
    output.println("  quiet   - Errors only");
    output.println("  normal  - Status + messages (default)");
    output.println("  verbose - Full debug output");
    output.println("  monitor - Live dashboard (1Hz refresh)");
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
  printColored("     CYPHER-CHAT 8266 Firmware                  \n", COLOR_CYAN);
  printColored("================================================\n", COLOR_CYAN);

  output.print("Version:    ");
  printColored("2.0.0-esp8266\n", COLOR_GREEN);

  output.print("Build:      ");
  output.println(__DATE__ " " __TIME__);

  output.print("Platform:   ");
  output.println("ESP8266 (Serial Terminal)");

  output.print("Features:   ");
#if MESH_ENABLED
  printColored("MESH ", COLOR_GREEN);
#else
  printColored("mesh ", COLOR_DIM);
#endif
  printColored("AES-256-GCM ", COLOR_GREEN);
  printColored("SERIAL\n", COLOR_GREEN);

  output.print("Crypto:     ");
  output.println("BearSSL (HKDF-SHA256 + AES-256-GCM)");

  output.print("ESP32 compat: ");
  printColored("YES\n", COLOR_GREEN);

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
    output.print("\033[2J\033[H");
  } else {
    for (int i = 0; i < 50; i++) output.println();
  }
}

void TerminalManager::cmdMenu() {
  toggleMode();
}

// Mesh commands
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
  cmdMeshSend(args);
}

void TerminalManager::cmdGPS() {
  output.println("GPS is not available on ESP8266 serial terminal.");
}

// Display functions
void TerminalManager::displayMainMenu() {
  if (ansiEnabled) output.print("\033[2J\033[H");

  output.println("================================================");
  output.println("        CYPHER-CHAT 8266 Control Menu           ");
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
  if (ansiEnabled) output.print("\033[2J\033[H");

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
  if (ansiEnabled) output.print("\033[2J\033[H");

  output.println("================================================");
  output.println("              Configuration                     ");
  output.println("================================================");
  output.print(" Unit Name: ");
  output.println(unitName);
  output.println("================================================");
  output.println(" Use command mode to change settings:           ");
  output.println("   name <newname>  - Change unit name           ");
  output.println("   passphrase <pp> - Change passphrase          ");
  output.println("   mode <mode>     - Change terminal mode       ");
  output.println("================================================");
  output.println(" [0] Back to Main Menu                          ");
  output.println("================================================");
  output.print("Enter choice: ");
}

void TerminalManager::displayMeshMenu() {
  if (ansiEnabled) output.print("\033[2J\033[H");

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
  if (ansiEnabled) output.print("\033[2J\033[H");

  output.println("================================================");
  output.println(" CYPHER-CHAT 8266 - LIVE MONITOR                ");
  output.println("================================================");

  char line[64];
  extern StateManager connectionState;
  ConnectionState state = connectionState.getState();
  const char* stateStr = getStateString(state);

  char uptime[16];
  formatUptime(uptime, sizeof(uptime));

  snprintf(line, sizeof(line), " State: %-12s      Uptime: %8s", stateStr, uptime);
  output.println(line);

  uint32_t freeHeap = ESP.getFreeHeap();
  snprintf(line, sizeof(line), " Free heap: %luKB", freeHeap / 1024);
  output.println(line);

  snprintf(line, sizeof(line), " Unit: %-16s", unitName.c_str());
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
  output.println("    CYPHER-CHAT 8266 - Emergency Communication  ");
  output.println("    ESP8266 Serial Terminal Interface            ");
  output.println("================================================");
}

void TerminalManager::printPrompt() {
  output.print("> ");
}

void TerminalManager::printStatus() {
  output.println("\n================================================");
  output.println("  CYPHER-CHAT 8266 - Status");
  output.println("================================================");

  output.print("Unit: ");
  output.println(unitName);

  extern StateManager connectionState;
  ConnectionState state = connectionState.getState();
  const char* stateStr = getStateString(state);

  output.print("State: ");
  output.println(stateStr);

  char uptime[16];
  formatUptime(uptime, sizeof(uptime));
  output.print("Uptime: ");
  output.println(uptime);

  output.println("------------------------------------------------");

#if MESH_ENABLED
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

  output.println("BLE UART: NOT AVAILABLE (ESP8266)");
  output.println("================================================\n");
}

void TerminalManager::printMessageHistory() {
  output.println("\nMessage History:");
  output.println("================");
  output.println("(Message history not available in ESP8266 version)\n");
}

void TerminalManager::printConfiguration() {
  output.println("\nDevice Configuration:");
  output.println("====================");
  output.print("Unit Name: ");
  output.println(unitName);
  output.print("Passphrase: ");
  output.print(strlen(currentPassphrase));
  output.println(" chars (configured)");
  output.println("Encryption: AES-256-GCM AEAD (BearSSL)");
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
  if (mode == TERM_MONITOR) return;

  output.print("[");
  if (strcmp(level, "ERROR") == 0) printColored(level, COLOR_RED);
  else if (strcmp(level, "INFO") == 0) printColored(level, COLOR_BLUE);
  else if (strcmp(level, "SCAN") == 0 || strcmp(level, "CONN") == 0) printColored(level, COLOR_CYAN);
  else output.print(level);
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
  if (!enabled || mode < TERM_NORMAL || mode == TERM_MONITOR) return;

  unsigned long seconds = millis() / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  seconds %= 60;
  minutes %= 60;

  char timestamp[16];
  snprintf(timestamp, sizeof(timestamp), "%02lu:%02lu:%02lu", hours, minutes, seconds);

  char formatted[128];
  const char* direction = isOutgoing ? "->" : "<-";
  const char* status = delivered ? "OK" : "";
  snprintf(formatted, sizeof(formatted), "[%s] %s %s %s", timestamp, direction, msg, status);
  output.println(formatted);
}

void TerminalManager::logHMAC(bool verified, const char* message) {
  if (!enabled || mode < TERM_VERBOSE || mode == TERM_MONITOR) return;

  if (verified) {
    printColored("[HMAC] OK Signature verified: ", COLOR_GREEN);
  } else {
    printColored("[HMAC] FAIL Verification FAILED: ", COLOR_RED);
  }
  output.println(message);
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
  if (!enabled || mode < TERM_VERBOSE || mode == TERM_MONITOR) return;

  char msg[64];
  const char* eventStr = (event == BUTTON_LONG_PRESS) ? "LONG PRESS" : "PRESS";
  snprintf(msg, sizeof(msg), "Button %d (%s): %s",
           buttonIndex + 1, BUTTON_LABELS[buttonIndex], eventStr);
  logEvent("BUTTON", msg);
}

// Utility
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

// EEPROM-based config persistence (instead of ESP32's Preferences)
void TerminalManager::saveConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
  EEPROM.write(EEPROM_ADDR_TERM_MODE, (uint8_t)mode);
  EEPROM.write(EEPROM_ADDR_ANSI, ansiEnabled ? 1 : 0);
  EEPROM.write(EEPROM_ADDR_MENU, menuOnStartup ? 1 : 0);
  EEPROM.commit();
  EEPROM.end();
}

void TerminalManager::loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(EEPROM_ADDR_MAGIC) == EEPROM_MAGIC) {
    mode = (TerminalMode)EEPROM.read(EEPROM_ADDR_TERM_MODE);
    ansiEnabled = EEPROM.read(EEPROM_ADDR_ANSI) != 0;
    menuOnStartup = EEPROM.read(EEPROM_ADDR_MENU) != 0;
  }
  EEPROM.end();
}

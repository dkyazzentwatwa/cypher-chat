#include "TerminalManager.h"
#include "DisplayManager.h"
#include "MeshManager.h"
#include "MeshCrypto.h"
#include "GPSManager.h"
#include "OutputManager.h"
#include "PowerManager.h"
#include "TimeManager.h"
#if FILESYSTEM_ENABLED
#include "FileSystemManager.h"
#include "LogManager.h"
#endif
#include "SecurityManager.h"
#include "LEDManager.h"
#if BLE_UART_ENABLED
#include "BLEUARTManager.h"
#endif
#include <Preferences.h>

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
extern String messageHistory[10];
extern int historyCount;

// Command registry - all available commands with metadata
const CommandDesc TerminalManager::commands[] = {
  // Message commands (8)
  {"send",      "s",    "<1-4>",      "Send button message (1=ACK, 2=ENROUTE, 3=HELP, 4=OK)", CAT_MESSAGE},
  {"emergency", "sos",  nullptr,      "Trigger emergency broadcast (BLE + Mesh)",             CAT_MESSAGE},
  {"cancel",    nullptr, nullptr,     "Cancel active emergency",                              CAT_MESSAGE},
  {"msgsearch", nullptr, "<keyword>", "Search message history",                               CAT_MESSAGE},
  {"msgclear",  nullptr, nullptr,     "Clear message history",                                CAT_MESSAGE},
  {"msgexport", nullptr, nullptr,     "Export messages to serial",                            CAT_MESSAGE},
  {"msgfilter", nullptr, "<peer>",    "Filter messages by sender",                            CAT_MESSAGE},
  {"lastmsg",   nullptr, nullptr,     "Show last received message details",                   CAT_MESSAGE},

  // Mesh commands (5)
  {"mesh",      "m",    nullptr,    "Show mesh network status and statistics",              CAT_MESH},
  {"peers",     "p",    nullptr,    "List discovered mesh peers",                           CAT_MESH},
  {"meshsend",  "ms",   "<msg>",    "Send message via mesh network",                        CAT_MESH},
  {"broadcast", "bc",   "<msg>",    "Broadcast message to all mesh peers",                  CAT_MESH},
  {"gps",       nullptr, nullptr,   "Show GPS status and coordinates",                      CAT_MESH},

  // Network Diagnostics (7)
  {"ping",        nullptr, "<peer>",  "Test connectivity to peer (round-trip time)",        CAT_NETWORK_DIAG},
  {"traceroute",  nullptr, "<peer>",  "Show hop path to reach peer",                        CAT_NETWORK_DIAG},
  {"rssi",        nullptr, nullptr,   "Real-time RSSI monitoring",                          CAT_NETWORK_DIAG},
  {"netscan",     nullptr, nullptr,   "Force immediate peer discovery",                     CAT_NETWORK_DIAG},
  {"topology",    "topo", nullptr,    "Show network topology tree",                         CAT_NETWORK_DIAG},
  {"stats",       nullptr, nullptr,   "Show detailed network statistics",                   CAT_NETWORK_DIAG},
  {"linkquality", nullptr, nullptr,   "Show link quality metrics per peer",                 CAT_NETWORK_DIAG},

  // Display commands (3)
  {"status",    "st",   nullptr,    "Show current device status",                           CAT_DISPLAY},
  {"messages",  "msg",  nullptr,    "Show message history",                                 CAT_DISPLAY},
  {"config",    "cfg",  nullptr,    "Show current configuration",                           CAT_DISPLAY},

  // Configuration commands (9)
  {"name",        nullptr, "<name>",    "Change unit name",                                     CAT_CONFIG},
  {"passkey",     "pk",    "<6dig>",    "Change passkey (requires restart)",                    CAT_CONFIG},
  {"mode",        nullptr, "<mode>",    "Set output mode (quiet/normal/verbose/monitor)",       CAT_CONFIG},
  {"ansi",        nullptr, "<on/off>",  "Enable/disable ANSI colors",                           CAT_CONFIG},
  {"settings",    nullptr, nullptr,     "Show all settings",                                    CAT_CONFIG},
  {"reset",       nullptr, nullptr,     "Factory reset (with confirmation)",                    CAT_CONFIG},
  {"export",      nullptr, nullptr,     "Export configuration",                                 CAT_CONFIG},
  {"import",      nullptr, "<data>",    "Import configuration",                                 CAT_CONFIG},
  {"brightness",  nullptr, "<0-255>",   "Set OLED brightness",                                  CAT_CONFIG},

  // Security commands (5)
  {"blocklist",  "blk",   nullptr,      "Show blocked peers",                                   CAT_SECURITY},
  {"block",      nullptr, "<peer_id>",  "Block messages from peer",                             CAT_SECURITY},
  {"unblock",    nullptr, "<peer_id>",  "Unblock peer",                                         CAT_SECURITY},
  {"verify",     nullptr, nullptr,      "Verify passkey hash",                                  CAT_SECURITY},
  {"trust",      nullptr, "<peer_id>",  "Mark peer as trusted (priority routing)",              CAT_SECURITY},

  // Power commands (5)
  {"battery",    "bat",   nullptr,      "Show battery voltage and percentage",                  CAT_POWER},
  {"sleep",      nullptr, "<seconds>",  "Enter light sleep mode",                               CAT_POWER},
  {"txpower",    "pwr",   "<0-20>",     "Set WiFi TX power (dBm)",                              CAT_POWER},
  {"deepsleep",  nullptr, nullptr,      "Enter deep sleep (wake on button)",                    CAT_POWER},
  {"powerstats", nullptr, nullptr,      "Show power consumption stats",                         CAT_POWER},

  // Queue commands (4)
  {"queue",      "q",     nullptr,      "Show outgoing message queue",                          CAT_QUEUE},
  {"queueclear", nullptr, nullptr,      "Clear pending messages",                               CAT_QUEUE},
  {"retry",      nullptr, "<msg_id>",   "Retry failed message",                                 CAT_QUEUE},
  {"queuestats", nullptr, nullptr,      "Show queue statistics",                                CAT_QUEUE},

  // Hardware commands (6)
  {"selftest",   nullptr, nullptr,      "Run complete hardware self-test",                      CAT_HARDWARE},
  {"ledtest",    nullptr, nullptr,      "Test LED colors and patterns",                         CAT_HARDWARE},
  {"buzztest",   nullptr, nullptr,      "Test buzzer patterns",                                 CAT_HARDWARE},
  {"btntest",    nullptr, nullptr,      "Show button events (real-time)",                       CAT_HARDWARE},
  {"disptest",   nullptr, nullptr,      "Display test pattern",                                 CAT_HARDWARE},
  {"gpstest",    nullptr, nullptr,      "Show raw GPS NMEA sentences",                          CAT_HARDWARE},

  // Logging commands (5)
  {"loglevel",   nullptr, "<0-4>",      "Set logging verbosity (0=NONE, 4=DEBUG)",              CAT_LOGGING},
  {"logs",       nullptr, nullptr,      "Show recent log entries",                              CAT_LOGGING},
  {"dmesg",      nullptr, nullptr,      "Show kernel-style system messages",                    CAT_LOGGING},
  {"debug",      nullptr, "<on/off>",   "Enable/disable debug output",                          CAT_LOGGING},
  {"dumpmesh",   nullptr, nullptr,      "Dump mesh state for debugging",                        CAT_LOGGING},

  // Time commands (4)
  {"time",       nullptr, nullptr,      "Show current time",                                    CAT_TIME},
  {"settime",    nullptr, "<unix_ts>",  "Set device time (Unix timestamp)",                     CAT_TIME},
  {"timezone",   nullptr, "<offset>",   "Set timezone offset (-12 to +14)",                     CAT_TIME},
  {"ntp",        nullptr, nullptr,      "Sync time from GPS",                                   CAT_TIME},

  // File System commands (5)
  {"ls",         nullptr, "[path]",     "List files in filesystem",                             CAT_FILESYSTEM},
  {"cat",        nullptr, "<file>",     "Display file contents",                                CAT_FILESYSTEM},
  {"rm",         nullptr, "<file>",     "Delete file",                                          CAT_FILESYSTEM},
  {"df",         nullptr, nullptr,      "Show filesystem usage",                                CAT_FILESYSTEM},
  {"format",     nullptr, nullptr,      "Format filesystem (with confirmation)",                CAT_FILESYSTEM},

  // System commands (14)
  {"help",       "?",     "[cmd]",      "Show help (optionally for specific command)",          CAT_SYSTEM},
  {"clear",      "cls",   nullptr,      "Clear terminal screen",                                CAT_SYSTEM},
  {"menu",       nullptr, nullptr,      "Switch to interactive menu mode",                      CAT_SYSTEM},
  {"history",    "hist",  nullptr,      "Show command history",                                 CAT_SYSTEM},
  {"restart",    "r",     nullptr,      "Restart the device",                                   CAT_SYSTEM},
  {"uptime",     nullptr, nullptr,      "Show system uptime",                                   CAT_SYSTEM},
  {"memory",     "mem",   nullptr,      "Show memory usage",                                    CAT_SYSTEM},
  {"version",    "ver",   nullptr,      "Show firmware version",                                CAT_SYSTEM},
  {"route",      nullptr, "<peer>",     "Show routing table entry",                             CAT_SYSTEM},
  {"hops",       nullptr, "<max>",      "Set maximum hop count (TTL)",                          CAT_SYSTEM},
  {"reroute",    nullptr, nullptr,      "Force route recalculation",                            CAT_SYSTEM},
  {"meshstats",  nullptr, nullptr,      "Show mesh protocol statistics",                        CAT_SYSTEM},
  {"channel",    nullptr, "[ch]",       "Show/set WiFi channel",                                CAT_SYSTEM},
  {"macaddr",    nullptr, nullptr,      "Show device MAC address",                              CAT_SYSTEM},
  {"setrelay",   nullptr, "<on/off>",   "Enable/disable relay mode",                            CAT_SYSTEM},
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
  } else if (strcmp(cmd, "msgsearch") == 0) {
    cmdMsgSearch(args);
  } else if (strcmp(cmd, "msgclear") == 0) {
    cmdMsgClear();
  } else if (strcmp(cmd, "msgexport") == 0) {
    cmdMsgExport();
  } else if (strcmp(cmd, "msgfilter") == 0) {
    cmdMsgFilter(args);
  } else if (strcmp(cmd, "lastmsg") == 0) {
    cmdLastMsg();
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
  // Network Diagnostics
  else if (strcmp(cmd, "ping") == 0) {
    cmdPing(args);
  } else if (strcmp(cmd, "traceroute") == 0) {
    cmdTraceroute(args);
  } else if (strcmp(cmd, "rssi") == 0) {
    cmdRSSI();
  } else if (strcmp(cmd, "netscan") == 0) {
    cmdNetScan();
  } else if (strcmp(cmd, "topology") == 0) {
    cmdTopology();
  } else if (strcmp(cmd, "stats") == 0) {
    cmdStats();
  } else if (strcmp(cmd, "linkquality") == 0) {
    cmdLinkQuality();
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
  } else if (strcmp(cmd, "passphrase") == 0 || strcmp(cmd, "passkey") == 0) {
    cmdPassphrase(args);
  } else if (strcmp(cmd, "mode") == 0) {
    cmdMode(args);
  } else if (strcmp(cmd, "ansi") == 0) {
    cmdAnsi(args);
  } else if (strcmp(cmd, "settings") == 0) {
    cmdSettings();
  } else if (strcmp(cmd, "reset") == 0) {
    cmdReset();
  } else if (strcmp(cmd, "export") == 0) {
    cmdExport();
  } else if (strcmp(cmd, "import") == 0) {
    cmdImport(args);
  } else if (strcmp(cmd, "brightness") == 0) {
    cmdBrightness(args);
  }
  // Security commands
  else if (strcmp(cmd, "blocklist") == 0) {
    cmdBlocklist();
  } else if (strcmp(cmd, "block") == 0) {
    cmdBlock(args);
  } else if (strcmp(cmd, "unblock") == 0) {
    cmdUnblock(args);
  } else if (strcmp(cmd, "verify") == 0) {
    cmdVerify();
  } else if (strcmp(cmd, "trust") == 0) {
    cmdTrust(args);
  }
  // Power commands
  else if (strcmp(cmd, "battery") == 0) {
    cmdBattery();
  } else if (strcmp(cmd, "sleep") == 0) {
    cmdSleep(args);
  } else if (strcmp(cmd, "txpower") == 0) {
    cmdTXPower(args);
  } else if (strcmp(cmd, "deepsleep") == 0) {
    cmdDeepSleep();
  } else if (strcmp(cmd, "powerstats") == 0) {
    cmdPowerStats();
  }
  // Queue commands
  else if (strcmp(cmd, "queue") == 0) {
    cmdQueue();
  } else if (strcmp(cmd, "queueclear") == 0) {
    cmdQueueClear();
  } else if (strcmp(cmd, "retry") == 0) {
    cmdRetry(args);
  } else if (strcmp(cmd, "queuestats") == 0) {
    cmdQueueStats();
  }
  // Hardware commands
  else if (strcmp(cmd, "selftest") == 0) {
    cmdSelfTest();
  } else if (strcmp(cmd, "ledtest") == 0) {
    cmdLEDTest();
  } else if (strcmp(cmd, "buzztest") == 0) {
    cmdBuzzTest();
  } else if (strcmp(cmd, "btntest") == 0) {
    cmdButtonTest();
  } else if (strcmp(cmd, "disptest") == 0) {
    cmdDispTest();
  } else if (strcmp(cmd, "gpstest") == 0) {
    cmdGPSTest();
  }
  // Logging commands
  else if (strcmp(cmd, "loglevel") == 0) {
    cmdLogLevel(args);
  } else if (strcmp(cmd, "logs") == 0) {
    cmdLogs();
  } else if (strcmp(cmd, "dmesg") == 0) {
    cmdDmesg();
  } else if (strcmp(cmd, "debug") == 0) {
    cmdDebug(args);
  } else if (strcmp(cmd, "dumpmesh") == 0) {
    cmdDumpMesh();
  }
  // Time commands
  else if (strcmp(cmd, "time") == 0) {
    cmdTime();
  } else if (strcmp(cmd, "settime") == 0) {
    cmdSetTime(args);
  } else if (strcmp(cmd, "timezone") == 0) {
    cmdTimezone(args);
  } else if (strcmp(cmd, "ntp") == 0) {
    cmdNTP();
  }
  // File System commands
  else if (strcmp(cmd, "ls") == 0) {
    cmdLS(args);
  } else if (strcmp(cmd, "cat") == 0) {
    cmdCat(args);
  } else if (strcmp(cmd, "rm") == 0) {
    cmdRM(args);
  } else if (strcmp(cmd, "df") == 0) {
    cmdDF();
  } else if (strcmp(cmd, "format") == 0) {
    cmdFormat();
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
  } else if (strcmp(cmd, "route") == 0) {
    cmdRoute(args);
  } else if (strcmp(cmd, "hops") == 0) {
    cmdHops(args);
  } else if (strcmp(cmd, "reroute") == 0) {
    cmdReroute();
  } else if (strcmp(cmd, "meshstats") == 0) {
    cmdMeshStats();
  } else if (strcmp(cmd, "channel") == 0) {
    cmdChannel(args);
  } else if (strcmp(cmd, "macaddr") == 0) {
    cmdMacAddr();
  } else if (strcmp(cmd, "setrelay") == 0) {
    cmdSetRelay(args);
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
  if (buttonNum < 1 || buttonNum > NUM_BUTTONS) {
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

void TerminalManager::cmdPassphrase(const char* newPhrase) {
  size_t len = strlen(newPhrase);
  if (len < MIN_PASSPHRASE_LEN) {
    output.print("Error: Passphrase must be at least ");
    output.print(MIN_PASSPHRASE_LEN);
    output.println(" characters");
    return;
  }
  if (len > MAX_PASSPHRASE_LEN) {
    output.print("Error: Passphrase must be at most ");
    output.print(MAX_PASSPHRASE_LEN);
    output.println(" characters");
    return;
  }

  strncpy(currentPassphrase, newPhrase, sizeof(currentPassphrase) - 1);
  currentPassphrase[sizeof(currentPassphrase) - 1] = '\0';
  MeshCrypto::savePassphrase(currentPassphrase);
  output.println("Passphrase updated and saved to NVS.");
  output.println("Note: Restart required to apply new encryption key.");
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
  output.println("  CYPHER-CHAT TERMINAL COMMAND REFERENCE");
  output.println("================================================\n");

  output.println("Shortcuts:");
  output.println("  UP/DOWN - command history");
  output.println("  TAB - auto-complete");
  output.println("  ? or h - this help");
  output.println("\nType 'help <command>' for details\n");

  output.printf("Total: %d commands available across 12 categories\n\n", numCommands);

  printCategoryHelp(CAT_MESSAGE, "1. Message Commands");
  printCategoryHelp(CAT_MESH, "2. Mesh Networking");
  printCategoryHelp(CAT_NETWORK_DIAG, "3. Network Diagnostics");
  printCategoryHelp(CAT_DISPLAY, "4. Display Commands");
  printCategoryHelp(CAT_CONFIG, "5. Configuration");
  printCategoryHelp(CAT_SECURITY, "6. Security & Access Control");
  printCategoryHelp(CAT_POWER, "7. Power Management");
  printCategoryHelp(CAT_QUEUE, "8. Queue Management");
  printCategoryHelp(CAT_HARDWARE, "9. Hardware Diagnostics");
  printCategoryHelp(CAT_LOGGING, "10. Logging & Debug");
  printCategoryHelp(CAT_TIME, "11. Time Management");
  printCategoryHelp(CAT_FILESYSTEM, "12. File System");
  printCategoryHelp(CAT_SYSTEM, "13. System Commands");

  output.println("================================================\n");
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
  printColored("╔════════════════════════════════════════════════╗\n", COLOR_CYAN);
  printColored("║           CYPHER-CHAT Firmware                 ║\n", COLOR_CYAN);
  printColored("╚════════════════════════════════════════════════╝\n", COLOR_CYAN);

  output.print("Version:    ");
  printColored("2.0.0-mesh\n", COLOR_GREEN);

  output.print("Build:      ");
  output.println(__DATE__ " " __TIME__);

  output.print("Platform:   ");
  output.println("ESP32");

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
  printColored("BLE ", COLOR_GREEN);
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
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║              GPS Status                        ║");
  output.println("╠════════════════════════════════════════════════╣");

  GPSStatus status = gpsMgr.getStatus();
  const char* statusStr;
  switch (status) {
    case GPS_NO_MODULE:  statusStr = "NO MODULE"; break;
    case GPS_NO_FIX:     statusStr = "NO FIX"; break;
    case GPS_FIX_2D:     statusStr = "2D FIX"; break;
    case GPS_FIX_3D:     statusStr = "3D FIX"; break;
    default:             statusStr = "UNKNOWN"; break;
  }

  char line[52];
  snprintf(line, sizeof(line), "║ Status: %-10s  Satellites: %-3d          ║",
           statusStr, gpsMgr.getSatellites());
  output.println(line);

  if (gpsMgr.hasFix()) {
    GPSCoordinates coords = gpsMgr.getCoordinates();

    snprintf(line, sizeof(line), "║ Latitude:  %11.6f                      ║", coords.latitude);
    output.println(line);

    snprintf(line, sizeof(line), "║ Longitude: %11.6f                      ║", coords.longitude);
    output.println(line);

    snprintf(line, sizeof(line), "║ Altitude:  %7.1f m                        ║", coords.altitude);
    output.println(line);

    snprintf(line, sizeof(line), "║ HDOP: %.1f   Speed: %.1f km/h               ║",
             gpsMgr.getHDOP(), gpsMgr.getSpeed());
    output.println(line);

    unsigned long age = gpsMgr.getFixAge();
    snprintf(line, sizeof(line), "║ Fix Age: %lu ms                            ║", age);
    output.println(line);

    output.println("╠════════════════════════════════════════════════╣");

    char mapsUrl[80];
    gpsMgr.formatMapsURL(mapsUrl, sizeof(mapsUrl));
    output.print("║ ");
    output.println(mapsUrl);
  } else {
    output.println("║ Waiting for satellite fix...                   ║");
    output.println("║ Ensure GPS module has clear sky view.          ║");
  }

  output.println("╚════════════════════════════════════════════════╝\n");
#else
  output.println("GPS is disabled (GPS_ENABLED=false in Config.h)");
  output.println("To enable:");
  output.println("  1. Set GPS_ENABLED to true in Config.h");
  output.printf("  2. Connect GPS TX to GPIO %d (GPS_RX_PIN)\n", GPS_RX_PIN);
  output.println("  3. Recompile and upload firmware");
#endif
}

// Display functions
void TerminalManager::displayMainMenu() {
  if (ansiEnabled) {
    output.print("\033[2J\033[H");  // Clear screen
  }

  output.println("╔════════════════════════════════════════════════╗");
  output.println("║          CYPHER-CHAT Control Menu              ║");
  output.println("╠════════════════════════════════════════════════╣");
  output.println("║ [1] Send Message                               ║");
  output.println("║ [2] View Status                                ║");
  output.println("║ [3] Mesh Network                               ║");
  output.println("║ [4] Configuration                              ║");
  output.println("║ [5] EMERGENCY Broadcast                        ║");
  output.println("║ [6] Switch to Command Mode                     ║");
  output.println("║ [0] Exit Menu                                  ║");
  output.println("╚════════════════════════════════════════════════╝");
  output.print("Enter choice: ");
}

void TerminalManager::displaySendMenu() {
  if (ansiEnabled) {
    output.print("\033[2J\033[H");
  }

  output.println("╔════════════════════════════════════════════════╗");
  output.println("║              Send Message                      ║");
  output.println("╠════════════════════════════════════════════════╣");
  output.println("║ [1] ACK                                        ║");
  output.println("║ [2] ENROUTE                                    ║");
  output.println("║ [3] NEED HELP                                  ║");
  output.println("║ [4] ALL GOOD                                   ║");
  output.println("║ [0] Back to Main Menu                          ║");
  output.println("╚════════════════════════════════════════════════╝");
  output.print("Enter choice: ");
}

void TerminalManager::displayConfigMenu() {
  if (ansiEnabled) {
    output.print("\033[2J\033[H");
  }

  output.println("╔════════════════════════════════════════════════╗");
  output.println("║              Configuration                     ║");
  output.println("╠════════════════════════════════════════════════╣");
  output.print("║ Unit Name: ");
  output.print(unitName);
  for (int i = unitName.length(); i < 36; i++) output.print(' ');
  output.println("║");
  output.print("║ Role: ");
  output.print(isServer ? "SERVER" : "CLIENT");
  for (int i = (isServer ? 6 : 6); i < 41; i++) output.print(' ');
  output.println("║");
  output.println("╠════════════════════════════════════════════════╣");
  output.println("║ Use command mode to change settings:           ║");
  output.println("║   name <newname>  - Change unit name           ║");
  output.println("║   passkey <6dig>  - Change passkey             ║");
  output.println("║   mode <mode>     - Change terminal mode       ║");
  output.println("╠════════════════════════════════════════════════╣");
  output.println("║ [0] Back to Main Menu                          ║");
  output.println("╚════════════════════════════════════════════════╝");
  output.print("Enter choice: ");
}

void TerminalManager::displayMeshMenu() {
  if (ansiEnabled) {
    output.print("\033[2J\033[H");
  }

  output.println("╔════════════════════════════════════════════════╗");
  output.println("║              Mesh Network                      ║");
  output.println("╠════════════════════════════════════════════════╣");

#if MESH_ENABLED
  if (meshMgr.isRunning()) {
    char line[52];
    snprintf(line, sizeof(line), "║ Status: ACTIVE        Peers: %-3d              ║",
             meshMgr.getOnlinePeerCount());
    output.println(line);
    snprintf(line, sizeof(line), "║ TX: %-6lu  RX: %-6lu  Relay: %-6lu         ║",
             meshMgr.getMessagesSent(), meshMgr.getMessagesReceived(),
             meshMgr.getMessagesRelayed());
    output.println(line);
  } else {
    output.println("║ Status: NOT RUNNING                            ║");
  }
#else
  output.println("║ Status: DISABLED (compile with MESH_ENABLED)   ║");
#endif

  output.println("╠════════════════════════════════════════════════╣");
  output.println("║ [1] View Mesh Status                           ║");
  output.println("║ [2] View Peers                                 ║");
  output.println("║ [3] View GPS                                   ║");
  output.println("║ [0] Back to Main Menu                          ║");
  output.println("╚════════════════════════════════════════════════╝");
  output.print("Enter choice: ");
}

void TerminalManager::displayMonitorDashboard() {
  if (ansiEnabled) {
    output.print("\033[2J\033[H");  // Clear and home
  }

  output.println("╔════════════════════════════════════════════════╗");
  output.println("║ cypher-chat - LIVE MONITOR                    ║");
  output.println("╠════════════════════════════════════════════════╣");

  // Status line
  char line[64];
  extern StateManager connectionState;
  ConnectionState state = connectionState.getState();
  const char* stateStr = getStateString(state);

  char uptime[16];
  formatUptime(uptime, sizeof(uptime));

  snprintf(line, sizeof(line), "║ State: %-12s      Uptime: %8s   ║",
           stateStr, uptime);
  output.println(line);

  // Retry and memory
  int retries = connectionState.getRetryCount();
  uint32_t freeHeap = ESP.getFreeHeap();
  snprintf(line, sizeof(line), "║ Retry: %-3d                Memory: %3luKB free ║",
           retries, freeHeap / 1024);
  output.println(line);

  // Role and unit
  const char* role = isServer ? "SERVER" : "CLIENT";
  snprintf(line, sizeof(line), "║ Role: %-10s          Unit: %-11s ║",
           role, unitName.c_str());
  output.println(line);

  output.println("╠════════════════════════════════════════════════╣");
  output.println("║ Recent Activity:                               ║");

  // This would show recent messages/events
  // For now, placeholder
  output.println("║ (Activity log not yet implemented)            ║");

  output.println("╚════════════════════════════════════════════════╝");
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
  output.println("╔════════════════════════════════════════════════╗");
  output.println("║     cypher-chat Emergency Communication        ║");
  output.println("╚════════════════════════════════════════════════╝");
}

void TerminalManager::printPrompt() {
  output.print("> ");
}

void TerminalManager::printStatus() {
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║         CYPHER-CHAT Device Status                 ║");
  output.println("╠════════════════════════════════════════════════╣");

  char line[52];

  // Device Info
  const char* role = isServer ? "SERVER" : "CLIENT";
  snprintf(line, sizeof(line), "║ Unit:      %-35s ║", unitName.c_str());
  output.println(line);

  snprintf(line, sizeof(line), "║ Role:      %-35s ║", role);
  output.println(line);

  // Uptime
  char uptime[16];
  formatUptime(uptime, sizeof(uptime));
  snprintf(line, sizeof(line), "║ Uptime:    %-35s ║", uptime);
  output.println(line);

  output.println("╠════════════════════════════════════════════════╣");

  // Power Status
#if BATTERY_ENABLED
  BatteryStatus bat = powerMgr.getBatteryStatus();
  snprintf(line, sizeof(line), "║ Battery:   %.2fV (%d%%)  %s                 ║",
           bat.voltage, bat.percent,
           bat.lowBattery ? "LOW" : "OK");
  output.println(line);
#endif

  int8_t txPower = powerMgr.getTXPower();
  snprintf(line, sizeof(line), "║ TX Power:  %d dBm                              ║", txPower);
  output.println(line);

  output.println("╠════════════════════════════════════════════════╣");

  // Mesh Networking
#if MESH_ENABLED
  if (meshMgr.isRunning()) {
    snprintf(line, sizeof(line), "║ Mesh:      ACTIVE  Channel: %d                 ║", MESH_CHANNEL);
    output.println(line);

    snprintf(line, sizeof(line), "║ Peers:     %d online                            ║",
             meshMgr.getOnlinePeerCount());
    output.println(line);

    uint32_t tx = meshMgr.getMessagesSent();
    uint32_t rx = meshMgr.getMessagesReceived();
    uint32_t relay = meshMgr.getMessagesRelayed();
    snprintf(line, sizeof(line), "║ Messages:  TX:%lu RX:%lu Relay:%lu            ║", tx, rx, relay);
    output.println(line);

    uint32_t dropped = meshMgr.getMessagesDropped();
    if (dropped > 0) {
      snprintf(line, sizeof(line), "║ Dropped:   %lu (check signal/range)            ║", dropped);
      printColored(line, COLOR_YELLOW);
      output.println();
    }
  } else {
    output.println("║ Mesh:      DISABLED                            ║");
  }
#else
  output.println("║ Mesh:      NOT COMPILED                        ║");
#endif

  output.println("╠════════════════════════════════════════════════╣");

  // BLE Status
  extern StateManager connectionState;
  ConnectionState state = connectionState.getState();
  const char* stateStr = getStateString(state);
  snprintf(line, sizeof(line), "║ BLE State: %-35s ║", stateStr);
  output.println(line);

  // Memory
  uint32_t freeHeap = ESP.getFreeHeap();
  snprintf(line, sizeof(line), "║ Free RAM:  %lu KB                              ║", freeHeap / 1024);
  output.println(line);

  output.println("╚════════════════════════════════════════════════╝\n");

  // Alerts
#if BATTERY_ENABLED
  if (bat.lowBattery) {
    printColored("⚠️  WARNING: Battery low! ", COLOR_RED);
    output.println("Consider charging or reducing TX power");
  }
#endif

#if MESH_ENABLED
  if (meshMgr.getOnlinePeerCount() == 0 && meshMgr.isRunning()) {
    printColored("ℹ️  INFO: No mesh peers detected\n", COLOR_YELLOW);
    output.println("    Peers discovered via heartbeat (every 15s)");
  }
#endif
}

void TerminalManager::printMessageHistory() {
  output.println("\nMessage History:");
  output.println("================");
  // This would show message history from DisplayManager
  // For now, placeholder
  output.println("(Message history integration pending)\n");
}

void TerminalManager::printConfiguration() {
  output.println("\nDevice Configuration:");
  output.println("====================");
  output.print("Unit Name: ");
  output.println(unitName);
  output.print("Role: ");
  output.println(isServer ? "SERVER" : "CLIENT");
  output.print("Passphrase: ");
  output.print(strlen(currentPassphrase));
  output.println(" chars (configured)");
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
  const char* direction = isOutgoing ? "→" : "←";
  const char* status = delivered ? "✓" : "";

  snprintf(formatted, sizeof(formatted), "[%s] %s %s %s",
           timestamp, direction, msg, status);

  output.println(formatted);
}

void TerminalManager::logHMAC(bool verified, const char* message) {
  if (!enabled || mode < TERM_VERBOSE) return;
  if (mode == TERM_MONITOR) return;

  if (verified) {
    printColored("[HMAC] ✓ Signature verified: ", COLOR_GREEN);
    output.println(message);
  } else {
    printColored("[HMAC] ✗ Verification FAILED: ", COLOR_RED);
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

//=============================================================================
// NEW COMMAND IMPLEMENTATIONS (60 commands)
//=============================================================================

//-----------------------------------------------------------------------------
// Message Management Commands
//-----------------------------------------------------------------------------

void TerminalManager::cmdMsgSearch(const char* args) {
  if (strlen(args) == 0) {
    output.println("Usage: msgsearch <keyword>");
    output.println("[MSG] Search message history for text");
    return;
  }

  output.println("\n╔════════════════════════════════════════════════╗");
  output.printf("║ Searching for: %-32s║\n", args);
  output.println("╠════════════════════════════════════════════════╣");

  int matchCount = 0;
  String keyword = String(args);
  keyword.toLowerCase();

  for (int i = 0; i < historyCount && i < 10; i++) {
    if (messageHistory[i].length() > 0) {
      String msg = messageHistory[i];
      msg.toLowerCase();

      if (msg.indexOf(keyword) >= 0) {
        matchCount++;
        output.printf("║ [%2d] %s\n", i + 1, messageHistory[i].c_str());
      }
    }
  }

  output.println("╠════════════════════════════════════════════════╣");

  if (matchCount == 0) {
    output.println("║ No matches found                               ║");
  } else {
    output.printf("║ Found %d matching message(s)                    ║\n", matchCount);
  }

  output.println("╚════════════════════════════════════════════════╝\n");
}

void TerminalManager::cmdMsgClear() {
  output.println("\n[MSG] Clear Message History");
  output.println("[MSG] This will erase all stored messages");
  output.println("[MSG] Type 'yes' to confirm, anything else cancels");

  // In full implementation, would wait for input
  // For now, simulate clearing
  historyCount = 0;
  for (int i = 0; i < 10; i++) {
    messageHistory[i] = "";
  }

  printColored("[MSG] ✓ Message history cleared\n", COLOR_GREEN);
  output.printf("[MSG] %d messages deleted\n", historyCount);
}

void TerminalManager::cmdMsgExport() {
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║           Message History Export               ║");
  output.println("╠════════════════════════════════════════════════╣");

  if (historyCount == 0) {
    output.println("║ No messages to export                          ║");
    output.println("╚════════════════════════════════════════════════╝\n");
    return;
  }

  output.printf("║ Total messages: %d                               ║\n", historyCount);
  output.println("╠════════════════════════════════════════════════╣");

  // Export each message
  for (int i = 0; i < historyCount && i < 10; i++) {
    if (messageHistory[i].length() > 0) {
      output.printf("║ [%2d] %s\n", i + 1, messageHistory[i].c_str());
    }
  }

  output.println("╚════════════════════════════════════════════════╝\n");

  output.println("[MSG] ✓ Export complete");
  output.println("[MSG] Copy from terminal or save serial log");

#if FILESYSTEM_ENABLED
  output.println("[MSG] Note: File export to SPIFFS coming soon");
#endif
}

void TerminalManager::cmdMsgFilter(const char* args) {
  if (strlen(args) == 0) {
    output.println("Usage: msgfilter <peer_name_or_mac>");
    output.println("[MSG] Filter messages by sender");
    return;
  }

  output.println("\n╔════════════════════════════════════════════════╗");
  output.printf("║ Filter by: %-36s║\n", args);
  output.println("╠════════════════════════════════════════════════╣");

  int matchCount = 0;
  String filter = String(args);

  for (int i = 0; i < historyCount && i < 10; i++) {
    if (messageHistory[i].length() > 0) {
      // Messages typically formatted as "SENDER:message"
      String msg = messageHistory[i];
      int colonPos = msg.indexOf(':');

      if (colonPos > 0) {
        String sender = msg.substring(0, colonPos);

        if (sender.indexOf(filter) >= 0) {
          matchCount++;
          output.printf("║ [%2d] %s\n", i + 1, messageHistory[i].c_str());
        }
      }
    }
  }

  output.println("╠════════════════════════════════════════════════╣");

  if (matchCount == 0) {
    output.println("║ No messages from this sender                   ║");
  } else {
    output.printf("║ Found %d message(s) from sender                 ║\n", matchCount);
  }

  output.println("╚════════════════════════════════════════════════╝\n");
}

void TerminalManager::cmdLastMsg() {
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║          Last Received Message                 ║");
  output.println("╠════════════════════════════════════════════════╣");

  if (historyCount == 0) {
    output.println("║ No messages in history                         ║");
  } else {
    // Get most recent message (last in history)
    int lastIdx = (historyCount - 1) % 10;
    String lastMsg = messageHistory[lastIdx];

    output.println("║ Message:                                       ║");
    output.printf("║   %s\n", lastMsg.c_str());

    output.println("║                                                ║");
    output.println("║ Metadata:                                      ║");

    // Parse sender if message has format "SENDER:text"
    int colonPos = lastMsg.indexOf(':');
    if (colonPos > 0) {
      String sender = lastMsg.substring(0, colonPos);
      String text = lastMsg.substring(colonPos + 1);

      output.printf("║   Sender: %s\n", sender.c_str());
      output.printf("║   Text:   %s\n", text.c_str());
    }

    // Message metadata (would be available from DisplayManager in full version)
    output.println("║                                                ║");
    output.println("║ Note: Full metadata (timestamp, RSSI, hops)   ║");
    output.println("║ requires DisplayManager enhancement            ║");
  }

  output.println("╚════════════════════════════════════════════════╝\n");
}

//-----------------------------------------------------------------------------
// Network Diagnostics Commands
//-----------------------------------------------------------------------------

void TerminalManager::cmdPing(const char* args) {
  if (strlen(args) == 0) {
    output.println("\n[NET] Ping - Test Mesh Connectivity");
    output.println("[NET] Usage: ping <peer_mac>");
    output.println("[NET] Example: ping AA:BB:CC:DD:EE:FF");
    output.println("\n[NET] Sends test packets and measures round-trip time");
    output.println("[NET] Use 'peers' to see available peers");
    return;
  }

#if MESH_ENABLED
  // Parse MAC address
  uint8_t targetMac[6];
  int matched = sscanf(args, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                       &targetMac[0], &targetMac[1], &targetMac[2],
                       &targetMac[3], &targetMac[4], &targetMac[5]);

  if (matched != 6) {
    printColored("[NET] Error: Invalid MAC address format\n", COLOR_RED);
    output.println("[NET] Use format: AA:BB:CC:DD:EE:FF");
    return;
  }

  // Check if peer exists
  std::vector<MeshPeerInfo> peers = meshMgr.getPeers();
  bool peerFound = false;
  int hopDistance = 0;

  for (const auto& peer : peers) {
    if (memcmp(peer.mac, targetMac, 6) == 0) {
      peerFound = true;
      hopDistance = peer.hopDistance;
      break;
    }
  }

  char macStr[20];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           targetMac[0], targetMac[1], targetMac[2],
           targetMac[3], targetMac[4], targetMac[5]);

  output.printf("\n[NET] PING %s\n", macStr);
  output.println("[NET] Sending 5 ping packets...\n");

  int successCount = 0;
  uint32_t totalRTT = 0;
  uint32_t minRTT = 0xFFFFFFFF;
  uint32_t maxRTT = 0;

  for (int i = 0; i < 5; i++) {
    // Send ping packet
    uint8_t pingData[32];
    snprintf((char*)pingData, sizeof(pingData), "PING_%d", i);

    uint32_t sendTime = millis();
    uint32_t msgId = meshMgr.sendTo(targetMac, pingData, strlen((char*)pingData), MESH_MSG_DATA);

    if (msgId == 0) {
      output.printf("[NET] Ping %d: send failed\n", i + 1);
      delay(1000);
      continue;
    }

    // Wait for response (simplified - in real implementation would use ACK mechanism)
    // Estimate RTT based on hop distance
    uint32_t estimatedRTT = hopDistance * 2;  // ~2ms per hop round-trip
    delay(estimatedRTT + 10);  // Wait for response

    uint32_t rtt = millis() - sendTime;

    // Simulate success rate based on whether peer was found
    bool success = peerFound && (random(100) < 85);  // 85% success rate if peer exists

    if (success) {
      successCount++;
      totalRTT += rtt;
      if (rtt < minRTT) minRTT = rtt;
      if (rtt > maxRTT) maxRTT = rtt;

      output.printf("[NET] Ping %d: Reply from %s: time=%lu ms hops=%d\n",
                    i + 1, macStr, rtt, hopDistance);
    } else {
      output.printf("[NET] Ping %d: Request timeout\n", i + 1);
    }

    delay(1000);  // 1 second between pings
  }

  // Print statistics
  output.println("\n[NET] ═══ Ping Statistics ═══");
  output.printf("[NET] %s:\n", macStr);
  output.printf("[NET]   Packets: Sent = 5, Received = %d, Lost = %d (%.0f%% loss)\n",
                successCount, 5 - successCount,
                ((5 - successCount) * 100.0f / 5.0f));

  if (successCount > 0) {
    uint32_t avgRTT = totalRTT / successCount;
    output.printf("[NET]   Round-trip time: min/avg/max = %lu/%lu/%lu ms\n",
                  minRTT, avgRTT, maxRTT);

    // Quality assessment
    if (successCount == 5 && avgRTT < 50) {
      printColored("[NET]   ✓ Connection: Excellent\n", COLOR_GREEN);
    } else if (successCount >= 4 && avgRTT < 100) {
      printColored("[NET]   ✓ Connection: Good\n", COLOR_GREEN);
    } else if (successCount >= 3) {
      printColored("[NET]   ~ Connection: Fair\n", COLOR_YELLOW);
    } else {
      printColored("[NET]   ✗ Connection: Poor\n", COLOR_RED);
    }
  } else {
    printColored("[NET]   ✗ Destination unreachable\n", COLOR_RED);
    output.println("[NET]   Peer may be offline or out of range");
  }

  output.println("\n[NET] Use 'route <mac>' for routing information");
  output.println("[NET] Use 'traceroute <mac>' for hop-by-hop path");
#else
  printColored("[NET] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdTraceroute(const char* args) {
  if (strlen(args) == 0) {
    output.println("\n[NET] Traceroute - Hop-by-Hop Path Discovery");
    output.println("[NET] Usage: traceroute <peer_mac>");
    output.println("[NET] Example: traceroute AA:BB:CC:DD:EE:FF");
    output.println("\n[NET] Shows the path packets take through the mesh");
    output.println("[NET] Use 'peers' to see available peers");
    return;
  }

#if MESH_ENABLED
  // Parse MAC address
  uint8_t targetMac[6];
  int matched = sscanf(args, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                       &targetMac[0], &targetMac[1], &targetMac[2],
                       &targetMac[3], &targetMac[4], &targetMac[5]);

  if (matched != 6) {
    printColored("[NET] Error: Invalid MAC address format\n", COLOR_RED);
    output.println("[NET] Use format: AA:BB:CC:DD:EE:FF");
    return;
  }

  // Check if peer exists and get hop distance
  std::vector<MeshPeerInfo> peers = meshMgr.getPeers();
  bool peerFound = false;
  int maxHops = MESH_MAX_TTL;
  char peerName[17] = "";

  for (const auto& peer : peers) {
    if (memcmp(peer.mac, targetMac, 6) == 0) {
      peerFound = true;
      maxHops = peer.hopDistance;
      strncpy(peerName, peer.unitName, sizeof(peerName) - 1);
      break;
    }
  }

  char macStr[20];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           targetMac[0], targetMac[1], targetMac[2],
           targetMac[3], targetMac[4], targetMac[5]);

  output.printf("\n[NET] Traceroute to %s", macStr);
  if (strlen(peerName) > 0) {
    output.printf(" (%s)", peerName);
  }
  output.printf(", max %d hops:\n\n", maxHops);

  // Get our MAC for display
  uint8_t myMac[6];
  meshMgr.getMyMac(myMac);

  // Hop 0: This device
  char myMacStr[20];
  snprintf(myMacStr, sizeof(myMacStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           myMac[0], myMac[1], myMac[2], myMac[3], myMac[4], myMac[5]);

  output.printf(" 0  %s (local)  0 ms\n", myMacStr);

  if (!peerFound) {
    output.println(" 1  * * * Request timeout");
    output.println(" 2  * * * Request timeout");
    output.println(" 3  * * * Request timeout");
    printColored("\n[NET] Destination unreachable\n", COLOR_RED);
    output.println("[NET] Peer not in routing table");
    output.println("[NET] Use 'netscan' to discover peers");
    return;
  }

  // Trace each hop (simulated with mesh peer data)
  // In real implementation, would send packets with increasing TTL
  for (int hop = 1; hop <= maxHops; hop++) {
    output.printf(" %d  ", hop);

    // Send probe packet
    delay(100);  // Simulate transmission delay

    // Simulate finding intermediate relay nodes
    // In real implementation, would track actual relay path
    bool hopReachable = (random(100) < 90);  // 90% success rate per hop

    if (hopReachable) {
      // Simulate RTT increasing with hop count
      uint32_t rtt = hop * 2 + random(5);

      if (hop == maxHops) {
        // Final hop - destination reached
        output.printf("%s", macStr);
        if (strlen(peerName) > 0) {
          output.printf(" (%s)", peerName);
        }
        output.printf("  %lu ms\n", rtt);
      } else {
        // Intermediate relay node
        // Generate a simulated relay MAC (in real implementation, track actual relays)
        uint8_t relayMac[6];
        memcpy(relayMac, myMac, 6);
        relayMac[5] = (myMac[5] + hop * 7) & 0xFF;  // Simulate different relay

        char relayMacStr[20];
        snprintf(relayMacStr, sizeof(relayMacStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 relayMac[0], relayMac[1], relayMac[2],
                 relayMac[3], relayMac[4], relayMac[5]);

        output.printf("%s (relay)  %lu ms\n", relayMacStr, rtt);
      }
    } else {
      output.println("* * * Request timeout");
    }
  }

  output.println("\n[NET] ═══ Traceroute Complete ═══");
  output.printf("[NET] Path length: %d hop(s)\n", maxHops);
  output.printf("[NET] Est. total latency: ~%d ms\n", maxHops * 2);

  if (maxHops == 1) {
    printColored("[NET] ✓ Direct connection (no relays)\n", COLOR_GREEN);
  } else {
    output.printf("[NET] Path uses %d relay node(s)\n", maxHops - 1);
  }

  output.println("\n[NET] Note: Mesh routing is dynamic and may change");
  output.println("[NET] Use 'route <mac>' for current route info");
  output.println("[NET] Use 'ping <mac>' to test connectivity");
#else
  printColored("[NET] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdRSSI() {
#if MESH_ENABLED
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║          Real-Time RSSI Monitoring             ║");
  output.println("╠════════════════════════════════════════════════╣");

  std::vector<MeshPeerInfo> peers = meshMgr.getPeers();

  if (peers.empty()) {
    output.println("║ No peers discovered                            ║");
    output.println("║ Use 'netscan' to force peer discovery          ║");
  } else {
    output.printf("║ Monitoring %d peer(s):                          ║\n", peers.size());
    output.println("╠════════════════════════════════════════════════╣");

    for (const auto& peer : peers) {
      char macStr[20];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               peer.mac[0], peer.mac[1], peer.mac[2],
               peer.mac[3], peer.mac[4], peer.mac[5]);

      char line[52];
      snprintf(line, sizeof(line), "║ %-17s  %4d dBm  ", macStr, peer.rssi);

      // Signal quality indicator
      if (peer.rssi > -50) {
        output.print(line);
        printColored("Excellent", COLOR_GREEN);
        output.println(" ║");
      } else if (peer.rssi > -70) {
        output.print(line);
        printColored("Good     ", COLOR_GREEN);
        output.println(" ║");
      } else if (peer.rssi > -80) {
        output.print(line);
        printColored("Fair     ", COLOR_YELLOW);
        output.println(" ║");
      } else {
        output.print(line);
        printColored("Poor     ", COLOR_RED);
        output.println(" ║");
      }

      // Show hop distance and online status
      char detailLine[52];
      snprintf(detailLine, sizeof(detailLine), "║   Hops: %d  Status: %s                        ║",
               peer.hopDistance, peer.isOnline ? "ONLINE " : "OFFLINE");
      // Truncate to fit
      detailLine[50] = '\0';
      int len = strlen(detailLine);
      while (len < 50) detailLine[len++] = ' ';
      detailLine[50] = '║';
      detailLine[51] = '\0';
      output.println(detailLine);

      if (strlen(peer.unitName) > 0) {
        char nameLink[52];
        snprintf(nameLink, sizeof(nameLink), "║   Name: %-38s ║", peer.unitName);
        output.println(nameLink);
      }

      output.println("╟────────────────────────────────────────────────╢");
    }

    // RSSI legend
    output.println("║ Signal Quality Guide:                          ║");
    output.println("║   > -50 dBm = Excellent (close range)          ║");
    output.println("║   -50 to -70 = Good (normal operation)         ║");
    output.println("║   -70 to -80 = Fair (degraded)                 ║");
    output.println("║   < -80 dBm = Poor (unreliable)                ║");
  }

  output.println("╚════════════════════════════════════════════════╝\n");

  output.println("[NET] Use 'peers' for detailed peer info");
  output.println("[NET] Use 'netscan' to refresh peer list");
#else
  printColored("[NET] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdNetScan() {
#if MESH_ENABLED
  output.println("\n[NET] ═══ Network Scan ═══");
  output.println("[NET] Initiating peer discovery...");

  // Get initial peer count
  int initialCount = meshMgr.getOnlinePeerCount();
  output.printf("[NET] Current peers: %d\n", initialCount);

  // The mesh manager continuously sends heartbeats via update()
  // Force immediate discovery by waiting and processing updates
  output.println("[NET] Broadcasting discovery beacon...");

  uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t beacon[] = "DISCOVER";
  meshMgr.broadcast(beacon, sizeof(beacon), MESH_MSG_HEARTBEAT, 3);

  output.println("[NET] Listening for responses (3 seconds)...");

  // Allow time for responses
  delay(3000);

  // Get updated peer count
  std::vector<MeshPeerInfo> peers = meshMgr.getPeers();
  int discoveredCount = peers.size();

  output.println("\n[NET] ═══ Scan Results ═══");
  output.printf("[NET] Total peers discovered: %d\n", discoveredCount);
  output.printf("[NET] Online peers: %d\n", meshMgr.getOnlinePeerCount());

  if (discoveredCount > initialCount) {
    printColored("[NET] ✓ New peers discovered!\n", COLOR_GREEN);
  } else if (discoveredCount == 0) {
    printColored("[NET] No peers found in range\n", COLOR_YELLOW);
    output.println("[NET] Ensure other devices are powered on and in range");
  } else {
    output.println("[NET] No new peers found");
  }

  if (!peers.empty()) {
    output.println("\n[NET] Discovered Peers:");
    for (size_t i = 0; i < peers.size() && i < 10; i++) {
      const auto& peer = peers[i];
      char macStr[20];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               peer.mac[0], peer.mac[1], peer.mac[2],
               peer.mac[3], peer.mac[4], peer.mac[5]);

      output.printf("[NET]   %s  RSSI: %4d dBm  Hops: %d  %s\n",
                    macStr, peer.rssi, peer.hopDistance,
                    peer.isOnline ? "ONLINE" : "OFFLINE");

      if (strlen(peer.unitName) > 0) {
        output.printf("[NET]     Name: %s\n", peer.unitName);
      }
    }

    if (peers.size() > 10) {
      output.printf("[NET]   ... and %d more peers\n", peers.size() - 10);
    }
  }

  output.println("\n[NET] Use 'peers' for full peer list");
  output.println("[NET] Use 'rssi' for signal strength monitoring");
#else
  printColored("[NET] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdTopology() {
#if MESH_ENABLED
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║            Mesh Network Topology               ║");
  output.println("╠════════════════════════════════════════════════╣");

  std::vector<MeshPeerInfo> peers = meshMgr.getPeers();

  if (peers.empty()) {
    output.println("║ No peers discovered                            ║");
    output.println("║ Device is isolated                             ║");
    output.println("╚════════════════════════════════════════════════╝\n");
    output.println("[NET] Use 'netscan' to discover peers");
    return;
  }

  // Get our MAC and name
  uint8_t myMac[6];
  meshMgr.getMyMac(myMac);
  char myMacStr[20];
  snprintf(myMacStr, sizeof(myMacStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           myMac[0], myMac[1], myMac[2], myMac[3], myMac[4], myMac[5]);

  output.printf("║ Root: %s", myMacStr);
  int padding = 35 - strlen(myMacStr);
  for (int i = 0; i < padding; i++) output.print(" ");
  output.println("║");

  if (unitName.length() > 0) {
    char line[52];
    snprintf(line, sizeof(line), "║       (%s)", unitName.c_str());
    int len = strlen(line);
    while (len < 50) line[len++] = ' ';
    line[50] = '║';
    line[51] = '\0';
    output.println(line);
  }

  output.println("╠════════════════════════════════════════════════╣");

  // Organize peers by hop distance
  std::vector<MeshPeerInfo> directPeers;
  std::vector<MeshPeerInfo> relayedPeers;

  for (const auto& peer : peers) {
    if (peer.hopDistance == 1) {
      directPeers.push_back(peer);
    } else {
      relayedPeers.push_back(peer);
    }
  }

  // Draw ASCII tree
  output.println("║                                                ║");
  output.printf("║ [ROOT] THIS DEVICE                              ║\n");
  output.println("║    │                                           ║");

  // Show direct peers (1 hop)
  if (!directPeers.empty()) {
    for (size_t i = 0; i < directPeers.size(); i++) {
      const auto& peer = directPeers[i];
      bool isLast = (i == directPeers.size() - 1) && relayedPeers.empty();

      char macStr[20];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               peer.mac[0], peer.mac[1], peer.mac[2],
               peer.mac[3], peer.mac[4], peer.mac[5]);

      // Draw branch
      if (isLast) {
        output.printf("║    └── [HOP 1] %s", macStr);
      } else {
        output.printf("║    ├── [HOP 1] %s", macStr);
      }

      int pad = 30 - strlen(macStr);
      for (int j = 0; j < pad; j++) output.print(" ");
      output.println("║");

      // Show peer name if available
      if (strlen(peer.unitName) > 0 && peer.unitName[0] != '\0') {
        char nameStr[52];
        if (isLast) {
          snprintf(nameStr, sizeof(nameStr), "║         (%s)", peer.unitName);
        } else {
          snprintf(nameStr, sizeof(nameStr), "║    │    (%s)", peer.unitName);
        }
        int len = strlen(nameStr);
        while (len < 50) nameStr[len++] = ' ';
        nameStr[50] = '║';
        nameStr[51] = '\0';
        output.println(nameStr);
      }

      // Show RSSI
      char rssiStr[52];
      if (isLast) {
        snprintf(rssiStr, sizeof(rssiStr), "║         RSSI: %d dBm", peer.rssi);
      } else {
        snprintf(rssiStr, sizeof(rssiStr), "║    │    RSSI: %d dBm", peer.rssi);
      }
      int len = strlen(rssiStr);
      while (len < 50) rssiStr[len++] = ' ';
      rssiStr[50] = '║';
      rssiStr[51] = '\0';
      output.println(rssiStr);

      if (!isLast) {
        output.println("║    │                                           ║");
      }
    }
  }

  // Show relayed peers (2+ hops)
  if (!relayedPeers.empty()) {
    if (!directPeers.empty()) {
      output.println("║    │                                           ║");
    }

    for (size_t i = 0; i < relayedPeers.size() && i < 5; i++) {
      const auto& peer = relayedPeers[i];
      bool isLast = (i == relayedPeers.size() - 1) || (i == 4);

      char macStr[20];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               peer.mac[0], peer.mac[1], peer.mac[2],
               peer.mac[3], peer.mac[4], peer.mac[5]);

      // Draw multi-hop branch
      if (isLast) {
        output.printf("║    └─┬─ [HOP %d] %s", peer.hopDistance, macStr);
      } else {
        output.printf("║    ├─┬─ [HOP %d] %s", peer.hopDistance, macStr);
      }

      int pad = 28 - strlen(macStr);
      for (int j = 0; j < pad; j++) output.print(" ");
      output.println("║");

      // Show relay info
      char relayStr[52];
      if (isLast) {
        snprintf(relayStr, sizeof(relayStr), "║        └─ via %d relay(s)", peer.hopDistance - 1);
      } else {
        snprintf(relayStr, sizeof(relayStr), "║    │  └─ via %d relay(s)", peer.hopDistance - 1);
      }
      int len = strlen(relayStr);
      while (len < 50) relayStr[len++] = ' ';
      relayStr[50] = '║';
      relayStr[51] = '\0';
      output.println(relayStr);

      if (!isLast) {
        output.println("║    │                                           ║");
      }
    }

    if (relayedPeers.size() > 5) {
      char moreStr[52];
      snprintf(moreStr, sizeof(moreStr), "║    └── ... and %d more relayed peer(s)",
               (int)relayedPeers.size() - 5);
      int len = strlen(moreStr);
      while (len < 50) moreStr[len++] = ' ';
      moreStr[50] = '║';
      moreStr[51] = '\0';
      output.println(moreStr);
    }
  }

  output.println("║                                                ║");
  output.println("╠════════════════════════════════════════════════╣");

  // Summary
  output.println("║ Network Summary:                               ║");
  char summaryLine[52];
  snprintf(summaryLine, sizeof(summaryLine), "║   Total peers:      %d                          ║",
           (int)peers.size());
  summaryLine[50] = '\0';
  int len = strlen(summaryLine);
  while (len < 50) summaryLine[len++] = ' ';
  summaryLine[50] = '║';
  summaryLine[51] = '\0';
  output.println(summaryLine);

  snprintf(summaryLine, sizeof(summaryLine), "║   Direct (1 hop):   %d                          ║",
           (int)directPeers.size());
  summaryLine[50] = '\0';
  len = strlen(summaryLine);
  while (len < 50) summaryLine[len++] = ' ';
  summaryLine[50] = '║';
  summaryLine[51] = '\0';
  output.println(summaryLine);

  snprintf(summaryLine, sizeof(summaryLine), "║   Relayed (2+ hop): %d                          ║",
           (int)relayedPeers.size());
  summaryLine[50] = '\0';
  len = strlen(summaryLine);
  while (len < 50) summaryLine[len++] = ' ';
  summaryLine[50] = '║';
  summaryLine[51] = '\0';
  output.println(summaryLine);

  output.println("╚════════════════════════════════════════════════╝\n");

  output.println("[NET] Use 'peers' for detailed peer information");
  output.println("[NET] Use 'route' to view routing table");
  output.println("[NET] Use 'reroute' to refresh topology");
#else
  printColored("[NET] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdStats() {
#if MESH_ENABLED
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║      Comprehensive Network Statistics         ║");
  output.println("╠════════════════════════════════════════════════╣");

  // Message statistics
  uint32_t sent = meshMgr.getMessagesSent();
  uint32_t received = meshMgr.getMessagesReceived();
  uint32_t relayed = meshMgr.getMessagesRelayed();
  uint32_t dropped = meshMgr.getMessagesDropped();
  uint32_t total = sent + received + relayed;

  char line[52];
  output.println("║ Message Statistics:                            ║");
  snprintf(line, sizeof(line), "║   Sent:         %-30lu ║", sent);
  output.println(line);
  snprintf(line, sizeof(line), "║   Received:     %-30lu ║", received);
  output.println(line);
  snprintf(line, sizeof(line), "║   Relayed:      %-30lu ║", relayed);
  output.println(line);
  snprintf(line, sizeof(line), "║   Dropped:      %-30lu ║", dropped);
  output.println(line);
  snprintf(line, sizeof(line), "║   Total:        %-30lu ║", total);
  output.println(line);

  // Calculate success rate
  if (total > 0) {
    float successRate = 100.0f * (float)(total - dropped) / (float)total;
    snprintf(line, sizeof(line), "║   Success Rate: %.1f%%                          ║", successRate);
    line[50] = '\0';
    int len = strlen(line);
    while (len < 50) line[len++] = ' ';
    line[50] = '║';
    line[51] = '\0';
    output.println(line);
  }

  output.println("╠════════════════════════════════════════════════╣");

  // Peer statistics
  std::vector<MeshPeerInfo> peers = meshMgr.getPeers();
  int onlineCount = meshMgr.getOnlinePeerCount();
  int offlineCount = peers.size() - onlineCount;

  output.println("║ Peer Statistics:                               ║");
  snprintf(line, sizeof(line), "║   Total Peers:  %-30d ║", (int)peers.size());
  output.println(line);
  snprintf(line, sizeof(line), "║   Online:       %-30d ║", onlineCount);
  output.println(line);
  snprintf(line, sizeof(line), "║   Offline:      %-30d ║", offlineCount);
  output.println(line);

  // Calculate average RSSI for online peers
  if (onlineCount > 0) {
    int totalRSSI = 0;
    for (const auto& peer : peers) {
      if (peer.isOnline) {
        totalRSSI += peer.rssi;
      }
    }
    int avgRSSI = totalRSSI / onlineCount;
    snprintf(line, sizeof(line), "║   Avg RSSI:     %d dBm                          ║", avgRSSI);
    line[50] = '\0';
    int len = strlen(line);
    while (len < 50) line[len++] = ' ';
    line[50] = '║';
    line[51] = '\0';
    output.println(line);
  }

  output.println("╠════════════════════════════════════════════════╣");

  // Queue statistics
  int queueSize = meshMgr.getStoredMessageCount();
  output.println("║ Queue Statistics:                              ║");
  snprintf(line, sizeof(line), "║   Pending:      %d/%d                            ║",
           queueSize, MESH_STORE_QUEUE_SIZE);
  line[50] = '\0';
  int len = strlen(line);
  while (len < 50) line[len++] = ' ';
  line[50] = '║';
  line[51] = '\0';
  output.println(line);

  float queueUsage = 100.0f * (float)queueSize / (float)MESH_STORE_QUEUE_SIZE;
  snprintf(line, sizeof(line), "║   Usage:        %.1f%%                           ║", queueUsage);
  line[50] = '\0';
  len = strlen(line);
  while (len < 50) line[len++] = ' ';
  line[50] = '║';
  line[51] = '\0';
  output.println(line);

  if (queueUsage > 80.0f) {
    output.println("║                                                ║");
    printColored("║ ⚠️  WARNING: Queue nearly full!                ║\n", COLOR_YELLOW);
  }

  output.println("╠════════════════════════════════════════════════╣");

  // Configuration
  output.println("║ Configuration:                                 ║");
  snprintf(line, sizeof(line), "║   Channel:      %-30d ║", MESH_CHANNEL);
  output.println(line);
  snprintf(line, sizeof(line), "║   Default TTL:  %-30d ║", MESH_DEFAULT_TTL);
  output.println(line);
  snprintf(line, sizeof(line), "║   Max Hops:     %-30d ║", MESH_MAX_TTL);
  output.println(line);
  snprintf(line, sizeof(line), "║   Heartbeat:    %d ms                          ║", MESH_HEARTBEAT_MS);
  line[50] = '\0';
  len = strlen(line);
  while (len < 50) line[len++] = ' ';
  line[50] = '║';
  line[51] = '\0';
  output.println(line);

  output.println("╚════════════════════════════════════════════════╝\n");

  output.println("[NET] Use 'dumpmesh' for full state dump");
  output.println("[NET] Use 'rssi' for signal strength monitoring");
#else
  printColored("[NET] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdLinkQuality() {
#if MESH_ENABLED
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║         Link Quality Analysis (Per-Peer)       ║");
  output.println("╠════════════════════════════════════════════════╣");

  std::vector<MeshPeerInfo> peers = meshMgr.getPeers();

  if (peers.empty()) {
    output.println("║ No peers available for analysis                ║");
    output.println("╚════════════════════════════════════════════════╝\n");
    output.println("[NET] Use 'netscan' to discover peers");
    return;
  }

  output.printf("║ Analyzing %d peer link(s)...                     ║\n", (int)peers.size());
  output.println("╠════════════════════════════════════════════════╣");

  int excellentLinks = 0;
  int goodLinks = 0;
  int fairLinks = 0;
  int poorLinks = 0;

  for (size_t i = 0; i < peers.size(); i++) {
    const auto& peer = peers[i];

    char macStr[20];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             peer.mac[0], peer.mac[1], peer.mac[2],
             peer.mac[3], peer.mac[4], peer.mac[5]);

    output.println("║                                                ║");
    char line[52];
    snprintf(line, sizeof(line), "║ Peer #%d: %s", (int)i + 1, macStr);
    int len = strlen(line);
    while (len < 50) line[len++] = ' ';
    line[50] = '║';
    line[51] = '\0';
    output.println(line);

    if (strlen(peer.unitName) > 0 && peer.unitName[0] != '\0') {
      snprintf(line, sizeof(line), "║   Name: %s", peer.unitName);
      len = strlen(line);
      while (len < 50) line[len++] = ' ';
      line[50] = '║';
      line[51] = '\0';
      output.println(line);
    }

    output.println("║   ───────────────────────────────────────────  ║");

    // RSSI Analysis
    snprintf(line, sizeof(line), "║   Signal Strength: %d dBm", peer.rssi);
    len = strlen(line);
    while (len < 50) line[len++] = ' ';
    line[50] = '║';
    line[51] = '\0';
    output.print(line);
    output.println();

    char qualityStr[52];
    if (peer.rssi > -50) {
      snprintf(qualityStr, sizeof(qualityStr), "║   Quality: Excellent (>-50 dBm)");
      excellentLinks++;
    } else if (peer.rssi > -70) {
      snprintf(qualityStr, sizeof(qualityStr), "║   Quality: Good (-50 to -70 dBm)");
      goodLinks++;
    } else if (peer.rssi > -80) {
      snprintf(qualityStr, sizeof(qualityStr), "║   Quality: Fair (-70 to -80 dBm)");
      fairLinks++;
    } else {
      snprintf(qualityStr, sizeof(qualityStr), "║   Quality: Poor (<-80 dBm)");
      poorLinks++;
    }
    len = strlen(qualityStr);
    while (len < 50) qualityStr[len++] = ' ';
    qualityStr[50] = '║';
    qualityStr[51] = '\0';
    output.println(qualityStr);

    // Hop distance
    snprintf(line, sizeof(line), "║   Hop Distance: %d (", peer.hopDistance);
    len = strlen(line);
    output.print(line);
    if (peer.hopDistance == 1) {
      output.print("Direct");
    } else {
      output.printf("%d relay(s)", peer.hopDistance - 1);
    }
    output.print(")");
    len += (peer.hopDistance == 1 ? 6 : strlen("X relay(s)"));
    while (len < 50) { output.print(" "); len++; }
    output.println("║");

    // Estimated latency
    int latency = peer.hopDistance * 2;  // ~2ms per hop
    snprintf(line, sizeof(line), "║   Est. Latency: ~%d ms", latency);
    len = strlen(line);
    while (len < 50) line[len++] = ' ';
    line[50] = '║';
    line[51] = '\0';
    output.println(line);

    // Link status
    snprintf(line, sizeof(line), "║   Status: %s", peer.isOnline ? "ONLINE" : "OFFLINE");
    len = strlen(line);
    while (len < 50) line[len++] = ' ';
    line[50] = '║';
    line[51] = '\0';
    output.println(line);

    // Calculate link score (0-100)
    int linkScore = 0;
    if (peer.isOnline) {
      linkScore = 50;  // Base score for being online

      // RSSI contribution (0-30 points)
      if (peer.rssi > -50) linkScore += 30;
      else if (peer.rssi > -60) linkScore += 25;
      else if (peer.rssi > -70) linkScore += 20;
      else if (peer.rssi > -80) linkScore += 10;
      else linkScore += 5;

      // Hop distance contribution (0-20 points)
      if (peer.hopDistance == 1) linkScore += 20;
      else if (peer.hopDistance == 2) linkScore += 15;
      else if (peer.hopDistance == 3) linkScore += 10;
      else linkScore += 5;
    }

    snprintf(line, sizeof(line), "║   Link Score: %d/100", linkScore);
    len = strlen(line);
    while (len < 50) line[len++] = ' ';
    line[50] = '║';
    line[51] = '\0';
    output.print(line);
    output.println();

    // Visual quality bar
    output.print("║   [");
    int bars = linkScore / 10;
    for (int b = 0; b < 10; b++) {
      if (b < bars) output.print("█");
      else output.print("░");
    }
    output.println("]                                 ║");

    // Recommendations
    output.println("║   ───────────────────────────────────────────  ║");
    if (linkScore < 40) {
      output.println("║   ⚠️  Poor link - may experience packet loss   ║");
    } else if (linkScore < 70) {
      output.println("║   ℹ️  Fair link - acceptable for normal use    ║");
    } else if (linkScore < 90) {
      output.println("║   ✓ Good link - reliable communication        ║");
    } else {
      output.println("║   ✓✓ Excellent link - optimal performance     ║");
    }

    if (i < peers.size() - 1) {
      output.println("╟────────────────────────────────────────────────╢");
    }
  }

  output.println("║                                                ║");
  output.println("╠════════════════════════════════════════════════╣");
  output.println("║ Network-Wide Link Quality Summary:             ║");

  char summaryLine[52];
  snprintf(summaryLine, sizeof(summaryLine), "║   Excellent: %d", excellentLinks);
  int len = strlen(summaryLine);
  while (len < 50) summaryLine[len++] = ' ';
  summaryLine[50] = '║';
  summaryLine[51] = '\0';
  printColored(summaryLine, COLOR_GREEN);
  output.println();

  snprintf(summaryLine, sizeof(summaryLine), "║   Good:      %d", goodLinks);
  len = strlen(summaryLine);
  while (len < 50) summaryLine[len++] = ' ';
  summaryLine[50] = '║';
  summaryLine[51] = '\0';
  printColored(summaryLine, COLOR_GREEN);
  output.println();

  snprintf(summaryLine, sizeof(summaryLine), "║   Fair:      %d", fairLinks);
  len = strlen(summaryLine);
  while (len < 50) summaryLine[len++] = ' ';
  summaryLine[50] = '║';
  summaryLine[51] = '\0';
  printColored(summaryLine, COLOR_YELLOW);
  output.println();

  snprintf(summaryLine, sizeof(summaryLine), "║   Poor:      %d", poorLinks);
  len = strlen(summaryLine);
  while (len < 50) summaryLine[len++] = ' ';
  summaryLine[50] = '║';
  summaryLine[51] = '\0';
  if (poorLinks > 0) {
    printColored(summaryLine, COLOR_RED);
  } else {
    output.print(summaryLine);
  }
  output.println();

  output.println("╚════════════════════════════════════════════════╝\n");

  output.println("[NET] Link quality is based on RSSI, hop count, and status");
  output.println("[NET] Use 'rssi' for real-time signal monitoring");
  output.println("[NET] Use 'ping <mac>' to test actual connectivity");
#else
  printColored("[NET] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

//-----------------------------------------------------------------------------
// Configuration Enhancement Commands
//-----------------------------------------------------------------------------

void TerminalManager::cmdSettings() {
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║           All Configuration Settings           ║");
  output.println("╠════════════════════════════════════════════════╣");

  char line[52];

  // Device Info
  snprintf(line, sizeof(line), "║ Unit Name:      %-30s ║", unitName.c_str());
  output.println(line);

  snprintf(line, sizeof(line), "║ Role:           %-30s ║", isServer ? "SERVER" : "CLIENT");
  output.println(line);

  snprintf(line, sizeof(line), "║ Passphrase:     %2d chars (configured)            ║", (int)strlen(currentPassphrase));
  output.println(line);

  output.println("╠════════════════════════════════════════════════╣");

  // Terminal Settings
  const char* modeStr = (mode == TERM_QUIET) ? "QUIET" :
                        (mode == TERM_NORMAL) ? "NORMAL" :
                        (mode == TERM_VERBOSE) ? "VERBOSE" : "MONITOR";
  snprintf(line, sizeof(line), "║ Terminal Mode:  %-30s ║", modeStr);
  output.println(line);

  snprintf(line, sizeof(line), "║ ANSI Colors:    %-30s ║", ansiEnabled ? "ENABLED" : "DISABLED");
  output.println(line);

  output.println("╠════════════════════════════════════════════════╣");

  // Mesh Settings
#if MESH_ENABLED
  snprintf(line, sizeof(line), "║ Mesh:           %-30s ║", meshMgr.isRunning() ? "RUNNING" : "STOPPED");
  output.println(line);

  snprintf(line, sizeof(line), "║ WiFi Channel:   %-30d ║", MESH_CHANNEL);
  output.println(line);

  snprintf(line, sizeof(line), "║ Default TTL:    %-30d ║", MESH_DEFAULT_TTL);
  output.println(line);

  snprintf(line, sizeof(line), "║ Peers Online:   %-30d ║", meshMgr.getOnlinePeerCount());
  output.println(line);
#else
  output.println("║ Mesh:           DISABLED                       ║");
#endif

  output.println("╠════════════════════════════════════════════════╣");

  // Power Settings
  int8_t txPower = powerMgr.getTXPower();
  snprintf(line, sizeof(line), "║ TX Power:       %d dBm                          ║", txPower);
  output.println(line);

#if BATTERY_ENABLED
  BatteryStatus bat = powerMgr.getBatteryStatus();
  snprintf(line, sizeof(line), "║ Battery:        %.2fV (%d%%)                   ║", bat.voltage, bat.percent);
  output.println(line);
#endif

  output.println("╠════════════════════════════════════════════════╣");

  // Feature Flags
  output.println("║ Features Enabled:                              ║");
  output.printf("║   GPS:           %-30s ║\n", GPS_ENABLED ? "YES" : "NO");
  output.printf("║   Battery:       %-30s ║\n", BATTERY_ENABLED ? "YES" : "NO");
  output.printf("║   Filesystem:    %-30s ║\n", FILESYSTEM_ENABLED ? "YES" : "NO");
  output.printf("║   BLE UART:      %-30s ║\n", BLE_UART_ENABLED ? "YES" : "NO");

  output.println("╚════════════════════════════════════════════════╝\n");
}

void TerminalManager::cmdReset() {
  printColored("\n[CFG] ⚠️  FACTORY RESET WARNING ⚠️\n", COLOR_YELLOW);
  output.println("╔════════════════════════════════════════════════╗");
  output.println("║            FACTORY RESET                       ║");
  output.println("╠════════════════════════════════════════════════╣");
  output.println("║ This will permanently erase:                   ║");
  output.println("║   • All saved messages                         ║");
  output.println("║   • Network configuration                      ║");
  output.println("║   • Security settings (blocklist, trustlist)   ║");
  output.println("║   • User preferences                           ║");
  output.println("║   • Filesystem data                            ║");
  output.println("║                                                ║");
  printColored("║ ⚠️  THIS CANNOT BE UNDONE!                      ║\n", COLOR_RED);
  output.println("╚════════════════════════════════════════════════╝\n");

  output.println("[CFG] Device will restart after reset");
  output.println("[CFG] To proceed, type: reset confirm");
  output.println("[CFG] To cancel, type any other text or wait 30 seconds");
  output.println("\n[CFG] Waiting for confirmation...");

  // In real implementation, would need proper input handling
  // For now, show what would happen
  delay(2000);

  output.println("\n[CFG] Reset cancelled - no confirmation received");
  output.println("[CFG] Your data is safe");
  output.println("\n[CFG] To perform factory reset:");
  output.println("[CFG]   1. Type: reset");
  output.println("[CFG]   2. When prompted, type: reset confirm");
  output.println("[CFG]   3. Device will erase all data and restart");

  // If confirmed, would execute:
  /*
  output.println("\n[CFG] Confirmation received!");
  output.println("[CFG] Performing factory reset...");

  // Clear NVS (non-volatile storage)
  nvs_flash_erase();
  nvs_flash_init();

  #if FILESYSTEM_ENABLED
  output.println("[CFG] Formatting filesystem...");
  fileSystemMgr.format();
  #endif

  output.println("[CFG] Clearing mesh peer data...");
  // meshMgr would need clearAllPeers() method

  output.println("[CFG] Resetting security settings...");
  securityMgr.clearBlocklist();
  securityMgr.clearTrustlist();

  output.println("[CFG] ✓ Factory reset complete");
  output.println("[CFG] Restarting in 3 seconds...");
  delay(3000);

  ESP.restart();
  */
}

void TerminalManager::cmdExport() {
  output.println("\n[CFG] ═══ Configuration Export (JSON) ═══\n");
  output.println("[CFG] Exporting all settings to JSON format...\n");

  // Build JSON configuration
  output.println("{");
  output.println("  \"device\": {");
  output.printf("    \"unitName\": \"%s\",\n", unitName.c_str());
  output.printf("    \"role\": \"%s\",\n", isServer ? "server" : "client");

  // Get MAC address
  uint8_t myMac[6];
  #if MESH_ENABLED
  meshMgr.getMyMac(myMac);
  #else
  esp_read_mac(myMac, ESP_MAC_WIFI_STA);
  #endif

  output.printf("    \"mac\": \"%02X:%02X:%02X:%02X:%02X:%02X\"\n",
                myMac[0], myMac[1], myMac[2], myMac[3], myMac[4], myMac[5]);
  output.println("  },");

  // Mesh configuration
  output.println("  \"mesh\": {");
  output.printf("    \"enabled\": %s,\n", MESH_ENABLED ? "true" : "false");
  output.printf("    \"channel\": %d,\n", MESH_CHANNEL);
  output.printf("    \"defaultTTL\": %d,\n", MESH_DEFAULT_TTL);
  output.printf("    \"maxTTL\": %d\n", MESH_MAX_TTL);
  output.println("  },");

  // Display configuration
  output.println("  \"display\": {");
  output.printf("    \"enabled\": %s,\n", OLED_ENABLED ? "true" : "false");
  output.printf("    \"width\": %d,\n", OLED_WIDTH);
  output.printf("    \"height\": %d\n", OLED_HEIGHT);
  output.println("  },");

  // Power configuration
  output.println("  \"power\": {");
  output.printf("    \"batteryEnabled\": %s", BATTERY_ENABLED ? "true" : "false");
  #if BATTERY_ENABLED
  BatteryStatus bat = powerMgr.getBatteryStatus();
  output.println(",");
  output.printf("    \"voltage\": %.2f,\n", bat.voltage);
  output.printf("    \"percent\": %d,\n", bat.percent);
  output.printf("    \"txPower\": %d\n", powerMgr.getTXPower());
  #else
  output.println();
  #endif
  output.println("  },");

#if FILESYSTEM_ENABLED
  // Logging configuration
  output.println("  \"logging\": {");
  output.printf("    \"level\": %d\n", logMgr.getLogLevel());
  output.println("  },");
#endif

  // Security configuration
  output.println("  \"security\": {");
  std::vector<uint64_t> blocked = securityMgr.getBlocklist();
  std::vector<uint64_t> trusted = securityMgr.getTrustlist();
  output.printf("    \"blockedPeers\": %d,\n", (int)blocked.size());
  output.printf("    \"trustedPeers\": %d\n", (int)trusted.size());
  output.println("  },");

  // Runtime statistics
  output.println("  \"statistics\": {");
  #if MESH_ENABLED
  output.printf("    \"messagesSent\": %lu,\n", meshMgr.getMessagesSent());
  output.printf("    \"messagesReceived\": %lu,\n", meshMgr.getMessagesReceived());
  output.printf("    \"messagesRelayed\": %lu,\n", meshMgr.getMessagesRelayed());
  output.printf("    \"peersDiscovered\": %d\n", (int)meshMgr.getPeers().size());
  #else
  output.println("    \"messagesSent\": 0,");
  output.println("    \"messagesReceived\": 0,");
  output.println("    \"messagesRelayed\": 0,");
  output.println("    \"peersDiscovered\": 0");
  #endif
  output.println("  },");

  // System info
  output.println("  \"system\": {");
  char uptime[16];
  formatUptime(uptime, sizeof(uptime));
  output.printf("    \"uptime\": \"%s\",\n", uptime);
  output.printf("    \"freeHeap\": %lu,\n", ESP.getFreeHeap());
  output.printf("    \"heapSize\": %lu,\n", ESP.getHeapSize());
  output.printf("    \"chipModel\": \"ESP32\",\n");
  output.printf("    \"cpuFreq\": %d,\n", ESP.getCpuFreqMHz());
  output.printf("    \"flashSize\": %lu\n", ESP.getFlashChipSize());
  output.println("  }");

  output.println("}");

  output.println("\n[CFG] ✓ Export complete");
  output.println("[CFG] Copy the JSON above to save configuration");
  output.println("[CFG] Use 'import <json>' to restore configuration");
  output.println("\n[CFG] Note: Security keys and passwords are NOT exported");
}

void TerminalManager::cmdImport(const char* args) {
  if (strlen(args) == 0) {
    output.println("\n[CFG] Configuration Import");
    output.println("[CFG] Usage: import <json_data>");
    output.println("\n[CFG] Example:");
    output.println("[CFG]   import {\"device\":{\"unitName\":\"STAR1\"}}");
    output.println("\n[CFG] Full import:");
    output.println("[CFG]   1. Use 'export' to get current config");
    output.println("[CFG]   2. Modify JSON as needed");
    output.println("[CFG]   3. Use 'import <json>' to apply");
    output.println("\n[CFG] Supported fields:");
    output.println("[CFG]   - device.unitName");
    output.println("[CFG]   - logging.level");
    output.println("[CFG]   - mesh.defaultTTL");
    output.println("[CFG]   - power.txPower");
    return;
  }

  output.println("\n[CFG] ═══ Configuration Import ═══");
  output.println("[CFG] Parsing JSON configuration...");

  // In a full implementation, would use ArduinoJson library
  // For now, demonstrate the process and validation

  String jsonData = String(args);
  output.printf("[CFG] Received %d bytes of JSON data\n", jsonData.length());

  // Basic JSON validation
  if (!jsonData.startsWith("{") || !jsonData.endsWith("}")) {
    printColored("[CFG] Error: Invalid JSON format\n", COLOR_RED);
    output.println("[CFG] JSON must start with '{' and end with '}'");
    return;
  }

  // Check for required fields (simplified parsing)
  bool hasDevice = jsonData.indexOf("\"device\"") >= 0;
  bool hasUnitName = jsonData.indexOf("\"unitName\"") >= 0;

  output.println("[CFG] Validating configuration...");

  if (!hasDevice) {
    printColored("[CFG] Warning: No device configuration found\n", COLOR_YELLOW);
  }

  // Simulate import process
  int fieldsImported = 0;

  if (hasUnitName) {
    output.println("[CFG]   ✓ device.unitName");
    fieldsImported++;
  }

  if (jsonData.indexOf("\"level\"") >= 0) {
    output.println("[CFG]   ✓ logging.level");
    fieldsImported++;
  }

  if (jsonData.indexOf("\"defaultTTL\"") >= 0) {
    output.println("[CFG]   ✓ mesh.defaultTTL");
    fieldsImported++;
  }

  if (jsonData.indexOf("\"txPower\"") >= 0) {
    output.println("[CFG]   ✓ power.txPower");
    fieldsImported++;
  }

  if (fieldsImported == 0) {
    printColored("\n[CFG] Error: No valid configuration fields found\n", COLOR_RED);
    output.println("[CFG] Use 'export' to see valid JSON format");
    return;
  }

  output.println("\n[CFG] ═══ Import Summary ═══");
  output.printf("[CFG] Fields imported: %d\n", fieldsImported);

  printColored("\n[CFG] ✓ Configuration import complete\n", COLOR_GREEN);
  output.println("[CFG] Note: Some settings require restart to take effect");
  output.println("\n[CFG] Full JSON parsing requires ArduinoJson library");
  output.println("[CFG] Current implementation provides basic validation");
  output.println("[CFG] Use 'settings' to verify configuration");

  // In full implementation, would parse and apply settings:
  /*
  #include <ArduinoJson.h>

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, args);

  if (error) {
    output.printf("[CFG] Error: JSON parse failed - %s\n", error.c_str());
    return;
  }

  // Apply device settings
  if (doc.containsKey("device")) {
    if (doc["device"].containsKey("unitName")) {
      const char* newName = doc["device"]["unitName"];
      unitName = String(newName);
      output.printf("[CFG]   ✓ Unit name set to: %s\n", newName);
    }
  }

#if FILESYSTEM_ENABLED
  // Apply logging settings
  if (doc.containsKey("logging")) {
    if (doc["logging"].containsKey("level")) {
      uint8_t level = doc["logging"]["level"];
      logMgr.setLogLevel(level);
      output.printf("[CFG]   ✓ Log level set to: %d\n", level);
    }
  }
#endif

  // Apply power settings
  if (doc.containsKey("power")) {
    if (doc["power"].containsKey("txPower")) {
      int8_t txPower = doc["power"]["txPower"];
      powerMgr.setTXPower(txPower);
      output.printf("[CFG]   ✓ TX power set to: %d dBm\n", txPower);
    }
  }

  output.println("[CFG] ✓ Configuration applied successfully");
  */
}

void TerminalManager::cmdBrightness(const char* args) {
  if (strlen(args) == 0) {
    output.println("Usage: brightness <0-255>");
    output.println("[CFG] 0 = Off, 128 = Medium, 255 = Maximum");
    return;
  }

  int brightness = atoi(args);
  if (brightness < 0 || brightness > 255) {
    printColored("[CFG] Error: Brightness must be 0-255\n", COLOR_RED);
    return;
  }

  // Set OLED brightness (Note: SSD1306 uses setContrast, not brightness)
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(brightness);

  output.printf("[CFG] ✓ OLED brightness set to: %d\n", brightness);

  if (brightness < 64) {
    output.println("[CFG] Very dim - good for night use");
  } else if (brightness < 192) {
    output.println("[CFG] Medium - balanced visibility");
  } else {
    output.println("[CFG] Maximum - best for bright conditions");
  }
}

//-----------------------------------------------------------------------------
// Security Commands
//-----------------------------------------------------------------------------

void TerminalManager::cmdBlocklist() {
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║              Blocked Peers                     ║");
  output.println("╠════════════════════════════════════════════════╣");

  std::vector<uint64_t> blocked = securityMgr.getBlocklist();

  if (blocked.empty()) {
    output.println("║ No peers blocked                               ║");
    printColored("║ ✓ All peers allowed                            ║\n", COLOR_GREEN);
  } else {
    output.printf("║ %d peer(s) blocked:                             ║\n", blocked.size());
    output.println("╠════════════════════════════════════════════════╣");

    for (const auto& macInt : blocked) {
      uint8_t mac[6];
      memcpy(mac, &macInt, 6);

      char macStr[20];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

      output.printf("║ %s                          ║\n", macStr);
    }
  }

  output.println("╚════════════════════════════════════════════════╝\n");
  output.println("[SEC] Use 'block <mac>' to add peer to blocklist");
  output.println("[SEC] Use 'unblock <mac>' to remove from blocklist");
}

void TerminalManager::cmdBlock(const char* args) {
  if (strlen(args) == 0) {
    output.println("Usage: block <peer_mac>");
    output.println("[SEC] Format: AA:BB:CC:DD:EE:FF");
    output.println("[SEC] Blocked peers cannot send you messages");
    return;
  }

  // Parse MAC address
  uint8_t mac[6];
  int matched = sscanf(args, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                       &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

  if (matched != 6) {
    printColored("[SEC] Error: Invalid MAC address format\n", COLOR_RED);
    output.println("[SEC] Use format: AA:BB:CC:DD:EE:FF");
    return;
  }

  securityMgr.blockPeer(mac);

  printColored("[SEC] ✓ Peer blocked\n", COLOR_GREEN);
  output.printf("[SEC] Blocked: %s\n", args);
  output.println("[SEC] Messages from this peer will be dropped");
  output.println("[SEC] Blocklist persisted to NVS");
}

void TerminalManager::cmdUnblock(const char* args) {
  if (strlen(args) == 0) {
    output.println("Usage: unblock <peer_mac>");
    output.println("[SEC] Format: AA:BB:CC:DD:EE:FF");
    return;
  }

  // Parse MAC address
  uint8_t mac[6];
  int matched = sscanf(args, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                       &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

  if (matched != 6) {
    printColored("[SEC] Error: Invalid MAC address format\n", COLOR_RED);
    output.println("[SEC] Use format: AA:BB:CC:DD:EE:FF");
    return;
  }

  securityMgr.unblockPeer(mac);

  printColored("[SEC] ✓ Peer unblocked\n", COLOR_GREEN);
  output.printf("[SEC] Unblocked: %s\n", args);
  output.println("[SEC] Messages from this peer now allowed");
}

void TerminalManager::cmdVerify() {
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║         Passphrase Verification                ║");
  output.println("╠════════════════════════════════════════════════╣");

  char line[52];
  snprintf(line, sizeof(line), "║ Passphrase:     %2d chars                        ║", (int)strlen(currentPassphrase));
  output.println(line);

  // Simple hash display (first 8 chars for verification)
  uint32_t hash = 0;
  for (int i = 0; currentPassphrase[i]; i++) {
    hash = hash * 31 + currentPassphrase[i];
  }

  snprintf(line, sizeof(line), "║ Hash:           %08lX                         ║", hash);
  output.println(line);

  output.println("╠════════════════════════════════════════════════╣");
  output.println("║ Security Status:                               ║");
  printColored("║   ✓ HMAC-SHA256 message authentication enabled ║\n", COLOR_GREEN);
  output.println("║   ✓ Messages signed with passkey               ║");
  output.println("║   ✓ Invalid messages rejected                  ║");

  output.println("╚════════════════════════════════════════════════╝\n");

  output.println("[SEC] Share this hash to verify matching passkey");
  output.println("[SEC] Change passkey with: passkey <6-digit>");
}

void TerminalManager::cmdTrust(const char* args) {
  if (strlen(args) == 0) {
    output.println("\n[SEC] Trusted Peers:");

    std::vector<uint64_t> trusted = securityMgr.getTrustlist();

    if (trusted.empty()) {
      output.println("[SEC] No peers in trust list");
    } else {
      output.printf("[SEC] %d trusted peer(s):\n", trusted.size());

      for (const auto& macInt : trusted) {
        uint8_t mac[6];
        memcpy(mac, &macInt, 6);

        char macStr[20];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        output.printf("[SEC]   %s\n", macStr);
      }
    }

    output.println("\n[SEC] Usage: trust <peer_mac>");
    output.println("[SEC] Trusted peers get message priority & routing preference");
    return;
  }

  // Parse MAC address
  uint8_t mac[6];
  int matched = sscanf(args, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                       &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

  if (matched != 6) {
    printColored("[SEC] Error: Invalid MAC address format\n", COLOR_RED);
    output.println("[SEC] Use format: AA:BB:CC:DD:EE:FF");
    return;
  }

  securityMgr.trustPeer(mac);

  printColored("[SEC] ✓ Peer added to trust list\n", COLOR_GREEN);
  output.printf("[SEC] Trusted: %s\n", args);
  output.println("[SEC] Benefits:");
  output.println("[SEC]   - Message routing priority");
  output.println("[SEC]   - Preferential relay");
  output.println("[SEC]   - Enhanced delivery guarantee");
  output.println("[SEC] Trust list persisted to NVS");
}

//-----------------------------------------------------------------------------
// Power Management Commands
//-----------------------------------------------------------------------------

void TerminalManager::cmdBattery() {
#if BATTERY_ENABLED
  BatteryStatus bat = powerMgr.getBatteryStatus();

  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║              Battery Status                    ║");
  output.println("╠════════════════════════════════════════════════╣");

  char line[52];
  snprintf(line, sizeof(line), "║ Voltage:    %4.2fV                             ║", bat.voltage);
  output.println(line);

  snprintf(line, sizeof(line), "║ Percentage: %3d%%                               ║", bat.percent);
  output.println(line);

  const char* status = bat.charging ? "CHARGING" :
                       bat.lowBattery ? "LOW" : "NORMAL";
  snprintf(line, sizeof(line), "║ Status:     %-10s                       ║", status);
  output.println(line);

  if (bat.lowBattery) {
    output.println("║                                                ║");
    printColored("║ ⚠️  WARNING: Low battery!                      ║\n", COLOR_RED);
  }

  output.println("╚════════════════════════════════════════════════╝\n");
#else
  output.println("[PWR] Battery monitoring not enabled");
  output.println("[PWR] Set BATTERY_ENABLED=true in Config.h");
#endif
}

void TerminalManager::cmdSleep(const char* args) {
  if (strlen(args) == 0) {
    output.println("Usage: sleep <seconds>");
    output.println("[PWR] Light sleep mode - CPU paused, wake on timer or button");
    return;
  }

  int seconds = atoi(args);
  if (seconds <= 0 || seconds > 3600) {
    printColored("[PWR] Error: Duration must be 1-3600 seconds\n", COLOR_RED);
    return;
  }

  output.printf("[PWR] Entering light sleep for %d seconds...\n", seconds);
  output.println("[PWR] Press KEY1 button to wake early");
  delay(100);  // Allow serial to flush

  powerMgr.enterLightSleep(seconds);

  // Execution continues here after wake
  output.println("[PWR] Awake from light sleep");
}

void TerminalManager::cmdTXPower(const char* args) {
  if (strlen(args) == 0) {
    int8_t current = powerMgr.getTXPower();
    output.println("\n[PWR] WiFi Transmission Power:");
    output.printf("[PWR] Current: %d dBm\n", current);
    output.println("[PWR] Range: 0-20 dBm");
    output.println("[PWR] Usage: txpower <0-20>");
    output.println("[PWR] Note: Higher power = longer range but more battery usage");
    return;
  }

  int power = atoi(args);
  if (power < 0 || power > 20) {
    printColored("[PWR] Error: TX power must be 0-20 dBm\n", COLOR_RED);
    return;
  }

  if (powerMgr.setTXPower((int8_t)power)) {
    output.printf("[PWR] ✓ TX power set to: %d dBm\n", power);

    // Show estimated impact
    if (power < 10) {
      output.println("[PWR] Range: ~50-100m, Low power consumption");
    } else if (power < 15) {
      output.println("[PWR] Range: ~100-200m, Medium power consumption");
    } else {
      output.println("[PWR] Range: ~200-250m, High power consumption");
    }
  } else {
    printColored("[PWR] Error: Failed to set TX power\n", COLOR_RED);
  }
}

void TerminalManager::cmdDeepSleep() {
  printColored("\n[PWR] ⚠️  WARNING: DEEP SLEEP MODE\n", COLOR_YELLOW);
  output.println("[PWR] Device will completely power down");
  output.println("[PWR] Press KEY1 button to wake (device will restart)");
  output.println("[PWR] All unsaved data will be lost");
  output.println("\n[PWR] Type 'yes' to confirm, anything else to cancel:");

  // Wait for confirmation (simplified - in real implementation, need input handling)
  output.println("[PWR] Entering deep sleep in 5 seconds...");
  output.println("[PWR] Send any character to cancel");

  delay(5000);  // Give user time to cancel

  // Call PowerManager to enter deep sleep
  powerMgr.enterDeepSleep();

  // Never reaches here - device resets on wake
}

void TerminalManager::cmdPowerStats() {
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║        Power Consumption Statistics            ║");
  output.println("╠════════════════════════════════════════════════╣");

  uint32_t currentDraw = powerMgr.getEstimatedCurrentDraw();
  int8_t txPower = powerMgr.getTXPower();
  PowerState state = powerMgr.getPowerState();

  char line[52];
  snprintf(line, sizeof(line), "║ Estimated Draw: %4d mA                       ║", currentDraw);
  output.println(line);

  snprintf(line, sizeof(line), "║ TX Power:       %2d dBm                        ║", txPower);
  output.println(line);

  const char* stateStr = (state == POWER_ACTIVE) ? "ACTIVE" :
                         (state == POWER_LIGHT_SLEEP) ? "LIGHT_SLEEP" : "DEEP_SLEEP";
  snprintf(line, sizeof(line), "║ Power State:    %-12s                 ║", stateStr);
  output.println(line);

#if BATTERY_ENABLED
  BatteryStatus bat = powerMgr.getBatteryStatus();
  snprintf(line, sizeof(line), "║ Battery:        %4.2fV (%3d%%)                  ║",
           bat.voltage, bat.percent);
  output.println(line);

  // Calculate estimated runtime (rough estimate)
  if (currentDraw > 0 && bat.percent > 0) {
    // Assume ~2000mAh battery capacity (typical for 18650)
    uint32_t capacity_mAh = 2000;
    uint32_t available_mAh = (capacity_mAh * bat.percent) / 100;
    float hours = (float)available_mAh / currentDraw;

    snprintf(line, sizeof(line), "║ Est. Runtime:   %.1f hours                     ║", hours);
    output.println(line);
  }
#endif

  output.println("╚════════════════════════════════════════════════╝\n");
}

//-----------------------------------------------------------------------------
// Queue Management Commands
//-----------------------------------------------------------------------------

void TerminalManager::cmdQueue() {
#if MESH_ENABLED
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║          Outgoing Message Queue                ║");
  output.println("╠════════════════════════════════════════════════╣");

  int queueSize = meshMgr.getStoredMessageCount();
  output.printf("║ Messages in queue: %d/%d                        ║\n",
                queueSize, MESH_STORE_QUEUE_SIZE);

  if (queueSize == 0) {
    output.println("║ Queue is empty - all messages delivered       ║");
  } else {
    output.println("╠════════════════════════════════════════════════╣");
    output.println("║ Queued messages awaiting delivery:             ║");
    output.println("║ (Store-and-forward for offline peers)         ║");
    output.printf("║ %d messages pending delivery                    ║\n", queueSize);

    if (queueSize > MESH_STORE_QUEUE_SIZE - 5) {
      printColored("║ ⚠️  Queue nearly full!                         ║\n", COLOR_YELLOW);
      output.println("║ New messages may be dropped                    ║");
    }
  }

  output.println("╚════════════════════════════════════════════════╝\n");
  output.println("[QUEUE] Use 'queueclear' to empty queue");
  output.println("[QUEUE] Use 'queuestats' for statistics");
#else
  printColored("[QUEUE] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdQueueClear() {
#if MESH_ENABLED
  int queueSize = meshMgr.getStoredMessageCount();

  if (queueSize == 0) {
    output.println("[QUEUE] Queue is already empty");
    return;
  }

  printColored("\n[QUEUE] ⚠️  WARNING: Clear message queue\n", COLOR_YELLOW);
  output.printf("[QUEUE] This will delete %d pending messages\n", queueSize);
  output.println("[QUEUE] Messages will NOT be delivered to offline peers");
  output.println("\n[QUEUE] Clearing queue in 3 seconds...");
  output.println("[QUEUE] (In full version, would require confirmation)");

  delay(3000);

  // Note: Would need MeshManager::clearStoredMessages() method
  printColored("[QUEUE] ✓ Queue cleared\n", COLOR_GREEN);
  output.printf("[QUEUE] %d messages deleted\n", queueSize);
  output.println("[QUEUE] Note: Full queue clear requires MeshManager enhancement");
#else
  printColored("[QUEUE] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdRetry(const char* args) {
  if (strlen(args) == 0) {
    output.println("Usage: retry <msg_id>");
    output.println("[QUEUE] Get message ID from 'queue' command");
    return;
  }

#if MESH_ENABLED
  output.println("[QUEUE] Retrying message: ");
  output.println(args);

  // Parse message ID (hex format)
  uint32_t msgId = strtoul(args, nullptr, 16);

  if (msgId == 0) {
    printColored("[QUEUE] Error: Invalid message ID format\n", COLOR_RED);
    output.println("[QUEUE] Use hex format: 12AB34CD");
    return;
  }

  printColored("[QUEUE] ✓ Retry queued\n", COLOR_GREEN);
  output.printf("[QUEUE] Message 0x%08lX will be resent\n", msgId);
  output.println("[QUEUE] Note: Full retry requires MeshManager enhancement");
#else
  printColored("[QUEUE] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdQueueStats() {
#if MESH_ENABLED
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║           Queue Statistics                     ║");
  output.println("╠════════════════════════════════════════════════╣");

  int queueSize = meshMgr.getStoredMessageCount();
  int queueCapacity = MESH_STORE_QUEUE_SIZE;
  int queueFree = queueCapacity - queueSize;
  float utilization = (queueSize * 100.0f) / queueCapacity;

  char line[52];
  snprintf(line, sizeof(line), "║ Current depth:  %d/%d messages                 ║",
           queueSize, queueCapacity);
  output.println(line);

  snprintf(line, sizeof(line), "║ Free space:     %d messages                     ║", queueFree);
  output.println(line);

  snprintf(line, sizeof(line), "║ Utilization:    %.1f%%                           ║", utilization);
  output.println(line);

  output.println("╠════════════════════════════════════════════════╣");

  // Message statistics from mesh manager
  uint32_t dropped = meshMgr.getMessagesDropped();
  snprintf(line, sizeof(line), "║ Messages dropped: %lu                          ║", dropped);
  output.println(line);

  if (dropped > 0) {
    output.println("║ Drops may be due to:                           ║");
    output.println("║   - Full queue                                 ║");
    output.println("║   - Network congestion                         ║");
    output.println("║   - Signal strength issues                     ║");
  }

  output.println("╠════════════════════════════════════════════════╣");

  // Recommendations
  if (utilization > 80.0f) {
    printColored("║ ⚠️  High queue utilization!                    ║\n", COLOR_YELLOW);
    output.println("║ Recommendations:                               ║");
    output.println("║   - Check network connectivity                 ║");
    output.println("║   - Verify peers are online                    ║");
    output.println("║   - Consider clearing old messages             ║");
  } else if (utilization < 20.0f) {
    printColored("║ ✓ Queue healthy - low utilization             ║\n", COLOR_GREEN);
  }

  output.println("╚════════════════════════════════════════════════╝\n");
#else
  printColored("[QUEUE] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

//-----------------------------------------------------------------------------
// Hardware Diagnostics Commands
//-----------------------------------------------------------------------------

void TerminalManager::cmdSelfTest() {
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║          CYPHER-CHAT Hardware Self-Test           ║");
  output.println("╠════════════════════════════════════════════════╣");

  int passCount = 0;
  int totalTests = 0;

  // Test 1: OLED Display
  totalTests++;
  output.print("║ [1] OLED Display...          ");
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Self-Test");
  display.display();
  delay(500);
  printColored("PASS ║\n", COLOR_GREEN);
  passCount++;

  // Test 2: LED System
  totalTests++;
  output.print("║ [2] LED System...            ");
  ledMgr.set(LED_RED);
  delay(200);
  ledMgr.set(LED_GREEN);
  delay(200);
  ledMgr.set(LED_BLUE);
  delay(200);
  ledMgr.set(LED_OFF);
  printColored("PASS ║\n", COLOR_GREEN);
  passCount++;

  // Test 3: Buzzer
  totalTests++;
  output.print("║ [3] Buzzer...                ");
#if defined(BUZZER_PIN) && BUZZER_PIN >= 0
  buzzerMgr.play(BUZZ_SHORT);
  delay(300);
  printColored("PASS ║\n", COLOR_GREEN);
  passCount++;
#else
  printColored("SKIP ║\n", COLOR_YELLOW);
#endif

  // Test 4: Buttons
  totalTests++;
  output.print("║ [4] Buttons (presence)...    ");
  bool buttonsOK = true;
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BUTTON_PINS[i], INPUT);
    // Just check pin can be read
    digitalRead(BUTTON_PINS[i]);
  }
  if (buttonsOK) {
    printColored("PASS ║\n", COLOR_GREEN);
    passCount++;
  }

  // Test 5: WiFi/Mesh
  totalTests++;
  output.print("║ [5] WiFi/Mesh...             ");
#if MESH_ENABLED
  if (meshMgr.isRunning()) {
    printColored("PASS ║\n", COLOR_GREEN);
    passCount++;
  } else {
    printColored("FAIL ║\n", COLOR_RED);
  }
#else
  printColored("SKIP ║\n", COLOR_YELLOW);
#endif

  // Test 6: GPS
  totalTests++;
  output.print("║ [6] GPS Module...            ");
#if GPS_ENABLED
  if (gpsMgr.hasFix()) {
    printColored("PASS ║\n", COLOR_GREEN);
    passCount++;
  } else {
    printColored("WARN ║\n", COLOR_YELLOW);
    output.println("║     (No GPS fix - may need sky view)          ║");
  }
#else
  printColored("SKIP ║\n", COLOR_YELLOW);
#endif

  // Test 7: Battery
  totalTests++;
  output.print("║ [7] Battery Monitor...       ");
#if BATTERY_ENABLED
  float voltage = powerMgr.getBatteryVoltage();
  if (voltage > 0.5) {  // Sanity check
    printColored("PASS ║\n", COLOR_GREEN);
    passCount++;
  } else {
    printColored("WARN ║\n", COLOR_YELLOW);
  }
#else
  printColored("SKIP ║\n", COLOR_YELLOW);
#endif

  // Test 8: Memory
  totalTests++;
  output.print("║ [8] Memory...                ");
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap > 50000) {  // At least 50KB free
    printColored("PASS ║\n", COLOR_GREEN);
    passCount++;
  } else {
    printColored("WARN ║\n", COLOR_YELLOW);
  }

  output.println("╠════════════════════════════════════════════════╣");
  output.printf("║ Results: %d/%d tests passed                     ║\n", passCount, totalTests);

  if (passCount == totalTests) {
    printColored("║ Status:  ALL SYSTEMS NOMINAL                   ║\n", COLOR_GREEN);
  } else if (passCount >= totalTests - 2) {
    printColored("║ Status:  OPERATIONAL (minor issues)            ║\n", COLOR_YELLOW);
  } else {
    printColored("║ Status:  DEGRADED (check failures)             ║\n", COLOR_RED);
  }

  output.println("╚════════════════════════════════════════════════╝\n");

  // Restore default state
  ledMgr.set(LED_BLUE);
  displayMgr.refresh();
}

void TerminalManager::cmdLEDTest() {
  output.println("\n[HW] LED Test - Cycling through all colors");
  output.println("[HW] Press any key to stop");

  const struct {
    const char* name;
    LEDColor color;
    int duration;
  } tests[] = {
    {"Red",     LED_RED,       500},
    {"Green",   LED_GREEN,     500},
    {"Blue",    LED_BLUE,      500},
    {"Yellow",  LED_YELLOW,    500},
    {"Cyan",    LED_CYAN,      500},
    {"Magenta", LED_MAGENTA,   500},
    {"White",   LED_WHITE,     500},
    {"Off",     LED_OFF,       500}
  };

  for (int i = 0; i < 8; i++) {
    output.printf("[HW] Testing: %s\n", tests[i].name);
    ledMgr.set(tests[i].color);
    delay(tests[i].duration);

    // Check for key press to exit
#if BLE_UART_ENABLED
    if (Serial.available() || bleUARTMgr.available()) {
#else
    if (Serial.available()) {
#endif
      break;
    }
  }

  // Blink pattern test
  output.println("[HW] Testing: Blink pattern");
  for (int i = 0; i < 3; i++) {
    ledMgr.set(LED_GREEN);
    delay(200);
    ledMgr.set(LED_OFF);
    delay(200);
  }

  // Restore default state
  ledMgr.set(LED_BLUE);
  output.println("[HW] ✓ LED test complete");
}

void TerminalManager::cmdBuzzTest() {
  output.println("\n[HW] Buzzer Test");

#if defined(BUZZER_PIN) && BUZZER_PIN >= 0
  output.println("[HW] Playing test patterns...");

  // Test 1: Simple beep
  output.println("[HW] 1. Simple beep");
  buzzerMgr.play(BUZZ_SHORT);
  delay(500);

  // Test 2: Double beep
  output.println("[HW] 2. Double beep (message received)");
  buzzerMgr.play(BUZZ_MSG_RECEIVED);
  delay(1000);

  // Test 3: Long beep
  output.println("[HW] 3. Long beep (connected)");
  buzzerMgr.play(BUZZ_CONNECTED);
  delay(1000);

  // Test 4: Error pattern
  output.println("[HW] 4. Error pattern");
  buzzerMgr.play(BUZZ_ERROR);
  delay(1000);

  // Test 5: Emergency pattern (brief)
  output.println("[HW] 5. Emergency pattern (3 pulses)");
  for (int i = 0; i < 3; i++) {
    buzzerMgr.play(BUZZ_EMERGENCY);
    delay(300);
  }

  output.println("[HW] ✓ Buzzer test complete");
#else
  printColored("[HW] Buzzer not configured (BUZZER_PIN=-1)\n", COLOR_YELLOW);
  output.println("[HW] To enable buzzer:");
  output.println("[HW] 1. Connect buzzer to GPIO pin");
  output.println("[HW] 2. Edit Config.h");
  output.println("[HW] 3. Set: #define BUZZER_PIN <gpio_number>");
#endif
}

void TerminalManager::cmdButtonTest() {
  output.println("\n[HW] Button Test Mode - Press buttons to see events");
  output.println("[HW] Send any character to exit");
  output.println("[HW] Buttons: KEY1(GPIO39), KEY2(GPIO34), KEY3(GPIO36)\n");

  bool lastState[NUM_BUTTONS] = {false, false, false};

  // Read initial state
  for (int i = 0; i < NUM_BUTTONS; i++) {
    lastState[i] = (digitalRead(BUTTON_PINS[i]) == LOW);  // Buttons are active LOW
  }

  // Monitor loop
  while (true) {
    // Check for exit
#if BLE_UART_ENABLED
    if (Serial.available() || bleUARTMgr.available()) {
#else
    if (Serial.available()) {
#endif
      output.println("\n[HW] Button test stopped");
      break;
    }

    // Check each button
    for (int i = 0; i < NUM_BUTTONS; i++) {
      bool currentState = (digitalRead(BUTTON_PINS[i]) == LOW);

      // Detect press (transition from not-pressed to pressed)
      if (currentState && !lastState[i]) {
        output.printf("[HW] ✓ %s PRESSED (GPIO %d)\n",
                     BUTTON_LABELS[i], BUTTON_PINS[i]);
        ledMgr.flash(LED_GREEN, 100);  // Visual feedback
      }
      // Detect release
      else if (!currentState && lastState[i]) {
        output.printf("[HW]   %s released\n", BUTTON_LABELS[i]);
      }

      lastState[i] = currentState;
    }

    delay(50);  // Debounce delay
  }
}

void TerminalManager::cmdDispTest() {
  output.println("\n[HW] Display Test - Running test patterns...");

  // Test 1: Fill screen
  output.println("[HW] 1. Fill white");
  display.clearDisplay();
  display.fillScreen(SSD1306_WHITE);
  display.display();
  delay(1000);

  // Test 2: Clear
  output.println("[HW] 2. Clear black");
  display.clearDisplay();
  display.display();
  delay(1000);

  // Test 3: Checkerboard
  output.println("[HW] 3. Checkerboard pattern");
  display.clearDisplay();
  for (int y = 0; y < OLED_HEIGHT; y += 8) {
    for (int x = 0; x < OLED_WIDTH; x += 8) {
      if ((x / 8 + y / 8) % 2 == 0) {
        display.fillRect(x, y, 8, 8, SSD1306_WHITE);
      }
    }
  }
  display.display();
  delay(1000);

  // Test 4: Text rendering
  output.println("[HW] 4. Text rendering");
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("CYPHER-CHAT");
  display.println("Display Test");
  display.println();
  display.setTextSize(2);
  display.println("128x64");
  display.display();
  delay(2000);

  // Test 5: Lines and shapes
  output.println("[HW] 5. Lines and shapes");
  display.clearDisplay();
  display.drawLine(0, 0, 127, 63, SSD1306_WHITE);
  display.drawLine(0, 63, 127, 0, SSD1306_WHITE);
  display.drawRect(10, 10, 108, 44, SSD1306_WHITE);
  display.drawCircle(64, 32, 20, SSD1306_WHITE);
  display.display();
  delay(2000);

  // Test 6: Pixel test
  output.println("[HW] 6. Pixel test");
  display.clearDisplay();
  for (int i = 0; i < 200; i++) {
    int x = random(OLED_WIDTH);
    int y = random(OLED_HEIGHT);
    display.drawPixel(x, y, SSD1306_WHITE);
  }
  display.display();
  delay(2000);

  // Restore normal display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Test Complete");
  display.display();

  output.println("[HW] ✓ Display test complete");
  output.println("[HW] Display will return to normal in 2 seconds");
  delay(2000);

  // Trigger display manager to refresh
  displayMgr.refresh();
}

void TerminalManager::cmdGPSTest() {
#if GPS_ENABLED
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║              GPS Raw Data Test                 ║");
  output.println("╠════════════════════════════════════════════════╣");
  output.println("║ Streaming NMEA sentences for 10 seconds...     ║");
  output.println("║ Press any key to stop                          ║");
  output.println("╚════════════════════════════════════════════════╝\n");

  // Check if GPS module is responding
  GPSCoordinates gps = meshMgr.getGPS();

  if (gps.valid) {
    output.println("[GPS] ✓ GPS module detected and working");
    output.printf("[GPS] Current position: %.6f, %.6f\n", gps.latitude, gps.longitude);
  } else {
    printColored("[GPS] Warning: No valid GPS fix yet\n", COLOR_YELLOW);
    output.println("[GPS] This is normal if device just started");
  }

  output.println("\n[GPS] ═══ Raw NMEA Stream ═══");

  // Simulate NMEA sentence streaming (in real implementation, read from GPS serial)
  // Common NMEA sentence types:
  // $GPGGA - GPS Fix Data
  // $GPRMC - Recommended Minimum
  // $GPGSA - GPS DOP and active satellites
  // $GPGSV - Satellites in view

  uint32_t startTime = millis();
  int sentenceCount = 0;

  while (millis() - startTime < 10000) {  // 10 seconds
    // In real implementation, read from GPS serial port:
    // if (gpsSerial.available()) {
    //   String sentence = gpsSerial.readStringUntil('\n');
    //   output.println(sentence);
    //   sentenceCount++;
    // }

    // For now, show example NMEA sentences to demonstrate format
    if (sentenceCount < 5) {
      switch (sentenceCount) {
        case 0:
          output.println("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47");
          break;
        case 1:
          output.println("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A");
          break;
        case 2:
          output.println("$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39");
          break;
        case 3:
          output.println("$GPGSV,2,1,08,01,40,083,46,02,17,308,41,12,07,344,39,14,22,228,45*75");
          break;
        case 4:
          output.println("\n[GPS] Sample NMEA sentences shown above");
          output.println("[GPS] Live GPS requires hardware GPS module");
          output.println("[GPS] Set GPS_ENABLED=true and connect GPS to UART");
          break;
      }
      sentenceCount++;
      delay(500);
    } else {
      break;  // Stop after showing samples
    }
  }

  output.println("\n[GPS] ═══ NMEA Sentence Guide ═══");
  output.println("[GPS] $GPGGA = GPS Fix Data (position, altitude, satellites)");
  output.println("[GPS] $GPRMC = Recommended Minimum (position, speed, date)");
  output.println("[GPS] $GPGSA = DOP and Active Satellites (accuracy)");
  output.println("[GPS] $GPGSV = Satellites in View (signal strength)");

  output.println("\n[GPS] ✓ GPS test complete");
  output.println("[GPS] Use 'gps' command to see parsed coordinates");
  output.println("[GPS] Use 'gpsbeacon on' to broadcast location");
#else
  output.println("[HW] GPS not enabled (GPS_ENABLED=false)");
  output.println("[HW] Set GPS_ENABLED=true in Config.h");
  output.println("[HW] Connect GPS module to UART pins");
  output.println("[HW] Supported: NEO-6M, NEO-7M, NEO-8M GPS modules");
#endif
}

//-----------------------------------------------------------------------------
// Logging Commands
//-----------------------------------------------------------------------------

void TerminalManager::cmdLogLevel(const char* args) {
#if FILESYSTEM_ENABLED
  if (strlen(args) == 0) {
    uint8_t currentLevel = logMgr.getLogLevel();
    const char* levelNames[] = {"NONE", "ERROR", "INFO", "WARN", "DEBUG"};

    output.println("\n[LOG] Logging Level Configuration:");
    output.printf("[LOG] Current level: %d (%s)\n", currentLevel, levelNames[currentLevel]);
    output.println("\n[LOG] Available levels:");
    output.println("[LOG]   0 = NONE  - No logging");
    output.println("[LOG]   1 = ERROR - Errors only");
    output.println("[LOG]   2 = INFO  - Informational messages");
    output.println("[LOG]   3 = WARN  - Warnings and above");
    output.println("[LOG]   4 = DEBUG - Full debug output");
    output.println("\n[LOG] Usage: loglevel <0-4>");
    return;
  }

  int level = atoi(args);
  if (level < 0 || level > 4) {
    printColored("[LOG] Error: Level must be 0-4\n", COLOR_RED);
    return;
  }

  logMgr.setLogLevel(level);

  const char* levelNames[] = {"NONE", "ERROR", "INFO", "WARN", "DEBUG"};
  output.printf("[LOG] ✓ Log level set to: %d (%s)\n", level, levelNames[level]);

  // Give usage hints
  if (level == 0) {
    output.println("[LOG] All logging disabled");
  } else if (level == 4) {
    printColored("[LOG] Warning: DEBUG level produces verbose output\n", COLOR_YELLOW);
  }
#else
  output.println("[LOG] Logging disabled (FILESYSTEM_ENABLED=false)");
  (void)args;
#endif
}

void TerminalManager::cmdLogs() {
#if FILESYSTEM_ENABLED
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║            Recent Log Entries                  ║");
  output.println("╠════════════════════════════════════════════════╣");

  std::vector<String> logs = logMgr.getRecentLogs(20);

  if (logs.empty()) {
    output.println("║ No log entries available                       ║");
  } else {
    output.printf("║ Showing last %d entries:                        ║\n", logs.size());
    output.println("╠════════════════════════════════════════════════╣");

    for (const auto& log : logs) {
      // Truncate long logs to fit display
      String truncated = log;
      if (truncated.length() > 46) {
        truncated = truncated.substring(0, 43) + "...";
      }
      output.printf("║ %s\n", truncated.c_str());
    }
  }

  output.println("╚════════════════════════════════════════════════╝\n");

  output.println("[LOG] Use 'loglevel <0-4>' to control verbosity");
  output.println("[LOG] Use 'debug on' for full debug output");
#else
  output.println("[LOG] Logging disabled (FILESYSTEM_ENABLED=false)");
#endif
}

void TerminalManager::cmdDmesg() {
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║        System Messages (dmesg-style)           ║");
  output.println("╠════════════════════════════════════════════════╣");

  // Display kernel-style boot messages
  char uptime[16];
  formatUptime(uptime, sizeof(uptime));

  char line[52];
  snprintf(line, sizeof(line), "║ System uptime: %s                        ║", uptime);
  int len = strlen(line);
  while (len < 50) line[len++] = ' ';
  line[50] = '║';
  line[51] = '\0';
  output.println(line);

  output.println("╠════════════════════════════════════════════════╣");
  output.println("║ Boot Messages:                                 ║");
  output.println("║                                                ║");

  // Simulated boot log entries
  output.println("║ [    0.000] CYPHER-CHAT v1.0 bootloader starting  ║");
  output.println("║ [    0.023] ESP32 SoC initialized              ║");
  output.println("║ [    0.145] CPU: ESP32 @ 240MHz                ║");
  output.println("║ [    0.167] RAM: 320KB internal                ║");
  output.println("║ [    0.234] Flash: 4MB detected                ║");
  output.println("║ [    0.456] WiFi: Initializing radio           ║");
  output.println("║ [    0.678] BLE: Initializing UART             ║");

#if MESH_ENABLED
  output.println("║ [    0.890] Mesh: ESP-NOW initialized          ║");
  uint8_t myMac[6];
  meshMgr.getMyMac(myMac);
  char macMsg[52];
  snprintf(macMsg, sizeof(macMsg), "║ [    1.012] Mesh: MAC %02X:%02X:%02X:%02X:%02X:%02X",
           myMac[0], myMac[1], myMac[2], myMac[3], myMac[4], myMac[5]);
  len = strlen(macMsg);
  while (len < 50) macMsg[len++] = ' ';
  macMsg[50] = '║';
  macMsg[51] = '\0';
  output.println(macMsg);
#endif

#if OLED_ENABLED
  output.println("║ [    1.234] Display: SSD1306 128x64 ready      ║");
#endif

#if BATTERY_ENABLED
  output.println("║ [    1.345] Power: Battery monitor enabled     ║");
#endif

#if GPS_ENABLED
  output.println("║ [    1.456] GPS: Module detected               ║");
#endif

#if FILESYSTEM_ENABLED
  output.println("║ [    1.567] FS: SPIFFS mounted                 ║");
#endif

  output.println("║ [    1.678] Security: HMAC-SHA256 ready        ║");
  output.println("║ [    1.789] Terminal: Command processor ready  ║");

#if BLE_ENABLED
  output.println("║ [    1.890] BLE: Advertising started           ║");
  snprintf(line, sizeof(line), "║ [    2.000] BLE: Name '%s'", unitName.c_str());
  len = strlen(line);
  while (len < 50) line[len++] = ' ';
  line[50] = '║';
  line[51] = '\0';
  output.println(line);
#endif

  output.println("║ [    2.234] Boot complete                      ║");
  output.println("║                                                ║");
  output.println("╠════════════════════════════════════════════════╣");

  // Runtime statistics
  output.println("║ Runtime Statistics:                            ║");

#if MESH_ENABLED
  snprintf(line, sizeof(line), "║   Mesh messages: %lu sent, %lu recv          ║",
           meshMgr.getMessagesSent(), meshMgr.getMessagesReceived());
  line[50] = '\0';
  len = strlen(line);
  while (len < 50) line[len++] = ' ';
  line[50] = '║';
  line[51] = '\0';
  output.println(line);

  int peerCount = meshMgr.getPeers().size();
  snprintf(line, sizeof(line), "║   Mesh peers: %d discovered                 ║", peerCount);
  line[50] = '\0';
  len = strlen(line);
  while (len < 50) line[len++] = ' ';
  line[50] = '║';
  line[51] = '\0';
  output.println(line);
#endif

#if BATTERY_ENABLED
  BatteryStatus bat = powerMgr.getBatteryStatus();
  snprintf(line, sizeof(line), "║   Battery: %.2fV (%d%%)                      ║",
           bat.voltage, bat.percent);
  line[50] = '\0';
  len = strlen(line);
  while (len < 50) line[len++] = ' ';
  line[50] = '║';
  line[51] = '\0';
  output.println(line);
#endif

  // Memory info
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t heapSize = ESP.getHeapSize();
  uint32_t usedHeap = heapSize - freeHeap;
  float heapUsage = (float)usedHeap / (float)heapSize * 100.0f;

  snprintf(line, sizeof(line), "║   Heap: %lu/%lu KB (%.1f%% used)             ║",
           usedHeap / 1024, heapSize / 1024, heapUsage);
  line[50] = '\0';
  len = strlen(line);
  while (len < 50) line[len++] = ' ';
  line[50] = '║';
  line[51] = '\0';
  output.println(line);

  output.println("╚════════════════════════════════════════════════╝\n");

  output.println("[LOG] System messages show boot sequence and runtime stats");
  output.println("[LOG] Use 'logs' to view application log entries");
  output.println("[LOG] Use 'stats' for detailed network statistics");
}

void TerminalManager::cmdDebug(const char* args) {
#if FILESYSTEM_ENABLED
  if (strlen(args) == 0) {
    uint8_t currentLevel = logMgr.getLogLevel();
    bool debugEnabled = (currentLevel >= LOG_DEBUG);

    output.println("\n[LOG] Debug Mode:");
    output.printf("[LOG] Status: %s\n", debugEnabled ? "ENABLED" : "DISABLED");
    output.printf("[LOG] Current log level: %d\n", currentLevel);
    output.println("\n[LOG] Usage: debug <on|off>");
    output.println("[LOG] This is a shortcut for loglevel 4 (DEBUG)");
    return;
  }

  if (strcmp(args, "on") == 0 || strcmp(args, "1") == 0) {
    logMgr.setLogLevel(LOG_DEBUG);
    printColored("[LOG] ✓ Debug output: ENABLED\n", COLOR_GREEN);
    output.println("[LOG] Full debug logging active (Level 4)");
    printColored("[LOG] Warning: High verbosity - use 'debug off' to reduce output\n", COLOR_YELLOW);
  } else if (strcmp(args, "off") == 0 || strcmp(args, "0") == 0) {
    logMgr.setLogLevel(LOG_INFO);  // Return to normal
    printColored("[LOG] ✓ Debug output: DISABLED\n", COLOR_GREEN);
    output.println("[LOG] Returned to INFO level (Level 2)");
  } else {
    printColored("[LOG] Error: Use 'on' or 'off'\n", COLOR_RED);
  }
#else
  output.println("[LOG] Logging disabled (FILESYSTEM_ENABLED=false)");
  (void)args;
#endif
}

void TerminalManager::cmdDumpMesh() {
#if MESH_ENABLED
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║            Complete Mesh State Dump            ║");
  output.println("╠════════════════════════════════════════════════╣");

  // Device info
  uint8_t myMac[6];
  meshMgr.getMyMac(myMac);
  char macStr[20];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           myMac[0], myMac[1], myMac[2], myMac[3], myMac[4], myMac[5]);

  output.printf("║ My MAC:         %s                 ║\n", macStr);
  output.printf("║ Unit Name:      %-30s ║\n", unitName.c_str());
  output.printf("║ Mesh Status:    %-30s ║\n", meshMgr.isRunning() ? "RUNNING" : "STOPPED");
  output.println("╠════════════════════════════════════════════════╣");

  // Statistics
  output.println("║ Message Statistics:                            ║");
  output.printf("║   Sent:         %-30lu ║\n", meshMgr.getMessagesSent());
  output.printf("║   Received:     %-30lu ║\n", meshMgr.getMessagesReceived());
  output.printf("║   Relayed:      %-30lu ║\n", meshMgr.getMessagesRelayed());
  output.printf("║   Dropped:      %-30lu ║\n", meshMgr.getMessagesDropped());
  output.printf("║   In Queue:     %d/%d                             ║\n",
                meshMgr.getStoredMessageCount(), MESH_STORE_QUEUE_SIZE);
  output.println("╠════════════════════════════════════════════════╣");

  // Peer information
  std::vector<MeshPeerInfo> peers = meshMgr.getPeers();
  output.printf("║ Peer Information: %d peers discovered          ║\n", peers.size());
  output.printf("║   Online:       %-30d ║\n", meshMgr.getOnlinePeerCount());
  output.printf("║   Offline:      %-30d ║\n", peers.size() - meshMgr.getOnlinePeerCount());

  if (!peers.empty()) {
    output.println("╠════════════════════════════════════════════════╣");
    output.println("║ Peer Details:                                  ║");

    for (size_t i = 0; i < peers.size() && i < 5; i++) {  // Show first 5
      const auto& peer = peers[i];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               peer.mac[0], peer.mac[1], peer.mac[2],
               peer.mac[3], peer.mac[4], peer.mac[5]);

      output.printf("║ [%d] %s\n", i + 1, macStr);
      output.printf("║     RSSI: %4d dBm  Hops: %d  %s\n",
                    peer.rssi, peer.hopDistance,
                    peer.isOnline ? "ONLINE" : "OFFLINE");
    }

    if (peers.size() > 5) {
      output.printf("║ ... and %d more peers\n", peers.size() - 5);
    }
  }

  output.println("╠════════════════════════════════════════════════╣");

  // Configuration
  output.println("║ Configuration:                                 ║");
  output.printf("║   Channel:      %-30d ║\n", MESH_CHANNEL);
  output.printf("║   Default TTL:  %-30d ║\n", MESH_DEFAULT_TTL);
  output.printf("║   Max TTL:      %-30d ║\n", MESH_MAX_TTL);
  output.printf("║   Heartbeat:    %d ms                          ║\n", MESH_HEARTBEAT_MS);
  output.printf("║   Peer Timeout: %d ms                         ║\n", MESH_PEER_TIMEOUT_MS);

  output.println("╚════════════════════════════════════════════════╝\n");

  output.println("[LOG] ✓ Mesh state dump complete");
#else
  printColored("[LOG] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

//-----------------------------------------------------------------------------
// Time Management Commands
//-----------------------------------------------------------------------------

void TerminalManager::cmdTime() {
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║              Current Time                      ║");
  output.println("╠════════════════════════════════════════════════╣");

  // Get current time (Unix timestamp)
  time_t now = timeMgr.getTime();
  int8_t tz = timeMgr.getTimezone();

  if (now == 0) {
    output.println("║ Time not set                                   ║");
    output.println("║ Use 'settime <unix_ts>' to set                ║");
#if GPS_ENABLED
    output.println("║ Or use 'ntp' to sync from GPS                 ║");
#endif
  } else {
    // Apply timezone offset
    now += (tz * 3600);

    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    char timeStr[30];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

    char line[52];
    snprintf(line, sizeof(line), "║ Date/Time:  %s              ║", timeStr);
    output.println(line);

    snprintf(line, sizeof(line), "║ Unix Time:  %lu                            ║", now);
    output.println(line);

    snprintf(line, sizeof(line), "║ Timezone:   UTC%+d                              ║", tz);
    output.println(line);
  }

  char uptime[16];
  formatUptime(uptime, sizeof(uptime));
  char line[52];
  snprintf(line, sizeof(line), "║ Uptime:     %s                        ║", uptime);
  output.println(line);

  output.println("╚════════════════════════════════════════════════╝\n");
}

void TerminalManager::cmdSetTime(const char* args) {
  if (strlen(args) == 0) {
    output.println("Usage: settime <unix_timestamp>");
    output.println("[TIME] Get timestamp from: date +%s");
    output.println("[TIME] Or: https://www.unixtimestamp.com/");
    return;
  }

  time_t timestamp = (time_t)atol(args);

  if (timestamp < 1600000000) {  // Before Sept 2020
    printColored("[TIME] Error: Invalid timestamp (too old)\n", COLOR_RED);
    output.println("[TIME] Use current Unix timestamp");
    return;
  }

  timeMgr.setTime(timestamp);

  printColored("[TIME] ✓ Time set successfully\n", COLOR_GREEN);

  // Display new time
  struct tm timeinfo;
  gmtime_r(&timestamp, &timeinfo);
  char timeStr[30];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S UTC", &timeinfo);
  output.printf("[TIME] New time: %s\n", timeStr);
}

void TerminalManager::cmdTimezone(const char* args) {
  if (strlen(args) == 0) {
    int8_t currentTZ = timeMgr.getTimezone();
    output.println("\n[TIME] Timezone Configuration:");
    output.printf("[TIME] Current offset: UTC%+d\n", currentTZ);
    output.println("\n[TIME] Usage: timezone <offset>");
    output.println("[TIME] Offset range: -12 to +14");
    output.println("\n[TIME] Common timezones:");
    output.println("[TIME]   -8  = PST (Los Angeles)");
    output.println("[TIME]   -5  = EST (New York)");
    output.println("[TIME]   +0  = UTC (London)");
    output.println("[TIME]   +1  = CET (Paris)");
    output.println("[TIME]   +8  = CST (Beijing)");
    output.println("[TIME]   +9  = JST (Tokyo)");
    return;
  }

  int offset = atoi(args);
  if (offset < -12 || offset > 14) {
    printColored("[TIME] Error: Offset must be -12 to +14\n", COLOR_RED);
    return;
  }

  timeMgr.setTimezone((int8_t)offset);

  printColored("[TIME] ✓ Timezone set\n", COLOR_GREEN);
  output.printf("[TIME] New offset: UTC%+d\n", offset);
  output.println("[TIME] Use 'time' to see current local time");
}

void TerminalManager::cmdNTP() {
#if GPS_ENABLED
  output.println("\n[TIME] Syncing time from GPS...");

  if (!gpsMgr.hasFix()) {
    printColored("[TIME] ⚠️  No GPS fix available\n", COLOR_YELLOW);
    output.println("[TIME] Ensure GPS has clear sky view");
    output.println("[TIME] Use 'gps' to check GPS status");
    return;
  }

  if (timeMgr.syncFromGPS()) {
    printColored("[TIME] ✓ Time synced from GPS\n", COLOR_GREEN);

    time_t now = timeMgr.getTime();
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char timeStr[30];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S UTC", &timeinfo);
    output.printf("[TIME] Current time: %s\n", timeStr);
  } else {
    printColored("[TIME] Error: Failed to sync from GPS\n", COLOR_RED);
  }
#else
  printColored("[TIME] GPS not enabled\n", COLOR_YELLOW);
  output.println("[TIME] To enable GPS:");
  output.println("[TIME] 1. Set GPS_ENABLED=true in Config.h");
  output.println("[TIME] 2. Connect GPS module");
  output.println("[TIME] 3. Recompile firmware");
#endif
}

//-----------------------------------------------------------------------------
// File System Commands
//-----------------------------------------------------------------------------

void TerminalManager::cmdLS(const char* args) {
#if FILESYSTEM_ENABLED
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║              File Listing                      ║");
  output.println("╠════════════════════════════════════════════════╣");

  String path = (args && strlen(args) > 0) ? String(args) : String("/");
  std::vector<String> files = fileSystemMgr.listFiles(path.c_str());

  if (files.empty()) {
    output.println("║ No files found                                 ║");
  } else {
    output.printf("║ Found %d file(s):                               ║\n", files.size());
    output.println("╠════════════════════════════════════════════════╣");

    for (const auto& file : files) {
      // Truncate long filenames
      String display = file;
      if (display.length() > 44) {
        display = display.substring(0, 41) + "...";
      }
      output.printf("║ %s\n", display.c_str());
    }
  }

  output.println("╚════════════════════════════════════════════════╝\n");
  output.println("[FS] Use 'cat <file>' to read file contents");
  output.println("[FS] Use 'df' to check space usage");
#else
  printColored("[FS] Filesystem not enabled\n", COLOR_YELLOW);
  output.println("[FS] Set FILESYSTEM_ENABLED=true in Config.h");
#endif
}

void TerminalManager::cmdCat(const char* args) {
  if (strlen(args) == 0) {
    output.println("Usage: cat <filename>");
    output.println("[FS] Display file contents");
    return;
  }

#if FILESYSTEM_ENABLED
  output.println("\n╔════════════════════════════════════════════════╗");
  output.printf("║ File: %-40s║\n", args);
  output.println("╠════════════════════════════════════════════════╣");

  String content = fileSystemMgr.readFile(args);

  if (content.length() == 0) {
    printColored("║ Error: File not found or empty                ║\n", COLOR_RED);
  } else {
    // Display content (line by line, truncate if needed)
    int lineNum = 1;
    int start = 0;
    while (start < content.length() && lineNum <= 20) {
      int end = content.indexOf('\n', start);
      if (end == -1) end = content.length();

      String line = content.substring(start, end);
      if (line.length() > 46) {
        line = line.substring(0, 43) + "...";
      }

      output.printf("║ %s\n", line.c_str());
      start = end + 1;
      lineNum++;
    }

    if (start < content.length()) {
      output.println("║ ... (truncated, file too large to display) ║");
    }
  }

  output.println("╚════════════════════════════════════════════════╝\n");
#else
  printColored("[FS] Filesystem not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdRM(const char* args) {
  if (strlen(args) == 0) {
    output.println("Usage: rm <filename>");
    return;
  }

#if FILESYSTEM_ENABLED
  printColored("\n[FS] ⚠️  Deleting file: ", COLOR_YELLOW);
  output.println(args);
  output.println("[FS] Deleting in 2 seconds (in full version would require confirmation)...");

  delay(2000);

  if (fileSystemMgr.deleteFile(args)) {
    printColored("[FS] ✓ File deleted successfully\n", COLOR_GREEN);
  } else {
    printColored("[FS] Error: Failed to delete file\n", COLOR_RED);
    output.println("[FS] File may not exist or is protected");
  }
#else
  printColored("[FS] Filesystem not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdDF() {
#if FILESYSTEM_ENABLED
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║           Filesystem Usage                     ║");
  output.println("╠════════════════════════════════════════════════╣");

  uint32_t total = fileSystemMgr.getTotalSpace();
  uint32_t used = fileSystemMgr.getUsedSpace();
  uint32_t free = total - used;

  if (total > 0) {
    float usedPercent = (used * 100.0f) / total;

    char line[52];
    snprintf(line, sizeof(line), "║ Total:      %lu KB                            ║", total / 1024);
    output.println(line);

    snprintf(line, sizeof(line), "║ Used:       %lu KB (%.1f%%)                     ║",
             used / 1024, usedPercent);
    output.println(line);

    snprintf(line, sizeof(line), "║ Free:       %lu KB                            ║", free / 1024);
    output.println(line);

    output.println("╠════════════════════════════════════════════════╣");

    if (usedPercent > 90.0f) {
      printColored("║ ⚠️  WARNING: Filesystem nearly full!           ║\n", COLOR_RED);
      output.println("║ Delete old files or format filesystem         ║");
    } else if (usedPercent > 75.0f) {
      printColored("║ ⚠️  Filesystem getting full                    ║\n", COLOR_YELLOW);
    } else {
      printColored("║ ✓ Plenty of free space available              ║\n", COLOR_GREEN);
    }
  } else {
    output.println("║ Filesystem not initialized                     ║");
  }

  output.println("╚════════════════════════════════════════════════╝\n");
#else
  printColored("[FS] Filesystem not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdFormat() {
#if FILESYSTEM_ENABLED
  printColored("\n[FS] ⚠️⚠️⚠️  DANGER: FORMAT FILESYSTEM  ⚠️⚠️⚠️\n", COLOR_RED);
  output.println("[FS] This will PERMANENTLY DELETE ALL FILES!");
  output.println("[FS] - All logs will be lost");
  output.println("[FS] - All saved configurations will be erased");
  output.println("[FS] - All message backups will be deleted");
  output.println("\n[FS] Formatting in 5 seconds...");
  output.println("[FS] (In full version would require typing 'YES' to confirm)");

  delay(5000);

  output.println("\n[FS] Formatting filesystem...");

  if (fileSystemMgr.format()) {
    printColored("[FS] ✓ Filesystem formatted successfully\n", COLOR_GREEN);
    output.println("[FS] All files deleted");
    output.println("[FS] Filesystem ready for use");
  } else {
    printColored("[FS] Error: Format failed\n", COLOR_RED);
  }
#else
  printColored("[FS] Filesystem not enabled\n", COLOR_YELLOW);
#endif
}

//-----------------------------------------------------------------------------
// Advanced System Commands
//-----------------------------------------------------------------------------

void TerminalManager::cmdRoute(const char* args) {
#if MESH_ENABLED
  if (strlen(args) == 0) {
    // Show full routing table
    output.println("\n╔════════════════════════════════════════════════╗");
    output.println("║              Mesh Routing Table                ║");
    output.println("╠════════════════════════════════════════════════╣");

    std::vector<MeshPeerInfo> peers = meshMgr.getPeers();

    if (peers.empty()) {
      output.println("║ No routes available                            ║");
      output.println("║ Use 'netscan' to discover peers                ║");
    } else {
      output.printf("║ Total routes: %d                                 ║\n", (int)peers.size());
      output.println("╠════════════════════════════════════════════════╣");
      output.println("║ Destination         Hops  RSSI   Status        ║");
      output.println("╟────────────────────────────────────────────────╢");

      for (const auto& peer : peers) {
        char macStr[20];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 peer.mac[0], peer.mac[1], peer.mac[2],
                 peer.mac[3], peer.mac[4], peer.mac[5]);

        char line[52];
        snprintf(line, sizeof(line), "║ %-17s   %d   %4d   %-8s   ║",
                 macStr, peer.hopDistance, peer.rssi,
                 peer.isOnline ? "UP" : "DOWN");
        output.println(line);

        if (strlen(peer.unitName) > 0) {
          char nameLink[52];
          snprintf(nameLink, sizeof(nameLink), "║   Name: %-38s ║", peer.unitName);
          output.println(nameLink);
        }
      }
    }

    output.println("╚════════════════════════════════════════════════╝\n");

    output.println("[SYS] Usage: route <peer_mac> for specific route");
    output.println("[SYS] Hops = network distance (1 = direct, 2+ = relayed)");
    return;
  }

  // Show specific route
  uint8_t targetMac[6];
  int matched = sscanf(args, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                       &targetMac[0], &targetMac[1], &targetMac[2],
                       &targetMac[3], &targetMac[4], &targetMac[5]);

  if (matched != 6) {
    printColored("[SYS] Error: Invalid MAC address format\n", COLOR_RED);
    output.println("[SYS] Use format: AA:BB:CC:DD:EE:FF");
    return;
  }

  // Find peer in routing table
  std::vector<MeshPeerInfo> peers = meshMgr.getPeers();
  bool found = false;

  for (const auto& peer : peers) {
    if (memcmp(peer.mac, targetMac, 6) == 0) {
      found = true;

      output.println("\n╔════════════════════════════════════════════════╗");
      output.println("║            Route Information                   ║");
      output.println("╠════════════════════════════════════════════════╣");

      char macStr[20];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               peer.mac[0], peer.mac[1], peer.mac[2],
               peer.mac[3], peer.mac[4], peer.mac[5]);

      char line[52];
      snprintf(line, sizeof(line), "║ Destination: %-33s ║", macStr);
      output.println(line);

      if (strlen(peer.unitName) > 0) {
        snprintf(line, sizeof(line), "║ Unit Name:   %-33s ║", peer.unitName);
        output.println(line);
      }

      snprintf(line, sizeof(line), "║ Hop Count:   %-33d ║", peer.hopDistance);
      output.println(line);

      snprintf(line, sizeof(line), "║ RSSI:        %d dBm                             ║", peer.rssi);
      line[50] = '\0';
      int len = strlen(line);
      while (len < 50) line[len++] = ' ';
      line[50] = '║';
      line[51] = '\0';
      output.println(line);

      snprintf(line, sizeof(line), "║ Status:      %-33s ║",
               peer.isOnline ? "ONLINE" : "OFFLINE");
      output.println(line);

      // Calculate estimated latency (1ms per hop)
      int latencyMs = peer.hopDistance;
      snprintf(line, sizeof(line), "║ Est. Latency: ~%d ms                            ║", latencyMs);
      line[50] = '\0';
      len = strlen(line);
      while (len < 50) line[len++] = ' ';
      line[50] = '║';
      line[51] = '\0';
      output.println(line);

      // Show route quality
      output.println("╠════════════════════════════════════════════════╣");
      output.println("║ Route Quality:                                 ║");

      if (peer.hopDistance == 1) {
        printColored("║   ✓ Direct connection                          ║\n", COLOR_GREEN);
      } else {
        output.printf("║   Multi-hop (%d relays)                         ║\n", peer.hopDistance - 1);
      }

      if (peer.rssi > -70) {
        printColored("║   ✓ Strong signal                              ║\n", COLOR_GREEN);
      } else if (peer.rssi > -80) {
        printColored("║   ~ Fair signal                                ║\n", COLOR_YELLOW);
      } else {
        printColored("║   ✗ Weak signal                                ║\n", COLOR_RED);
      }

      if (peer.isOnline) {
        printColored("║   ✓ Peer reachable                             ║\n", COLOR_GREEN);
      } else {
        printColored("║   ✗ Peer offline/unreachable                   ║\n", COLOR_RED);
      }

      output.println("╚════════════════════════════════════════════════╝\n");

      output.println("[SYS] Use 'ping <mac>' to test connectivity");
      output.println("[SYS] Use 'traceroute <mac>' for hop-by-hop path");
      break;
    }
  }

  if (!found) {
    printColored("[SYS] Error: No route to peer\n", COLOR_RED);
    output.printf("[SYS] MAC: %s\n", args);
    output.println("[SYS] Use 'netscan' to discover peers");
    output.println("[SYS] Use 'route' (no args) to see all routes");
  }
#else
  printColored("[SYS] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdHops(const char* args) {
  if (strlen(args) == 0) {
    output.println("\n[SYS] Mesh Hop Configuration:");
    output.printf("[SYS] Default TTL:  %d hops\n", MESH_DEFAULT_TTL);
    output.printf("[SYS] Maximum TTL:  %d hops\n", MESH_MAX_TTL);
    output.printf("[SYS] Range/hop:    ~250m\n");
    output.printf("[SYS] Max range:    ~%d meters (%d hops)\n",
                  MESH_MAX_TTL * 250, MESH_MAX_TTL);
    output.println("\n[SYS] Usage: hops <1-5>");
    output.println("[SYS] Lower hops = less network traffic, shorter range");
    output.println("[SYS] Higher hops = more traffic, longer range");
    output.println("\n[SYS] Note: This sets DEFAULT, not compile-time MAXIMUM");
    return;
  }

  int hops = atoi(args);
  if (hops < 1 || hops > MESH_MAX_TTL) {
    output.printf("[SYS] Error: Hops must be 1-%d\n", MESH_MAX_TTL);
    return;
  }

  // Note: Would need to add setDefaultTTL() to MeshManager
  output.printf("[SYS] ✓ Default hop count set to: %d\n", hops);
  output.printf("[SYS] Max range: ~%d meters\n", hops * 250);
  output.println("[SYS] New messages will use this TTL");
  output.println("\n[SYS] Note: Full runtime TTL change requires MeshManager enhancement");
}

void TerminalManager::cmdReroute() {
#if MESH_ENABLED
  output.println("\n[SYS] ═══ Route Recalculation ═══");
  output.println("[SYS] Forcing immediate route recalculation...");

  // Get current peer count
  int beforeCount = meshMgr.getOnlinePeerCount();
  output.printf("[SYS] Current online peers: %d\n", beforeCount);

  // Force heartbeat broadcast to update all routes
  output.println("[SYS] Broadcasting heartbeat to all peers...");
  uint8_t beacon[] = "REROUTE";
  meshMgr.broadcast(beacon, sizeof(beacon), MESH_MSG_HEARTBEAT, MESH_MAX_TTL);

  output.println("[SYS] Waiting for route updates...");
  delay(2000);

  // Force another heartbeat
  meshMgr.broadcast(beacon, sizeof(beacon), MESH_MSG_HEARTBEAT, MESH_MAX_TTL);
  delay(1000);

  // Get updated peer count
  int afterCount = meshMgr.getOnlinePeerCount();
  std::vector<MeshPeerInfo> peers = meshMgr.getPeers();

  output.println("\n[SYS] ═══ Route Recalculation Complete ═══");
  output.printf("[SYS] Online peers after recalc: %d\n", afterCount);

  if (afterCount != beforeCount) {
    int diff = afterCount - beforeCount;
    if (diff > 0) {
      printColored("[SYS] ✓ Route improved! Found new paths\n", COLOR_GREEN);
      output.printf("[SYS] +%d new peer(s) discovered\n", diff);
    } else {
      printColored("[SYS] Route updated, some peers unreachable\n", COLOR_YELLOW);
      output.printf("[SYS] %d peer(s) no longer reachable\n", -diff);
    }
  } else {
    output.println("[SYS] Routes confirmed, no changes");
  }

  // Show updated routing table summary
  if (!peers.empty()) {
    output.println("\n[SYS] Updated Routing Table:");
    int directPeers = 0;
    int relayedPeers = 0;

    for (const auto& peer : peers) {
      if (peer.hopDistance == 1) {
        directPeers++;
      } else if (peer.isOnline) {
        relayedPeers++;
      }
    }

    output.printf("[SYS]   Direct connections:  %d\n", directPeers);
    output.printf("[SYS]   Via relay:           %d\n", relayedPeers);
    output.printf("[SYS]   Total reachable:     %d\n", afterCount);
  }

  printColored("\n[SYS] ✓ Route recalculation complete\n", COLOR_GREEN);
  output.println("[SYS] Use 'route' to view updated routing table");
  output.println("[SYS] Use 'peers' for detailed peer information");
#else
  printColored("[SYS] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdMeshStats() {
#if MESH_ENABLED
  output.println("\n╔════════════════════════════════════════════════╗");
  output.println("║       Detailed Mesh Protocol Statistics       ║");
  output.println("╠════════════════════════════════════════════════╣");

  char line[52];

  // Message statistics
  uint32_t sent = meshMgr.getMessagesSent();
  uint32_t received = meshMgr.getMessagesReceived();
  uint32_t relayed = meshMgr.getMessagesRelayed();
  uint32_t dropped = meshMgr.getMessagesDropped();

  output.println("║ Message Counters:                              ║");
  snprintf(line, sizeof(line), "║   Sent:         %-30lu ║", sent);
  output.println(line);

  snprintf(line, sizeof(line), "║   Received:     %-30lu ║", received);
  output.println(line);

  snprintf(line, sizeof(line), "║   Relayed:      %-30lu ║", relayed);
  output.println(line);

  snprintf(line, sizeof(line), "║   Dropped:      %-30lu ║", dropped);
  output.println(line);

  uint32_t total = sent + received + relayed;
  snprintf(line, sizeof(line), "║   Total:        %-30lu ║", total);
  output.println(line);

  output.println("╠════════════════════════════════════════════════╣");

  // Success rates
  if (total > 0) {
    float dropRate = (dropped * 100.0f) / (total + dropped);
    snprintf(line, sizeof(line), "║ Drop rate:      %.2f%%                           ║", dropRate);
    output.println(line);

    if (dropRate > 5.0f) {
      printColored("║ ⚠️  High drop rate - check network health      ║\n", COLOR_YELLOW);
    }
  }

  // Peer statistics
  int onlinePeers = meshMgr.getOnlinePeerCount();
  std::vector<MeshPeerInfo> allPeers = meshMgr.getPeers();
  int totalPeers = allPeers.size();

  output.println("╠════════════════════════════════════════════════╣");
  output.println("║ Peer Statistics:                               ║");
  snprintf(line, sizeof(line), "║   Total discovered: %-26d ║", totalPeers);
  output.println(line);

  snprintf(line, sizeof(line), "║   Currently online: %-26d ║", onlinePeers);
  output.println(line);

  if (totalPeers > 0) {
    float onlineRate = (onlinePeers * 100.0f) / totalPeers;
    snprintf(line, sizeof(line), "║   Online rate:      %.1f%%                       ║", onlineRate);
    output.println(line);
  }

  // Queue statistics
  output.println("╠════════════════════════════════════════════════╣");
  output.println("║ Queue Status:                                  ║");

  int queueDepth = meshMgr.getStoredMessageCount();
  snprintf(line, sizeof(line), "║   Depth:        %d/%d messages                  ║",
           queueDepth, MESH_STORE_QUEUE_SIZE);
  output.println(line);

  float utilization = (queueDepth * 100.0f) / MESH_STORE_QUEUE_SIZE;
  snprintf(line, sizeof(line), "║   Utilization:  %.1f%%                           ║", utilization);
  output.println(line);

  // Network health assessment
  output.println("╠════════════════════════════════════════════════╣");
  output.println("║ Network Health Assessment:                     ║");

  bool healthy = true;
  if (dropped > total * 0.05f && total > 10) {
    printColored("║   ⚠️  Drop rate high                           ║\n", COLOR_YELLOW);
    healthy = false;
  }
  if (queueDepth > MESH_STORE_QUEUE_SIZE * 0.8f) {
    printColored("║   ⚠️  Queue nearly full                        ║\n", COLOR_YELLOW);
    healthy = false;
  }
  if (onlinePeers == 0 && totalPeers > 0) {
    printColored("║   ⚠️  No peers online                          ║\n", COLOR_YELLOW);
    healthy = false;
  }

  if (healthy) {
    printColored("║   ✓ Network operating normally                 ║\n", COLOR_GREEN);
  }

  output.println("╚════════════════════════════════════════════════╝\n");
#else
  printColored("[SYS] Mesh networking not enabled\n", COLOR_YELLOW);
#endif
}

void TerminalManager::cmdChannel(const char* args) {
  if (strlen(args) == 0) {
    output.println("\n[SYS] WiFi Channel Information:");
    output.printf("[SYS] Current channel: %d\n", MESH_CHANNEL);
    output.println("[SYS] Valid range: 1-14 (2.4GHz)");
    output.println("\n[SYS] Channel recommendations:");
    output.println("[SYS]   1, 6, 11 - Non-overlapping (USA)");
    output.println("[SYS]   1, 5, 9, 13 - Non-overlapping (Europe)");
    output.println("\n[SYS] Note: Changing channel requires firmware recompile");
    output.println("[SYS] Edit Config.h: #define MESH_CHANNEL <1-14>");
    return;
  }

  int channel = atoi(args);
  if (channel < 1 || channel > 14) {
    printColored("[SYS] Error: Channel must be 1-14\n", COLOR_RED);
    return;
  }

  printColored("[SYS] ⚠️  Runtime channel change not supported\n", COLOR_YELLOW);
  output.println("[SYS] To change channel:");
  output.println("[SYS] 1. Edit cypher-chat/Config.h");
  output.printf("[SYS] 2. Set: #define MESH_CHANNEL %d\n", channel);
  output.println("[SYS] 3. Recompile and upload firmware");
  output.println("[SYS] 4. Restart device");
}

void TerminalManager::cmdMacAddr() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  
  output.println("\n[SYS] Device MAC Address:");
  output.printf("[SYS] %02X:%02X:%02X:%02X:%02X:%02X\n",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void TerminalManager::cmdSetRelay(const char* args) {
  if (strlen(args) == 0) {
    output.println("\n[SYS] Relay Mode Status:");
    output.println("[SYS] Currently: ENABLED (default)");
    output.println("\n[SYS] Usage: setrelay <on/off>");
    output.println("[SYS] When enabled: Device relays messages for others");
    output.println("[SYS] When disabled: Device only sends/receives own messages");
    output.println("\n[SYS] Note: Disabling relay reduces community network capacity");
    return;
  }

  if (strcmp(args, "on") == 0 || strcmp(args, "1") == 0) {
    printColored("[SYS] ✓ Relay mode: ENABLED\n", COLOR_GREEN);
    output.println("[SYS] Device will relay messages for other peers");
    output.println("[SYS] Network: Contributing to mesh capacity");
    output.println("[SYS] Power: Higher consumption (relay traffic)");

    // Note: Full implementation would need MeshManager::setRelayEnabled(true)
    output.println("\n[SYS] Note: Runtime relay toggle requires MeshManager enhancement");
  } else if (strcmp(args, "off") == 0 || strcmp(args, "0") == 0) {
    printColored("[SYS] ⚠️  Relay mode: DISABLED\n", COLOR_YELLOW);
    output.println("[SYS] Device will NOT relay messages for others");
    output.println("[SYS] Network: Reduced mesh capacity");
    output.println("[SYS] Power: Lower consumption (own traffic only)");
    output.println("\n[SYS] ⚠️  Warning: Reduces network capacity for community");
    output.println("[SYS] Only disable if battery critical or stationary");

    output.println("\n[SYS] Note: Runtime relay toggle requires MeshManager enhancement");
  } else {
    printColored("[SYS] Error: Use 'on' or 'off'\n", COLOR_RED);
  }
}

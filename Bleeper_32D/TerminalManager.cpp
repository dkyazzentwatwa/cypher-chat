#include "TerminalManager.h"
#include "DisplayManager.h"
#include "MeshManager.h"
#include "GPSManager.h"
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

void TerminalManager::begin() {
  if (!TERMINAL_ENABLED) {
    enabled = false;
    return;
  }

  enabled = true;
  inputPos = 0;
  menuState = MENU_NONE;
  mode = TERMINAL_DEFAULT_MODE;
  ansiEnabled = true;  // Auto-detect later
  lastUpdate = 0;

  loadConfig();  // Load saved preferences

  delay(100);
  printBanner();

  Serial.println();
  Serial.println("Press 'M' for menu or any other key for command mode...");

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
        Serial.println("Command mode. Type 'help' for commands or 'menu' to switch.");
        printPrompt();
      }
      return;
    }
  }

  // Default to command mode
  menuState = MENU_NONE;
  Serial.println("\nCommand mode. Type 'help' for commands or 'menu' to switch.");
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
    Serial.println("Switched to command mode.");
    printPrompt();
  }
}

void TerminalManager::poll() {
  if (!enabled) return;

  while (Serial.available()) {
    char c = Serial.read();

    if (menuState != MENU_NONE) {
      // Menu mode - single character input
      if (c >= '0' && c <= '9') {
        handleMenuInput(c);
      }
    } else {
      // Command mode - line-based input
      if (c == '\n' || c == '\r') {
        if (inputPos > 0) {
          inputBuffer[inputPos] = '\0';
          Serial.println();  // New line after command
          processCommand(inputBuffer);
          inputPos = 0;
          printPrompt();
        }
      } else if (c == 0x08 || c == 0x7F) {  // Backspace
        if (inputPos > 0) {
          inputPos--;
          Serial.print("\b \b");  // Erase character
        }
      } else if (inputPos < sizeof(inputBuffer) - 1 && c >= 0x20) {
        inputBuffer[inputPos++] = c;
        Serial.write(c);  // Echo
      }
    }
  }
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

  if (strcmp(verb, "send") == 0) {
    cmdSend(args);
  } else if (strcmp(verb, "emergency") == 0) {
    cmdEmergency();
  } else if (strcmp(verb, "cancel") == 0) {
    cmdCancel();
  } else if (strcmp(verb, "status") == 0) {
    cmdStatus();
  } else if (strcmp(verb, "messages") == 0) {
    cmdMessages();
  } else if (strcmp(verb, "config") == 0) {
    cmdConfig();
  } else if (strcmp(verb, "name") == 0) {
    cmdName(args);
  } else if (strcmp(verb, "passkey") == 0) {
    cmdPasskey(args);
  } else if (strcmp(verb, "mode") == 0) {
    cmdMode(args);
  } else if (strcmp(verb, "restart") == 0) {
    cmdRestart();
  } else if (strcmp(verb, "uptime") == 0) {
    cmdUptime();
  } else if (strcmp(verb, "memory") == 0) {
    cmdMemory();
  } else if (strcmp(verb, "help") == 0) {
    cmdHelp();
  } else if (strcmp(verb, "clear") == 0) {
    cmdClear();
  } else if (strcmp(verb, "menu") == 0) {
    cmdMenu();
  } else if (strcmp(verb, "mesh") == 0) {
    cmdMesh();
  } else if (strcmp(verb, "peers") == 0) {
    cmdPeers();
  } else if (strcmp(verb, "meshsend") == 0) {
    cmdMeshSend(args);
  } else if (strcmp(verb, "broadcast") == 0) {
    cmdMeshBroadcast(args);
  } else if (strcmp(verb, "gps") == 0) {
    cmdGPS();
  } else {
    printColored("Unknown command. Type 'help' for command list.", COLOR_RED);
    Serial.println();
  }
}

void TerminalManager::handleMenuInput(char input) {
  switch (menuState) {
    case MENU_MAIN:
      switch (input) {
        case '1': menuState = MENU_SEND; displaySendMenu(); break;
        case '2': cmdStatus(); displayMainMenu(); break;
        case '3': cmdMessages(); displayMainMenu(); break;
        case '4': menuState = MENU_CONFIG; displayConfigMenu(); break;
        case '5': cmdEmergency(); delay(1000); displayMainMenu(); break;
        case '6': cmdMenu(); break;  // Switch to command mode
        case '0': menuState = MENU_NONE; Serial.println("Exiting menu mode."); printPrompt(); break;
        default: Serial.println("Invalid choice."); delay(500); displayMainMenu(); break;
      }
      break;

    case MENU_SEND:
      if (input >= '1' && input <= '4') {
        cmdSend(String(input).c_str());
        Serial.println("Message sent.");
        delay(1000);
        menuState = MENU_MAIN;
        displayMainMenu();
      } else if (input == '0') {
        menuState = MENU_MAIN;
        displayMainMenu();
      }
      break;

    case MENU_CONFIG:
      if (input == '0') {
        menuState = MENU_MAIN;
        displayMainMenu();
      } else {
        Serial.println("Configuration menu not yet implemented.");
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
    Serial.println();
    return;
  }

  simulateButtonPress(buttonNum - 1);

  Serial.print("Sending: ");
  Serial.println(BUTTON_LABELS[buttonNum - 1]);
}

void TerminalManager::cmdEmergency() {
  if (!emergencyActive) {
    printColored("[EMERGENCY] Triggering emergency broadcast...", COLOR_RED);
    Serial.println();
    broadcastEmergency();
  } else {
    printColored("[EMERGENCY] Emergency already active", COLOR_YELLOW);
    Serial.println();
  }
}

void TerminalManager::cmdCancel() {
  if (emergencyActive) {
    printColored("[EMERGENCY] Canceling emergency broadcast...", COLOR_YELLOW);
    Serial.println();
    cancelEmergency();
  } else {
    Serial.println("No active emergency to cancel.");
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
    Serial.println("Error: Unit name cannot be empty");
    return;
  }

  unitName = String(newName);
  Serial.print("Unit name changed to: ");
  Serial.println(unitName);
  Serial.println("Note: Restart required for BLE advertising to update.");
}

void TerminalManager::cmdPasskey(const char* newKey) {
  if (strlen(newKey) != PASSKEY_DIGITS) {
    Serial.print("Error: Passkey must be exactly ");
    Serial.print(PASSKEY_DIGITS);
    Serial.println(" digits");
    return;
  }

  uint32_t passkey = atoi(newKey);
  if (passkey < MIN_PASSKEY || passkey > MAX_PASSKEY) {
    Serial.println("Error: Passkey out of range (100000-999999)");
    return;
  }

  currentPasskey = passkey;
  Serial.print("Passkey changed to: ");
  Serial.println(currentPasskey);
  Serial.println("Note: Restart required to apply new passkey.");
}

void TerminalManager::cmdMode(const char* modeStr) {
  if (strcmp(modeStr, "quiet") == 0) {
    setMode(TERM_QUIET);
    Serial.println("Terminal mode: QUIET (errors only)");
  } else if (strcmp(modeStr, "normal") == 0) {
    setMode(TERM_NORMAL);
    Serial.println("Terminal mode: NORMAL (status + messages)");
  } else if (strcmp(modeStr, "verbose") == 0) {
    setMode(TERM_VERBOSE);
    Serial.println("Terminal mode: VERBOSE (full debug)");
  } else if (strcmp(modeStr, "monitor") == 0) {
    setMode(TERM_MONITOR);
    Serial.println("Terminal mode: MONITOR (live dashboard)");
    Serial.println("Press any key to exit monitor mode.");
    lastUpdate = 0;  // Force immediate update
  } else {
    Serial.println("Error: Invalid mode. Use: quiet, normal, verbose, or monitor");
  }
}

void TerminalManager::cmdRestart() {
  Serial.println("Restarting ESP32 in 3 seconds...");
  delay(3000);
  ESP.restart();
}

void TerminalManager::cmdUptime() {
  char uptime[32];
  formatUptime(uptime, sizeof(uptime));
  Serial.print("System uptime: ");
  Serial.println(uptime);
}

void TerminalManager::cmdMemory() {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t totalHeap = ESP.getHeapSize();
  uint32_t usedHeap = totalHeap - freeHeap;

  Serial.print("Memory usage: ");
  Serial.print(usedHeap / 1024);
  Serial.print("KB / ");
  Serial.print(totalHeap / 1024);
  Serial.print("KB (");
  Serial.print((usedHeap * 100) / totalHeap);
  Serial.println("%)");
  Serial.print("Free heap: ");
  Serial.print(freeHeap / 1024);
  Serial.println("KB");
}

void TerminalManager::cmdHelp() {
  Serial.println("\nAvailable Commands:");
  Serial.println("==================\n");

  Serial.println("Message Commands:");
  Serial.println("  send <1-4>       Send button message (1=ACK, 2=ENROUTE, 3=NEED HELP, 4=ALL GOOD)");
  Serial.println("  emergency        Trigger emergency broadcast (BLE + Mesh)");
  Serial.println("  cancel           Cancel emergency\n");

  Serial.println("Mesh Networking (ESP-NOW, ~250m range):");
  Serial.println("  mesh             Show mesh network status and statistics");
  Serial.println("  peers            List discovered mesh peers");
  Serial.println("  meshsend <msg>   Send message via mesh network");
  Serial.println("  broadcast <msg>  Alias for meshsend");
  Serial.println("  gps              Show GPS status and coordinates\n");

  Serial.println("Display Commands:");
  Serial.println("  status           Show current status (BLE + Mesh)");
  Serial.println("  messages         Show message history");
  Serial.println("  config           Show configuration");
  Serial.println("  clear            Clear screen\n");

  Serial.println("Configuration:");
  Serial.println("  name <name>      Change unit name");
  Serial.println("  passkey <6dig>   Change passkey (requires restart)");
  Serial.println("  mode <mode>      Set terminal mode (quiet/normal/verbose/monitor)\n");

  Serial.println("System:");
  Serial.println("  restart          Restart device");
  Serial.println("  uptime           Show system uptime");
  Serial.println("  memory           Show memory usage");
  Serial.println("  help             Show this help");
  Serial.println("  menu             Switch to interactive menu mode\n");
}

void TerminalManager::cmdClear() {
  if (ansiEnabled) {
    Serial.print("\033[2J\033[H");  // Clear screen and move to home
  } else {
    for (int i = 0; i < 50; i++) Serial.println();  // Fallback
  }
}

void TerminalManager::cmdMenu() {
  toggleMode();
}

// Mesh networking commands
void TerminalManager::cmdMesh() {
#if MESH_ENABLED
  Serial.println("\n╔════════════════════════════════════════════════╗");
  Serial.println("║           Mesh Network Status                  ║");
  Serial.println("╠════════════════════════════════════════════════╣");

  if (!meshMgr.isRunning()) {
    Serial.println("║ Mesh networking: DISABLED                      ║");
    Serial.println("╚════════════════════════════════════════════════╝\n");
    return;
  }

  // Get MAC address
  uint8_t mac[6];
  meshMgr.getMyMac(mac);
  char macStr[20];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  char line[52];
  snprintf(line, sizeof(line), "║ My MAC: %-17s              ║", macStr);
  Serial.println(line);

  snprintf(line, sizeof(line), "║ Status: %-10s    Peers Online: %-3d    ║",
           meshMgr.isRunning() ? "RUNNING" : "STOPPED",
           meshMgr.getOnlinePeerCount());
  Serial.println(line);

  Serial.println("╠════════════════════════════════════════════════╣");

  snprintf(line, sizeof(line), "║ Messages Sent:     %-8lu                  ║",
           meshMgr.getMessagesSent());
  Serial.println(line);

  snprintf(line, sizeof(line), "║ Messages Received: %-8lu                  ║",
           meshMgr.getMessagesReceived());
  Serial.println(line);

  snprintf(line, sizeof(line), "║ Messages Relayed:  %-8lu                  ║",
           meshMgr.getMessagesRelayed());
  Serial.println(line);

  snprintf(line, sizeof(line), "║ Messages Dropped:  %-8lu                  ║",
           meshMgr.getMessagesDropped());
  Serial.println(line);

  snprintf(line, sizeof(line), "║ Store Queue:       %-3d / %-3d messages        ║",
           meshMgr.getStoredMessageCount(), MESH_STORE_QUEUE_SIZE);
  Serial.println(line);

  Serial.println("╚════════════════════════════════════════════════╝\n");
#else
  Serial.println("Mesh networking is disabled (MESH_ENABLED=false)");
#endif
}

void TerminalManager::cmdPeers() {
#if MESH_ENABLED
  if (!meshMgr.isRunning()) {
    Serial.println("Mesh networking not running.");
    return;
  }

  std::vector<MeshPeer> peers = meshMgr.getPeers();

  Serial.println("\n╔════════════════════════════════════════════════════════════╗");
  Serial.println("║                    Mesh Peers                               ║");
  Serial.println("╠════════════════════════════════════════════════════════════╣");

  if (peers.empty()) {
    Serial.println("║ No peers discovered yet.                                    ║");
    Serial.println("║ Peers are discovered via heartbeat (every 15 seconds).      ║");
  } else {
    Serial.println("║ MAC Address       | Name       | RSSI | Hops | Status      ║");
    Serial.println("╠════════════════════════════════════════════════════════════╣");

    for (const auto& peer : peers) {
      char macStr[18];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               peer.mac[0], peer.mac[1], peer.mac[2],
               peer.mac[3], peer.mac[4], peer.mac[5]);

      char line[72];
      snprintf(line, sizeof(line), "║ %-17s | %-10s | %-4d | %-4d | %-10s ║",
               macStr,
               strlen(peer.unitName) > 0 ? peer.unitName : "-",
               peer.rssi,
               peer.hopDistance,
               peer.isOnline ? "ONLINE" : "OFFLINE");
      Serial.println(line);
    }
  }

  Serial.println("╚════════════════════════════════════════════════════════════╝\n");

  Serial.print("Total peers: ");
  Serial.print(peers.size());
  Serial.print(" (");
  Serial.print(meshMgr.getOnlinePeerCount());
  Serial.println(" online)\n");
#else
  Serial.println("Mesh networking is disabled (MESH_ENABLED=false)");
#endif
}

void TerminalManager::cmdMeshSend(const char* args) {
#if MESH_ENABLED
  if (!meshMgr.isRunning()) {
    Serial.println("Mesh networking not running.");
    return;
  }

  if (strlen(args) == 0) {
    Serial.println("Usage: meshsend <message>");
    return;
  }

  // Build message with unit name prefix
  char msg[128];
  snprintf(msg, sizeof(msg), "%s:%s", unitName.c_str(), args);

  uint32_t msgId = meshMgr.broadcast((uint8_t*)msg, strlen(msg), MESH_MSG_DATA, MESH_DEFAULT_TTL);

  if (msgId > 0) {
    printColored("[MESH TX] ", COLOR_CYAN);
    Serial.printf("Broadcast sent (id: %08X, TTL: %d)\n", msgId, MESH_DEFAULT_TTL);
    Serial.print("  Message: ");
    Serial.println(msg);
  } else {
    printColored("[MESH] ", COLOR_RED);
    Serial.println("Failed to send broadcast");
  }
#else
  Serial.println("Mesh networking is disabled (MESH_ENABLED=false)");
#endif
}

void TerminalManager::cmdMeshBroadcast(const char* args) {
  // Alias for meshsend for convenience
  cmdMeshSend(args);
}

// GPS commands
void TerminalManager::cmdGPS() {
#if GPS_ENABLED
  Serial.println("\n╔════════════════════════════════════════════════╗");
  Serial.println("║              GPS Status                        ║");
  Serial.println("╠════════════════════════════════════════════════╣");

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
  Serial.println(line);

  if (gpsMgr.hasFix()) {
    GPSCoordinates coords = gpsMgr.getCoordinates();

    snprintf(line, sizeof(line), "║ Latitude:  %11.6f                      ║", coords.latitude);
    Serial.println(line);

    snprintf(line, sizeof(line), "║ Longitude: %11.6f                      ║", coords.longitude);
    Serial.println(line);

    snprintf(line, sizeof(line), "║ Altitude:  %7.1f m                        ║", coords.altitude);
    Serial.println(line);

    snprintf(line, sizeof(line), "║ HDOP: %.1f   Speed: %.1f km/h               ║",
             gpsMgr.getHDOP(), gpsMgr.getSpeed());
    Serial.println(line);

    unsigned long age = gpsMgr.getFixAge();
    snprintf(line, sizeof(line), "║ Fix Age: %lu ms                            ║", age);
    Serial.println(line);

    Serial.println("╠════════════════════════════════════════════════╣");

    char mapsUrl[80];
    gpsMgr.formatMapsURL(mapsUrl, sizeof(mapsUrl));
    Serial.print("║ ");
    Serial.println(mapsUrl);
  } else {
    Serial.println("║ Waiting for satellite fix...                   ║");
    Serial.println("║ Ensure GPS module has clear sky view.          ║");
  }

  Serial.println("╚════════════════════════════════════════════════╝\n");
#else
  Serial.println("GPS is disabled (GPS_ENABLED=false in Config.h)");
  Serial.println("To enable:");
  Serial.println("  1. Set GPS_ENABLED to true in Config.h");
  Serial.println("  2. Connect GPS TX to GPIO %d (GPS_RX_PIN)", GPS_RX_PIN);
  Serial.println("  3. Recompile and upload firmware");
#endif
}

// Display functions
void TerminalManager::displayMainMenu() {
  if (ansiEnabled) {
    Serial.print("\033[2J\033[H");  // Clear screen
  }

  Serial.println("╔════════════════════════════════════════════════╗");
  Serial.println("║          Bleeper_32D Control Menu              ║");
  Serial.println("╠════════════════════════════════════════════════╣");
  Serial.println("║ [1] Send Message                               ║");
  Serial.println("║ [2] View Status                                ║");
  Serial.println("║ [3] View Messages                              ║");
  Serial.println("║ [4] Configuration                              ║");
  Serial.println("║ [5] Emergency Broadcast                        ║");
  Serial.println("║ [6] Switch to Command Mode                     ║");
  Serial.println("║ [0] Exit Menu                                  ║");
  Serial.println("╚════════════════════════════════════════════════╝");
  Serial.print("Enter choice: ");
}

void TerminalManager::displaySendMenu() {
  if (ansiEnabled) {
    Serial.print("\033[2J\033[H");
  }

  Serial.println("╔════════════════════════════════════════════════╗");
  Serial.println("║              Send Message                      ║");
  Serial.println("╠════════════════════════════════════════════════╣");
  Serial.println("║ [1] ACK                                        ║");
  Serial.println("║ [2] ENROUTE                                    ║");
  Serial.println("║ [3] NEED HELP                                  ║");
  Serial.println("║ [4] ALL GOOD                                   ║");
  Serial.println("║ [0] Back to Main Menu                          ║");
  Serial.println("╚════════════════════════════════════════════════╝");
  Serial.print("Enter choice: ");
}

void TerminalManager::displayConfigMenu() {
  Serial.println("Configuration menu not yet implemented.");
  Serial.println("Press 0 to return to main menu.");
}

void TerminalManager::displayMonitorDashboard() {
  if (ansiEnabled) {
    Serial.print("\033[2J\033[H");  // Clear and home
  }

  Serial.println("╔════════════════════════════════════════════════╗");
  Serial.println("║ Bleeper_32D - LIVE MONITOR                    ║");
  Serial.println("╠════════════════════════════════════════════════╣");

  // Status line
  char line[64];
  extern StateManager connectionState;
  ConnectionState state = connectionState.getState();
  const char* stateStr = getStateString(state);

  char uptime[16];
  formatUptime(uptime, sizeof(uptime));

  snprintf(line, sizeof(line), "║ State: %-12s      Uptime: %8s   ║",
           stateStr, uptime);
  Serial.println(line);

  // Retry and memory
  int retries = connectionState.getRetryCount();
  uint32_t freeHeap = ESP.getFreeHeap();
  snprintf(line, sizeof(line), "║ Retry: %-3d                Memory: %3luKB free ║",
           retries, freeHeap / 1024);
  Serial.println(line);

  // Role and unit
  const char* role = isServer ? "SERVER" : "CLIENT";
  snprintf(line, sizeof(line), "║ Role: %-10s          Unit: %-11s ║",
           role, unitName.c_str());
  Serial.println(line);

  Serial.println("╠════════════════════════════════════════════════╣");
  Serial.println("║ Recent Activity:                               ║");

  // This would show recent messages/events
  // For now, placeholder
  Serial.println("║ (Activity log not yet implemented)            ║");

  Serial.println("╚════════════════════════════════════════════════╝");
  Serial.println("Press any key to exit monitor mode...");
}

void TerminalManager::printColored(const char* text, const char* color) {
  if (ansiEnabled && color) {
    Serial.print(color);
    Serial.print(text);
    Serial.print(COLOR_RESET);
  } else {
    Serial.print(text);
  }
}

void TerminalManager::printBanner() {
  if (!TERMINAL_BANNER_ENABLED) return;

  Serial.println();
  Serial.println("╔════════════════════════════════════════════════╗");
  Serial.println("║     Bleeper_32D Emergency Communication        ║");
  Serial.println("╚════════════════════════════════════════════════╝");
}

void TerminalManager::printPrompt() {
  Serial.print("> ");
}

void TerminalManager::printStatus() {
  Serial.println("\n╔════════════════════════════════════════════════╗");
  Serial.println("║ Bleeper_32D - Emergency Communication Device   ║");
  Serial.println("╠════════════════════════════════════════════════╣");

  // Role and unit
  char line[64];
  const char* role = isServer ? "SERVER" : "CLIENT";
  snprintf(line, sizeof(line), "║ Role: %-10s          Unit: %-11s ║",
           role, unitName.c_str());
  Serial.println(line);

  // State and retry
  extern StateManager connectionState;
  ConnectionState state = connectionState.getState();
  const char* stateStr = getStateString(state);
  int retries = connectionState.getRetryCount();

  snprintf(line, sizeof(line), "║ BLE State: %-10s          Retry: %-3d    ║",
           stateStr, retries);
  Serial.println(line);

  // Uptime
  char uptime[16];
  formatUptime(uptime, sizeof(uptime));
  snprintf(line, sizeof(line), "║ Uptime: %-37s ║", uptime);
  Serial.println(line);

  Serial.println("╠════════════════════════════════════════════════╣");

#if MESH_ENABLED
  // Mesh status
  if (meshMgr.isRunning()) {
    snprintf(line, sizeof(line), "║ Mesh: ACTIVE        Peers Online: %-3d        ║",
             meshMgr.getOnlinePeerCount());
    Serial.println(line);

    snprintf(line, sizeof(line), "║ Mesh TX: %-6lu   RX: %-6lu   Relay: %-6lu ║",
             meshMgr.getMessagesSent(),
             meshMgr.getMessagesReceived(),
             meshMgr.getMessagesRelayed());
    Serial.println(line);
  } else {
    Serial.println("║ Mesh: DISABLED                                 ║");
  }
#else
  Serial.println("║ Mesh: NOT COMPILED                             ║");
#endif

  Serial.println("╚════════════════════════════════════════════════╝\n");
}

void TerminalManager::printMessageHistory() {
  Serial.println("\nMessage History:");
  Serial.println("================");
  // This would show message history from DisplayManager
  // For now, placeholder
  Serial.println("(Message history integration pending)\n");
}

void TerminalManager::printConfiguration() {
  Serial.println("\nDevice Configuration:");
  Serial.println("====================");
  Serial.print("Unit Name: ");
  Serial.println(unitName);
  Serial.print("Role: ");
  Serial.println(isServer ? "SERVER" : "CLIENT");
  Serial.print("Passkey: ");
  Serial.print(currentPasskey);
  Serial.println(" (configured)");
  Serial.print("Terminal Mode: ");
  switch (mode) {
    case TERM_QUIET: Serial.println("QUIET"); break;
    case TERM_NORMAL: Serial.println("NORMAL"); break;
    case TERM_VERBOSE: Serial.println("VERBOSE"); break;
    case TERM_MONITOR: Serial.println("MONITOR"); break;
  }
  Serial.println();
}

// Event logging
void TerminalManager::logEvent(const char* level, const char* message) {
  if (!enabled) return;
  if (mode == TERM_QUIET && strcmp(level, "ERROR") != 0) return;
  if (mode == TERM_MONITOR) return;  // Don't log in monitor mode

  Serial.print("[");

  if (strcmp(level, "ERROR") == 0) {
    printColored(level, COLOR_RED);
  } else if (strcmp(level, "INFO") == 0) {
    printColored(level, COLOR_BLUE);
  } else if (strcmp(level, "SCAN") == 0 || strcmp(level, "CONN") == 0) {
    printColored(level, COLOR_CYAN);
  } else {
    Serial.print(level);
  }

  Serial.print("] ");
  Serial.println(message);
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

  Serial.println(formatted);
}

void TerminalManager::logHMAC(bool verified, const char* message) {
  if (!enabled || mode < TERM_VERBOSE) return;
  if (mode == TERM_MONITOR) return;

  if (verified) {
    printColored("[HMAC] ✓ Signature verified: ", COLOR_GREEN);
    Serial.println(message);
  } else {
    printColored("[HMAC] ✗ Verification FAILED: ", COLOR_RED);
    Serial.println(message);
  }
}

void TerminalManager::logEmergency(bool active) {
  if (!enabled) return;

  if (active) {
    printColored("[EMERGENCY] Emergency broadcast activated!", COLOR_RED);
  } else {
    printColored("[EMERGENCY] Emergency canceled", COLOR_YELLOW);
  }
  Serial.println();
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

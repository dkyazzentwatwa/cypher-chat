# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Compilation

**Note:** This project has compilation issues due to missing Server.cpp/Client.cpp files referenced in Bleeper_32D.ino. The old BLE server/client architecture has been removed but references remain.

Compile the firmware:
```bash
arduino-cli compile --fqbn esp32:esp32:esp32 Bleeper_32D.ino
```

Upload to device:
```bash
arduino-cli upload -p /dev/cu.SLAB_USBtoUART --fqbn esp32:esp32:esp32 Bleeper_32D.ino
```

Common ports: `/dev/cu.SLAB_USBtoUART`, `/dev/cu.usbserial-0001`

## Project Architecture

**Starbeam** is an ESP32-based mesh networking system for emergency communications with wireless terminal access. It uses **ESP-NOW** for multi-hop message relay (250m per hop, up to 5 hops) and provides terminal access via USB Serial or Bluetooth Serial from mobile apps.

### System Overview

```
┌─────────────────────────────────────────────────────────────┐
│  Input Layer                                                │
│  • 3 Physical Buttons (GPIO 39, 34, 36)                    │
│  • USB Serial Terminal (115200 baud)                        │
│  • Bluetooth Serial Terminal (phone apps)                   │
└──────────────────┬──────────────────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────────────────┐
│  Processing Layer                                           │
│  • ButtonManager: Debounce, long-press detection           │
│  • TerminalManager: CLI with 25+ commands, history, menus  │
│  • MessageAuth: HMAC-SHA256 authentication                  │
└──────────────────┬──────────────────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────────────────┐
│  Mesh Networking Layer (Primary Transport)                 │
│  • MeshManager: ESP-NOW multi-hop relay                     │
│  • Store-and-forward for offline nodes (32 msg queue)      │
│  • Automatic peer discovery via 15s heartbeats              │
│  • Message deduplication (64-entry circular cache)          │
│  • TTL 3-5 hops, ~1.25km theoretical range                 │
└──────────────────┬──────────────────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────────────────┐
│  Output Layer                                               │
│  • OutputManager: Broadcast to USB + BT Serial             │
│  • DisplayManager: OLED UI (3 screens)                      │
│  • LEDManager: RGB connection state indicators              │
│  • BuzzerManager: Audio feedback                            │
└─────────────────────────────────────────────────────────────┘
```

### Core Components

**Bleeper_32D.ino** (Main entry point)
- Sets up all managers and mesh networking
- Event handlers: `onMeshMessage()`, `onMeshPeerUpdate()`
- Emergency broadcast logic with 30-second active window
- Global state: `unitName`, `currentPasskey`, `messageHistory[10]`

**MeshManager** (Primary communication layer)
- ESP-NOW based peer-to-peer mesh with multi-hop relay
- Automatic peer discovery via periodic heartbeats (15s interval)
- Store-and-forward queue (32 messages) for offline nodes
- Message types: DATA, EMERGENCY, HEARTBEAT, ACK
- Deduplication via circular cache of 64 message IDs
- GPS coordinates included in emergency broadcasts (if GPS enabled)
- Key methods: `sendMessage()`, `sendEmergency()`, `broadcast()`, `relay()`

**TerminalManager** (CLI interface)
- Dual input sources: USB Serial + Bluetooth Serial (source-aware echo)
- 25+ commands organized into 5 categories
- Command history (UP/DOWN arrows), tab completion
- Four terminal modes: QUIET, NORMAL, VERBOSE, MONITOR
- Interactive menus with scrolling
- Commands execute mesh operations, display stats, configure settings

**OutputManager** (Dual serial output)
- Broadcasts all output to both USB Serial and Bluetooth Serial simultaneously
- Strips ANSI escape codes for Bluetooth (mobile app compatibility)
- **Critical:** BLE writes are batched (not character-by-character) to prevent garbled output
- Uses 256-byte buffer for printf formatting
- BLE UART chunks sent as 64-byte packets with 10ms delays

**BLEUARTManager** (Wireless terminal access)
- Nordic UART Service (NUS) for iPhone/Android terminal apps
- Service UUID: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- TX Characteristic: `6E400003...` (notify to client)
- RX Characteristic: `6E400002...` (write from client)
- 256-byte circular RX buffer
- Compatible with: nRF Connect, Serial Bluetooth Terminal, iESP32 Terminal

**Config_Starbeam.h** (Hardware and feature flags)
- Hardware: ESP32 DevKit C V4
- Pin definitions for buttons, LEDs, I2C, UART
- Feature flags: `MESH_ENABLED=true`, `BLE_UART_ENABLED=true`, `BLE_ENABLED=false`
- Runtime config via `extern`: `unitName`, `currentPasskey`, `isServer`

### Message Flow

**Emergency Broadcast:**
1. Button 3 long-press (>2s) triggers `broadcastEmergency()`
2. Sets 30-second emergency window with red LED pattern
3. Floods mesh via `meshMgr.sendEmergency()` with TTL=5 (max hops)
4. Each peer relays message if TTL > 0, decrements TTL
5. Message includes GPS coordinates if `GPS_ENABLED=true`
6. Recipients display emergency alert on OLED + buzzer alert

**Mesh Message Received:**
1. `MeshPeer::onReceive()` → `MeshManager::handlePeerReceive()`
2. `processPacket()` checks deduplication cache (64 entries)
3. If TTL > 0 and not seen: relay to all other peers
4. Invoke `onMeshMessage()` callback in Bleeper_32D.ino
5. Parse message type, format for display
6. Trigger LED/buzzer feedback, add to history
7. Broadcast to USB + BT Serial via `OutputManager`

**Terminal Command:**
1. User types command on USB or phone (Bluetooth Serial)
2. `TerminalManager::processInput(char, source)` with source-aware echo
3. `executeCommand()` parses and routes to handlers
4. Commands trigger mesh sends, display updates, config changes
5. Output via `output.println()` to both USB and BT

### Key Configuration

From `Config_Starbeam.h`:

```cpp
// ENABLED FEATURES
#define MESH_ENABLED true
#define BLE_UART_ENABLED true        // Nordic UART for wireless terminal
#define TERMINAL_ENABLED true
#define BT_SERIAL_ENABLED true       // Classic Bluetooth (currently unused)

// DISABLED FEATURES
#define BLE_ENABLED false            // Old NimBLE server/client (obsolete)
#define GPS_ENABLED false            // Can enable for GPS in emergencies

// MESH PARAMETERS
#define MESH_CHANNEL 1               // ESP-NOW WiFi channel
#define MESH_MAX_PEERS 20            // Max simultaneous mesh neighbors
#define MESH_DEFAULT_TTL 3           // Default hop limit
#define MESH_MAX_TTL 5               // Emergency broadcast TTL
#define MESH_HEARTBEAT_MS 15000      // Peer discovery interval
#define MESH_PEER_TIMEOUT_MS 60000   // Peer offline after 1 min silence
#define MESH_STORE_QUEUE_SIZE 32     // Store-and-forward buffer
```

### Hardware Configuration

- **Display:** SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22, Addr=0x3C)
- **Buttons:** 3 buttons on GPIO 39, 34, 36 (input-only, require external 10kΩ pull-ups)
  - Button 3 short: Cycle OLED screens
  - Button 3 long: Emergency broadcast
  - Button 0 long: Cancel emergency
- **LEDs:** RGB indicators on GPIO 0, 3, 1
  - Blue: Idle, Yellow: Connecting, Green: Connected, Red: Emergency
- **Serial:** USB (UART_TXD=1, UART_RXD=3) + Bluetooth Serial

### Terminal Commands

**Message Commands:**
- `send <1-4> [s]` - Simulate button press, optional `[s]` suffix for emergency SOS
- `emergency [sos]` - Trigger emergency broadcast
- `cancel` - Cancel active emergency

**Mesh Commands:**
- `mesh [m]` - Show mesh network status and peer list
- `peers [p]` - List discovered mesh peers with RSSI
- `gps` - Show GPS status and coordinates (if GPS enabled)

**Display Commands:**
- `status` - Show current device status and connections
- `mess[msg]` - Show message history with config details
- `cfg` - Show configuration menu

**Configuration:**
- `name <name>` - Change device name
- `passkey <6dig> [pk]` - Change authentication passkey
- `mode <quiet/normal/verbose/monitor>` - Set terminal mode
- `ansi <on/off>` - Toggle ANSI color codes

**System:**
- `help [h]` - Show command list
- `restart` - Reboot ESP32
- `uptime` - Show system uptime
- `memory [mem]` - Display heap/PSRAM usage
- `version [v]` - Show firmware version
- `clear` - Clear screen

## Known Issues

1. **Compilation broken:** `Bleeper_32D.ino` includes `Server.h` and `Client.h` (lines 12-13) which don't exist. Remove these includes and the calls to `setupServer()`, `setupClient()`, `loopServer()`, `clientLoop()` (lines 470-474, 517-519).

2. **BLE_ENABLED=false:** Old NimBLE server/client architecture is disabled. References to `pServer`, `pTxCharacteristic`, `pRemoteRxCharacteristic` are non-functional.

3. **Missing pin definitions:** `BUZZER_PIN`, `LED_PIN_R/G/B` not defined in Config_Starbeam.h but referenced in code. Currently handled with `#if BUZZER_PIN != -1` guards.

## Architecture Notes

**Output Batching (OutputManager):**
- Prior issue: Character-by-character BLE writes caused garbled Bluetooth terminal output
- Solution: Buffer characters, strip ANSI codes, send as single bulk write
- BLE chunking: 64-byte packets (increased from 20 bytes) with 10ms inter-chunk delay
- This ensures USB and Bluetooth Serial show identical formatting

**Thread Safety:**
- `StateManager` uses FreeRTOS semaphores for connection state access
- ESP-NOW callbacks run in WiFi task context, must be fast
- Mesh message processing deferred to main loop via queue

**Message Security:**
- `MessageAuth` provides HMAC-SHA256 authentication with shared passkey
- 8-byte HMAC appended to messages (16 hex chars)
- Passkey configurable at runtime via terminal or boot sequence

**Power Management:**
- No explicit sleep modes implemented
- Continuous mesh heartbeat every 15 seconds prevents deep sleep
- WiFi (ESP-NOW) always active for mesh networking

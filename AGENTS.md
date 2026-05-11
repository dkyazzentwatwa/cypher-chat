# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Project Variants

This repository contains **two variants** of the Cypher-Chat firmware, sharing most core components:

1. **cypher-chat-32D/** - Full-featured version with hardware peripherals (OLED, buttons, LEDs, buzzer, optional GPS)
2. **cypher-chat-basic/** - Minimal version for bare ESP32 boards ($3-5), terminal-only operation

Both variants share the same mesh networking, encryption, and terminal systems but have different `Config.h` files that enable/disable hardware features.

## Build and Compilation

**Compile firmware:**
```bash
# Full version (32D) - requires huge_app partition for NimBLE + crypto
cd cypher-chat-32D
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app cypher-chat-32D.ino

# Basic version
cd cypher-chat-basic
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app cypher-chat-basic.ino
```

**Upload to device:**
```bash
arduino-cli upload -p /dev/cu.SLAB_USBtoUART --fqbn esp32:esp32:esp32:PartitionScheme=huge_app <sketch>.ino
```

Common ports: `/dev/cu.SLAB_USBtoUART`, `/dev/cu.usbserial-0001`

**Required libraries:**
- NimBLE-Arduino (for BLE UART)
- Adafruit GFX and Adafruit SSD1306 (32D version only)
- mbedtls (built into ESP32 core)

## Project Architecture

**cypher-chat** is an ESP32-based mesh networking system for emergency communications. It uses **ESP-NOW** for multi-hop message relay (250m per hop, up to 5 hops) with **AES-256-GCM AEAD encryption** and provides terminal access via USB Serial or Bluetooth Serial (BLE UART).

### System Overview

```
┌─────────────────────────────────────────────────────────────┐
│  Input Layer                                                │
│  • Physical Buttons (GPIO 39, 34, 36) - 32D only           │
│  • USB Serial Terminal (115200 baud)                        │
│  • BLE UART Terminal (Nordic UART Service)                  │
└──────────────────┬──────────────────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────────────────┐
│  Processing Layer                                           │
│  • ButtonManager: Debounce, long-press (32D only)          │
│  • TerminalManager: CLI with 85+ commands in 13 categories │
│  • MeshCrypto: AES-256-GCM encryption + HKDF key derivation│
│  • SecurityManager: Peer blocklist, trust list (32D only)  │
│  • PowerManager: TX power control, sleep modes (32D only)  │
└──────────────────┬──────────────────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────────────────┐
│  Mesh Networking Layer (Primary Transport)                 │
│  • MeshManager: ESP-NOW multi-hop relay                     │
│  • AES-256-GCM authenticated encryption (32-byte keys)      │
│  • MAC randomization for anti-tracking                      │
│  • Store-and-forward for offline nodes (32 msg queue)      │
│  • Automatic peer discovery via 15s heartbeats              │
│  • Message deduplication (64-entry circular cache)          │
│  • TTL 3-5 hops, ~1.25km theoretical range                 │
└──────────────────┬──────────────────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────────────────┐
│  Output Layer                                               │
│  • OutputManager: Broadcast to USB + BLE Serial            │
│  • DisplayManager: OLED UI with 3 screens (32D only)       │
│  • LEDManager: RGB state indicators (32D only)             │
│  • BuzzerManager: Audio feedback (32D only)                │
└─────────────────────────────────────────────────────────────┘
```

### Core Components

**cypher-chat-32D.ino / cypher-chat-basic.ino** (Main entry points)
- Initialize all managers and mesh networking
- Event handlers: `onMeshMessage()`, `onMeshPeerUpdate()`
- Emergency broadcast logic with 30-second active window
- Global state: `unitName`, `currentPassphrase`, `messageHistory[10]`

**MeshManager.h** (Primary communication layer)
- ESP-NOW based peer-to-peer mesh with multi-hop relay
- Automatic peer discovery via periodic heartbeats (15s interval)
- Store-and-forward queue (32 messages) for offline nodes
- Message types: DATA, EMERGENCY, HEARTBEAT, ACK
- Deduplication via circular cache of 64 message IDs
- GPS coordinates in emergency broadcasts (if GPS enabled)
- Key methods: `sendMessage()`, `sendEmergency()`, `broadcast()`, `relay()`

**MeshCrypto.h** (Encryption and authentication)
- **AES-256-GCM AEAD encryption** with 96-bit nonces
- **HKDF-SHA256** key derivation from user passphrase
- Wire format: `[version:1][nonce:12][ciphertext:N][auth_tag:16]`
- Protocol version 0x02 (incremented for backward-incompatible changes)
- Authenticated data includes: origin MAC, hop count, TTL, message type
- Key methods: `encrypt()`, `decrypt()`, `deriveKey()`

**TerminalManager.h** (CLI interface)
- Dual input sources: USB Serial + BLE UART (source-aware echo)
- **85+ commands** organized into 13 categories (see COMMANDS.md)
- Command history (UP/DOWN arrows), tab completion
- Four terminal modes: QUIET, NORMAL, VERBOSE, MONITOR
- Interactive menus with scrolling navigation
- Commands execute mesh operations, display stats, configure settings

**OutputManager.h** (Dual serial output)
- Broadcasts all output to both USB Serial and BLE UART simultaneously
- Strips ANSI escape codes for Bluetooth (mobile app compatibility)
- **Critical:** BLE writes are batched (not character-by-character) to prevent garbled output
- Uses 256-byte buffer for printf formatting
- BLE UART chunks sent as 64-byte packets with 10ms delays

**BLEUARTManager.h** (Wireless terminal access)
- Nordic UART Service (NUS) for iPhone/Android terminal apps
- Service UUID: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- TX Characteristic: `6E400003...` (notify to client)
- RX Characteristic: `6E400002...` (write from client)
- 256-byte circular RX buffer
- Compatible with: nRF Connect, Serial Bluetooth Terminal, Bluefruit Connect

**StateManager.h** (Thread-safe state management)
- FreeRTOS semaphore-protected connection state
- Connection states: DISCONNECTED, CONNECTING, CONNECTED, EMERGENCY
- Used by all managers to coordinate LED/display/buzzer feedback

**Config.h / Config_Basic.h** (Hardware and feature flags)
- Hardware: ESP32 DevKit C V4
- Pin definitions for buttons, LEDs, I2C, UART, RF modules (32D only)
- Feature flags control which managers are compiled
- Runtime config via `extern`: `unitName`, `currentPassphrase`, `isServer`

### Message Flow

**Emergency Broadcast:**
1. Button 3 long-press (>2s) or terminal `emergency` command triggers broadcast
2. Sets 30-second emergency window with red LED pattern (32D only)
3. Floods mesh via `meshMgr.sendEmergency()` with TTL=5 (max hops)
4. Message encrypted with AES-256-GCM, authenticated with AEAD tag
5. Each peer decrypts, verifies, relays if TTL > 0 (decrements TTL)
6. Includes GPS coordinates if GPS_ENABLED=true (32D only)
7. Recipients display emergency alert on OLED/terminal + buzzer alert

**Mesh Message Received:**
1. `MeshPeer::onReceive()` → `MeshManager::handlePeerReceive()`
2. `MeshCrypto::decrypt()` validates AEAD tag and decrypts ciphertext
3. `processPacket()` checks deduplication cache (64 entries)
4. If TTL > 0 and not seen: relay to all other peers
5. Invoke `onMeshMessage()` callback in main .ino file
6. Parse message type, format for display
7. Trigger LED/buzzer feedback, add to history (32D only)
8. Broadcast to USB + BLE Serial via `OutputManager`

**Terminal Command:**
1. User types command on USB or phone (BLE UART)
2. `TerminalManager::processInput(char, source)` with source-aware echo
3. `executeCommand()` parses and routes to category handlers
4. Commands trigger mesh sends, display updates, config changes
5. Output via `output.println()` to both USB and BLE

### Security Architecture

**Passphrase Security:**
- **No default passphrase:** Users MUST set a secure passphrase on first boot
- **Minimum length:** 8 characters (enforced at runtime)
- **Blocklist:** Common weak passphrases ("123456", "password", etc.) are rejected
- **Secure storage:** Passphrase encrypted in NVS using device-unique AES-256-GCM key
- **Device binding:** NVS encryption key derived from ESP32 MAC, prevents flash dump attacks

**BLE Security:**
- **Static PIN:** 123456 (protected by same physical access model as NVS encryption)
- **Encrypted link:** BLE link-layer encryption with bonding and MITM protection

**Message Encryption (MeshCrypto):**
- **Algorithm:** AES-256-GCM (Galois/Counter Mode) - AEAD cipher
- **Key derivation:** HKDF-SHA256 from user passphrase + salt
- **Nonce:** 96-bit random (hardware RNG, never reused)
- **Authentication:** 128-bit AEAD tag covers payload + additional data
- **Additional authenticated data:** origin MAC, hop count, TTL, message type
- **Wire overhead:** 28 bytes (12 nonce + 16 tag)

**Anti-Tracking (MAC Randomization):**
- `MESH_MAC_RANDOMIZE=true` randomizes ESP32 WiFi MAC on boot
- Optional periodic rotation every 5 minutes (`MESH_MAC_ROTATE_MS`)
- Can preserve Espressif OUI prefix for compatibility (`MESH_MAC_KEEP_OUI`)
- Prevents device tracking via MAC address correlation

**Access Control (32D only):**
- `SecurityManager.h` maintains blocklist (20 peers) and trust list (50 peers)
- Blocked peers ignored at network layer
- Trusted peers can be prioritized in routing decisions
- Lists persist to NVS if `SECURITY_PERSIST_NVS=true`

### Key Configuration

From `Config.h` (32D) and `Config_Basic.h`:

```cpp
// ENABLED FEATURES (both variants)
#define MESH_ENABLED true
#define BLE_UART_ENABLED true        // Nordic UART for wireless terminal
#define TERMINAL_ENABLED true

// 32D ONLY FEATURES
#define OLED_ENABLED true            // SSD1306 OLED display
#define LED_ENABLED false            // RGB LEDs (GPIO 1/3 conflict with UART)
#define BUZZER_PIN -1                // Buzzer disabled
#define BATTERY_ENABLED true         // Battery voltage monitoring
#define POWER_MANAGEMENT_ENABLED true
#define SECURITY_BLOCKLIST_SIZE 20

// DISABLED FEATURES (both variants)
#define BLE_ENABLED false            // Old NimBLE server/client (obsolete)
#define GPS_ENABLED false            // Optional GPS tagging

// MESH PARAMETERS
#define MESH_CHANNEL 1               // ESP-NOW WiFi channel
#define MESH_MAX_PEERS 20            // Max simultaneous mesh neighbors
#define MESH_DEFAULT_TTL 3           // Default hop limit
#define MESH_MAX_TTL 5               // Emergency broadcast TTL
#define MESH_HEARTBEAT_MS 15000      // Peer discovery interval
#define MESH_PEER_TIMEOUT_MS 60000   // Peer offline after 1 min silence
#define MESH_STORE_QUEUE_SIZE 32     // Store-and-forward buffer

// MAC RANDOMIZATION (anti-tracking)
#define MESH_MAC_RANDOMIZE true      // Randomize MAC on boot
#define MESH_MAC_ROTATE_MS 300000    // Rotate every 5 min (0 = boot only)
#define MESH_MAC_KEEP_OUI false      // Don't preserve Espressif OUI

// ENCRYPTION
#define MESH_PROTOCOL_VERSION 0x02   // Wire format version
```

### Hardware Configuration (32D Only)

- **Display:** SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22, Addr=0x3C)
- **Buttons:** 3 buttons on GPIO 39, 34, 36 (input-only ADC pins, require external 10kΩ pull-ups)
  - Button 3 short: Cycle OLED screens
  - Button 3 long: Emergency broadcast
  - Button 0 long: Cancel emergency
- **LEDs:** GPIO 0, 3, 1 (disabled - conflict with UART)
- **Serial:** USB (UART_TXD=1, UART_RXD=3) at 115200 baud
- **Battery:** ADC1_CH7 (GPIO 35) with 2:1 voltage divider

## Terminal Commands

The terminal interface includes **85+ commands** in 13 categories. See [COMMANDS.md](COMMANDS.md) for complete reference.

**Quick reference:**

**Message Commands:** `send`, `emergency`, `cancel`, `msgsearch`, `msgclear`
**Mesh Commands:** `mesh`, `peers`, `meshsend`, `broadcast`, `gps`
**Network Diagnostics:** `ping`, `traceroute`, `rssi`, `netscan`, `topology`, `stats`
**Display Commands:** `status`, `messages`, `config`
**Configuration:** `name`, `passkey`, `mode`, `ansi`, `txpower`, `timezone`
**Security:** `blocklist`, `trustlist`, `block`, `unblock`, `trust`, `untrust`
**Power Management:** `sleep`, `battery`, `powermode`, `wakeup`
**File System:** `ls`, `cat`, `rm`, `format` (if FILESYSTEM_ENABLED)
**Logging:** `logs`, `logclear`, `loglevel` (if LOG_ENABLED)
**GPS:** `gps`, `gpsdump`, `gpsreset` (if GPS_ENABLED)
**Diagnostics:** `selftest`, `hwinfo`, `memtest`, `i2cscan`
**Time:** `time`, `uptime`, `timezone`, `ntp`
**System:** `help`, `restart`, `memory`, `version`, `clear`

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

**IRAM Optimization:**
- NimBLE library consumes significant IRAM (~40KB)
- FileSystemManager disabled by default (`FILESYSTEM_ENABLED=false`) to save IRAM
- Use `IRAM_ATTR` sparingly; prefer flash execution for non-ISR code

**Encryption Performance:**
- AES-256-GCM hardware acceleration via ESP32 mbedtls
- Typical encrypt/decrypt: <2ms per message
- HKDF key derivation: ~10ms on boot (cached afterward)
- No noticeable latency impact on mesh relay

**Power Management (32D only):**
- No deep sleep by default (mesh heartbeat every 15s)
- `PowerManager` supports manual sleep modes via terminal
- Light sleep between heartbeats can extend battery life
- WiFi (ESP-NOW) must remain active for mesh participation

## Development Guidelines

**When modifying mesh protocol:**
1. Increment `MESH_PROTOCOL_VERSION` in Config.h for breaking changes
2. Update wire format documentation in MeshCrypto.h
3. Test interoperability between old and new versions

**When adding terminal commands:**
1. Add to appropriate category in TerminalManager.h
2. Update COMMANDS.md with syntax and examples
3. Consider ANSI color support and plain-text fallback

**When changing Config.h:**
- Update both Config.h (32D) and Config_Basic.h (basic) if shared
- Use feature flags (#if FEATURE_ENABLED) to conditionally compile
- Document hardware dependencies in comments

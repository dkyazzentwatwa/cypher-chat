# STARBEAM Terminal Command Enhancement - Architecture Plan

## Overview
Expanding from 25 commands to **~85 commands** across **12 categories** to create a comprehensive off-grid device management system.

---

## Command Organization (12 Categories)

### 1. **CAT_MESSAGE** - Message Commands (8 commands)
*Existing + Enhanced*
- `send` `<1-4>` - Send button message *(enhanced: add custom messages, priority)*
- `emergency` - Trigger emergency broadcast *(enhanced: custom message, types)*
- `cancel` - Cancel active emergency
- **NEW: `msgsearch`** `<keyword>` - Search message history
- **NEW: `msgclear`** - Clear message history
- **NEW: `msgexport`** - Export messages to serial
- **NEW: `msgfilter`** `<peer>` - Filter messages by sender
- **NEW: `lastmsg`** - Show last received message details

### 2. **CAT_MESH** - Mesh Networking (5 commands)
*Existing + Enhanced*
- `mesh` - Show mesh network status *(enhanced: topology, packet loss, queue depth)*
- `peers` - List discovered peers *(enhanced: sort by RSSI, timestamps, hop counts, trust markers)*
- `meshsend` `<msg>` - Send via mesh *(enhanced: delivery confirmation)*
- `broadcast` `<msg>` - Broadcast to all peers
- `gps` - Show GPS status *(enhanced: satellites, fix quality, speed, heading)*

### 3. **CAT_NETWORK_DIAG** - Network Diagnostics (7 commands)
*All New*
- **NEW: `ping`** `<peer_id>` - Test connectivity (round-trip time)
- **NEW: `traceroute`** `<peer_id>` - Show hop path to peer
- **NEW: `rssi`** - Real-time RSSI monitoring
- **NEW: `netscan`** - Force immediate peer discovery
- **NEW: `topology`** - ASCII art network tree
- **NEW: `stats`** - Detailed network statistics
- **NEW: `linkquality`** - Link quality metrics per peer

### 4. **CAT_DISPLAY** - Display Commands (3 commands)
*Existing*
- `status` - Show device status *(enhanced: battery, TX/RX, power mode)*
- `messages` - Show message history
- `config` - Show configuration

### 5. **CAT_CONFIG** - Configuration (9 commands)
*Existing + New*
- `name` `<name>` - Change unit name
- `passkey` `<6dig>` - Change passkey
- `mode` `<mode>` - Set terminal mode
- `ansi` `<on/off>` - Enable/disable ANSI colors
- **NEW: `settings`** - Show all settings at once
- **NEW: `reset`** - Factory reset (with confirmation)
- **NEW: `export`** - Export configuration
- **NEW: `import`** `<data>` - Import configuration
- **NEW: `brightness`** `<0-255>` - Set OLED brightness

### 6. **CAT_SECURITY** - Security & Access Control (5 commands)
*All New*
- **NEW: `blocklist`** - Show blocked peers
- **NEW: `block`** `<peer_id>` - Block peer
- **NEW: `unblock`** `<peer_id>` - Unblock peer
- **NEW: `verify`** - Verify passkey hash
- **NEW: `trust`** `<peer_id>` - Mark peer as trusted

### 7. **CAT_POWER** - Power Management (5 commands)
*All New*
- **NEW: `battery`** - Show battery voltage/percentage
- **NEW: `sleep`** `<seconds>` - Enter light sleep
- **NEW: `txpower`** `<0-20>` - Set WiFi TX power (dBm)
- **NEW: `deepsleep`** - Enter deep sleep (wake via button)
- **NEW: `powerstats`** - Power consumption stats

### 8. **CAT_QUEUE** - Queue Management (4 commands)
*All New*
- **NEW: `queue`** - Show outgoing message queue
- **NEW: `queueclear`** - Clear pending messages
- **NEW: `retry`** `<msg_id>` - Retry failed message
- **NEW: `queuestats`** - Queue statistics

### 9. **CAT_HARDWARE** - Hardware Diagnostics (6 commands)
*All New*
- **NEW: `selftest`** - Complete hardware self-test
- **NEW: `ledtest`** - Cycle LED colors
- **NEW: `buzztest`** - Test buzzer patterns
- **NEW: `btntest`** - Show button events (real-time)
- **NEW: `disptest`** - Display test pattern
- **NEW: `gpstest`** - Show raw GPS NMEA sentences

### 10. **CAT_LOGGING** - Logging & Debug (5 commands)
*All New*
- **NEW: `loglevel`** `<0-4>` - Set logging verbosity
- **NEW: `logs`** - Show recent log entries
- **NEW: `dmesg`** - Kernel-style system messages
- **NEW: `debug`** `<on/off>` - Enable debug output
- **NEW: `dumpmesh`** - Dump mesh state for debugging

### 11. **CAT_TIME** - Time Management (4 commands)
*All New*
- **NEW: `time`** - Show current time
- **NEW: `settime`** `<unix_ts>` - Set device time
- **NEW: `timezone`** `<offset>` - Set timezone offset
- **NEW: `ntp`** - Sync time from GPS

### 12. **CAT_FILESYSTEM** - File System (5 commands)
*All New*
- **NEW: `ls`** `[path]` - List files
- **NEW: `cat`** `<file>` - Display file contents
- **NEW: `rm`** `<file>` - Delete file
- **NEW: `df`** - Show filesystem usage
- **NEW: `format`** - Format filesystem (with confirmation)

### 13. **CAT_SYSTEM** - System Commands (14 commands)
*Existing + New*
- `help` `[cmd]` - Show help
- `clear` - Clear screen
- `menu` - Switch to menu mode
- `history` - Show command history
- `restart` - Restart device
- `uptime` - Show uptime
- `memory` - Show memory usage
- `version` - Show firmware version
- **NEW: `route`** `<peer_id>` - Show routing table
- **NEW: `hops`** `<max>` - Set max hop count
- **NEW: `reroute`** - Force route recalculation
- **NEW: `meshstats`** - Mesh protocol statistics
- **NEW: `channel`** - Show/set WiFi channel
- **NEW: `macaddr`** - Show device MAC address
- **NEW: `setrelay`** `<on/off>` - Enable/disable relay mode

---

## Command Aliases (~25 Total)

| Alias | Command | Category |
|-------|---------|----------|
| `?` | help | System |
| `h` | help | System |
| `cls` | clear | System |
| `hist` | history | System |
| `reboot` | restart | System |
| `mem` | memory | System |
| `ver` | version | System |
| `s` | send | Message |
| `sos` | emergency | Message |
| `msg` | messages | Display |
| `st` | status | Display |
| `cfg` | config | Display |
| `pk` | passkey | Config |
| `m` | mesh | Mesh |
| `p` | peers | Mesh |
| `ms` | meshsend | Mesh |
| `bc` | broadcast | Mesh |
| `bat` | battery | Power |
| `pwr` | txpower | Power |
| `q` | queue | Queue |
| `blk` | blocklist | Security |
| `topo` | topology | Network Diag |

---

## New Infrastructure Required

### 1. **PowerManager** (`PowerManager.h/.cpp`)
```cpp
class PowerManager {
  - float getBatteryVoltage()      // ADC reading
  - uint8_t getBatteryPercent()    // 0-100%
  - void enterLightSleep(uint32_t seconds)
  - void enterDeepSleep()          // Wake on GPIO
  - void setTXPower(int8_t dBm)    // 0-20 dBm
  - uint32_t getTotalPowerDraw()   // mW estimate
}
```
**Hardware:** ADC pin for battery voltage divider (GPIO 35 - VDET)

### 2. **FileSystemManager** (`FileSystemManager.h/.cpp`)
```cpp
class FileSystemManager {
  - bool begin()                   // Initialize SPIFFS/LittleFS
  - std::vector<String> listFiles(const char* path)
  - String readFile(const char* path)
  - bool writeFile(const char* path, const char* data)
  - bool deleteFile(const char* path)
  - bool format()
  - uint32_t getTotalSpace()
  - uint32_t getUsedSpace()
}
```
**Storage:** SPIFFS (default) or LittleFS for configuration/logs

### 3. **TimeManager** (`TimeManager.h/.cpp`)
```cpp
class TimeManager {
  - void setTime(time_t unixTime)
  - time_t getTime()
  - void setTimezone(int8_t offset)
  - bool syncFromGPS()
  - String formatTime(const char* format)
}
```
**Source:** GPS (if available) or manual set

### 4. **SecurityManager** (`SecurityManager.h/.cpp`)
```cpp
class SecurityManager {
  - bool isBlocked(const uint8_t* mac)
  - void blockPeer(const uint8_t* mac)
  - void unblockPeer(const uint8_t* mac)
  - bool isTrusted(const uint8_t* mac)
  - void trustPeer(const uint8_t* mac)
  - std::vector<uint64_t> getBlocklist()
  - std::vector<uint64_t> getTrustlist()
  - void saveToNVS()
  - void loadFromNVS()
}
```
**Storage:** NVS (Non-Volatile Storage) for persistence

### 5. **LogManager** (`LogManager.h/.cpp`)
```cpp
class LogManager {
  - void log(const char* level, const char* msg)
  - std::vector<String> getRecentLogs(int count)
  - void setLogLevel(uint8_t level)  // 0=NONE, 4=DEBUG
  - void clearLogs()
  - void dumpToSerial()
}
```
**Buffer:** Circular buffer (100-500 entries)

### 6. **Enhanced MeshManager** (Add methods)
```cpp
// Network Diagnostics
- uint32_t ping(const uint8_t* mac, uint16_t timeout)
- std::vector<uint8_t*> traceroute(const uint8_t* mac)
- float getLinkQuality(const uint8_t* mac)  // 0.0-1.0
- float getPacketLoss(const uint8_t* mac)   // 0.0-1.0
- String getTopologyTree()                  // ASCII art

// Routing
- std::map<uint64_t, RouteEntry> getRouteTable()
- void recalculateRoutes()

// Statistics
- NetworkStats getDetailedStats()
```

### 7. **Enhanced DisplayManager** (Add methods)
```cpp
- std::vector<Message> searchMessages(const char* keyword)
- void clearMessageHistory()
- String exportMessages()
- void filterMessagesBySender(const uint8_t* mac)
```

---

## Hardware Additions

### Battery Monitoring
- **Pin:** GPIO 35 (ADC1_CH7) - `BATTERY_ADC_PIN`
- **Circuit:** Voltage divider (100K + 100K for 2:1 ratio)
- **Range:** 0-3.3V ADC = 0-6.6V battery
- **Formula:** `voltage = (adcValue / 4095.0) * 3.3 * 2.0`

### Storage
- **SPIFFS:** Built into ESP32 flash (partition scheme: "Default 4MB with spiffs")
- **Size:** ~1.5MB available for logs, config, message backups

---

## Configuration File Changes

### `Config_Starbeam.h` - New Definitions
```cpp
// Battery Monitoring
#define BATTERY_ENABLED true
#define BATTERY_ADC_PIN 35
#define BATTERY_ADC_SAMPLES 10
#define BATTERY_MIN_VOLTAGE 3.0
#define BATTERY_MAX_VOLTAGE 4.2

// File System
#define FILESYSTEM_ENABLED true
#define FILESYSTEM_TYPE SPIFFS  // or LITTLEFS

// Time Management
#define TIME_ENABLED true
#define TIME_DEFAULT_TZ 0  // UTC

// Logging
#define LOG_BUFFER_SIZE 200
#define LOG_DEFAULT_LEVEL 2  // INFO

// Security
#define SECURITY_BLOCKLIST_SIZE 20
#define SECURITY_TRUSTLIST_SIZE 50
```

---

## Memory Footprint Estimate

| Component | RAM | Flash |
|-----------|-----|-------|
| PowerManager | 0.5KB | 8KB |
| FileSystemManager | 1KB | 12KB |
| TimeManager | 0.2KB | 4KB |
| SecurityManager | 3KB | 8KB |
| LogManager | 10KB | 6KB |
| Enhanced MeshManager | 5KB | 20KB |
| New Commands | 2KB | 40KB |
| **TOTAL ADDED** | **~22KB** | **~98KB** |

**Current Usage:** ~120KB RAM / ~800KB Flash
**After Enhancement:** ~142KB RAM / ~898KB Flash
**ESP32 Limits:** 520KB RAM / 4MB Flash ✅ **PLENTY OF ROOM**

---

## Help Menu Reorganization

### New Format (12 Sections)
```
================================================
  STARBEAM TERMINAL COMMAND REFERENCE
================================================

Shortcuts: UP/DOWN - history | TAB - complete
Type 'help <command>' for detailed help

[1] MESSAGE COMMANDS (8)
[2] MESH NETWORKING (5)
[3] NETWORK DIAGNOSTICS (7)
[4] DISPLAY COMMANDS (3)
[5] CONFIGURATION (9)
[6] SECURITY & ACCESS (5)
[7] POWER MANAGEMENT (5)
[8] QUEUE MANAGEMENT (4)
[9] HARDWARE DIAGNOSTICS (6)
[10] LOGGING & DEBUG (5)
[11] TIME MANAGEMENT (4)
[12] FILE SYSTEM (5)
[13] SYSTEM COMMANDS (14)

Total: 85 commands available
================================================
```

---

## Implementation Strategy

### Phase 1: Foundation (Tasks 1-8)
- Update headers and config
- Create new manager classes
- Add category enums

### Phase 2: Command Registration (Tasks 9-20)
- Add all new commands to registry
- Implement command handlers (stub first)
- Wire up executeCommand()

### Phase 3: Infrastructure (Tasks 21-38)
- Implement PowerManager
- Implement FileSystemManager
- Implement TimeManager
- Implement SecurityManager
- Implement LogManager

### Phase 4: Mesh Enhancements (Tasks 39-42)
- Add diagnostic methods to MeshManager
- Implement ping/traceroute
- Add topology visualization
- Add statistics tracking

### Phase 5: Feature Implementation (Tasks 43-58)
- Implement each command handler fully
- Add hardware test routines
- Add message search/export
- Add queue visibility

### Phase 6: Testing & Documentation (Tasks 59-75)
- Test all command categories
- Test help system
- Update documentation
- Commit and push

---

## Critical Design Decisions

### 1. **Non-Blocking Execution**
All long-running commands (selftest, sleep, format) must:
- Print status updates
- Allow Ctrl+C to cancel
- Not block other operations

### 2. **Pagination for Long Output**
Commands with >20 lines of output (logs, peers, topology) should:
- Display in pages
- Use `-- More -- (Space: next, Q: quit)`
- Support both USB and BLE input

### 3. **Confirmation for Destructive Actions**
Commands that delete/modify data (format, reset, queueclear) must:
- Print warning message
- Require explicit confirmation: `Are you sure? (yes/no): `
- Default to NO on timeout

### 4. **Resource Management**
- Free memory after command execution
- Close files properly
- Clean up failed operations

### 5. **Backward Compatibility**
- Existing commands must maintain identical behavior
- Alias expansion must work transparently
- Config file format must support old and new settings

---

## Success Metrics

✅ All 85 commands implemented and tested
✅ Help menu organized into 12 logical categories
✅ Memory usage stays under 200KB RAM
✅ Tab completion works for all commands
✅ Command history works seamlessly
✅ All aliases resolve correctly
✅ Comprehensive documentation created
✅ Code compiles without warnings
✅ All commands tested via USB and BLE

---

## Timeline Estimate

- **Total Tasks:** 75
- **Estimated Time:** 8-12 hours
- **Completion Target:** Single coding session

---

*Generated: 2026-02-09*
*Project: STARBEAM Off-Grid Terminal Enhancement*
*Target Branch: claude/offgrid-terminal-commands-mpjvH*

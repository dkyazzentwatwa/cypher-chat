# CYPHER-CHAT Terminal Command Reference

**Version:** 2.0.0-mesh
**Total Commands:** 85
**Date:** 2026-02-09

---

## Table of Contents

1. [Message Commands (8)](#1-message-commands)
2. [Mesh Networking (5)](#2-mesh-networking)
3. [Network Diagnostics (7)](#3-network-diagnostics)
4. [Display Commands (3)](#4-display-commands)
5. [Configuration (9)](#5-configuration)
6. [Security & Access Control (5)](#6-security--access-control)
7. [Power Management (5)](#7-power-management)
8. [Queue Management (4)](#8-queue-management)
9. [Hardware Diagnostics (6)](#9-hardware-diagnostics)
10. [Logging & Debug (5)](#10-logging--debug)
11. [Time Management (4)](#11-time-management)
12. [File System (5)](#12-file-system)
13. [System Commands (14)](#13-system-commands)

---

## 1. Message Commands

### `send <1-4>`
**Alias:** `s`
**Description:** Send predefined button message
**Usage:**
```
send 1    # Send ACK
send 2    # Send ENROUTE
send 3    # Send NEED HELP
send 4    # Send ALL GOOD
```
**Example:** `s 3` (sends NEED HELP using alias)

### `emergency`
**Alias:** `sos`
**Description:** Trigger emergency broadcast to all mesh peers
**Notes:** Broadcasts for 30 seconds, includes GPS if available
**Example:** `emergency` or `sos`

### `cancel`
**Description:** Cancel active emergency broadcast
**Example:** `cancel`

### `msgsearch <keyword>`
**Description:** Search message history for keyword
**Example:** `msgsearch help`
**Status:** Coming soon

### `msgclear`
**Description:** Clear all message history
**Example:** `msgclear`
**Status:** Coming soon

### `msgexport`
**Description:** Export messages to serial/file
**Example:** `msgexport`
**Status:** Coming soon

### `msgfilter <peer>`
**Description:** Filter messages by sender MAC address
**Example:** `msgfilter AA:BB:CC:DD:EE:FF`
**Status:** Coming soon

### `lastmsg`
**Description:** Show details of last received message
**Example:** `lastmsg`
**Status:** Coming soon

---

## 2. Mesh Networking

### `mesh`
**Alias:** `m`
**Description:** Show mesh network status and statistics
**Output:**
- MAC address
- Status (RUNNING/STOPPED)
- Online peer count
- Messages sent/received/relayed/dropped
- Store-and-forward queue status

**Example:** `mesh` or `m`

### `peers`
**Alias:** `p`
**Description:** List all discovered mesh peers
**Output:** MAC address, unit name, RSSI, hop distance, online status
**Example:** `peers` or `p`

### `meshsend <message>`
**Alias:** `ms`
**Description:** Send custom message via mesh network
**Example:** `meshsend Need assistance at checkpoint`
**Notes:** Broadcast with default TTL=3

### `broadcast <message>`
**Alias:** `bc`
**Description:** Broadcast message to all mesh peers (same as meshsend)
**Example:** `bc All clear`

### `gps`
**Description:** Show GPS status and coordinates
**Output:**
- GPS status (NO MODULE/NO FIX/2D FIX/3D FIX)
- Satellite count
- Latitude/Longitude/Altitude
- HDOP, speed
- Fix age
- Google Maps URL

**Example:** `gps`
**Requires:** GPS_ENABLED=true

---

## 3. Network Diagnostics

### `ping <peer_mac>`
**Description:** Test connectivity to specific peer (round-trip time)
**Example:** `ping AA:BB:CC:DD:EE:FF`
**Status:** Coming soon

### `traceroute <peer_mac>`
**Description:** Show hop-by-hop path to reach peer
**Example:** `traceroute AA:BB:CC:DD:EE:FF`
**Status:** Coming soon

### `rssi`
**Description:** Real-time RSSI monitoring of all peers
**Example:** `rssi`
**Notes:** Press any key to exit
**Status:** Coming soon

### `netscan`
**Description:** Force immediate peer discovery scan
**Example:** `netscan`
**Status:** Coming soon

### `topology`
**Alias:** `topo`
**Description:** Show network topology as ASCII art tree
**Example:** `topology` or `topo`
**Status:** Coming soon

### `stats`
**Description:** Show detailed network statistics
**Output:** Packet loss, link quality, throughput, etc.
**Example:** `stats`
**Status:** Coming soon

### `linkquality`
**Description:** Show link quality metrics per peer
**Example:** `linkquality`
**Status:** Coming soon

---

## 4. Display Commands

### `status`
**Alias:** `st`
**Description:** Show current device status
**Output:**
- Role (SERVER/CLIENT)
- Unit name
- BLE state
- Retry count
- Uptime
- Mesh status
- Message statistics

**Example:** `status` or `st`

### `messages`
**Alias:** `msg`
**Description:** Show message history
**Example:** `messages` or `msg`

### `config`
**Alias:** `cfg`
**Description:** Show current configuration
**Output:**
- Unit name
- Role
- Passkey status
- Terminal mode
**Example:** `config` or `cfg`

---

## 5. Configuration

### `name <name>`
**Description:** Change unit name
**Example:** `name RESCUE_01`
**Notes:** Restart required for BLE advertising update

### `passkey <6-digit>`
**Alias:** `pk`
**Description:** Change HMAC passkey (6 digits: 100000-999999)
**Example:** `passkey 654321` or `pk 654321`
**Notes:** Restart required to apply

### `mode <mode>`
**Description:** Set terminal output mode
**Modes:**
- `quiet` - Errors only
- `normal` - Status + messages (default)
- `verbose` - Full debug output
- `monitor` - Live dashboard (1Hz refresh)

**Example:** `mode verbose`

### `ansi <on/off>`
**Description:** Enable/disable ANSI color codes
**Example:** `ansi off`

### `settings`
**Description:** Show all configuration settings
**Example:** `settings`
**Status:** Coming soon

### `reset`
**Description:** Factory reset (erase all settings)
**Example:** `reset`
**Notes:** Requires confirmation
**Status:** Coming soon

### `export`
**Description:** Export configuration as JSON
**Example:** `export`
**Status:** Coming soon

### `import <json>`
**Description:** Import configuration from JSON
**Example:** `import {\"name\":\"NODE1\"}`
**Status:** Coming soon

### `brightness <0-255>`
**Description:** Set OLED display brightness
**Example:** `brightness 128`
**Status:** Coming soon

---

## 6. Security & Access Control

### `blocklist`
**Alias:** `blk`
**Description:** Show all blocked peer MAC addresses
**Example:** `blocklist` or `blk`
**Status:** Coming soon

### `block <peer_mac>`
**Description:** Block messages from specific peer
**Example:** `block AA:BB:CC:DD:EE:FF`
**Notes:** Persisted to NVS
**Status:** Coming soon

### `unblock <peer_mac>`
**Description:** Remove peer from blocklist
**Example:** `unblock AA:BB:CC:DD:EE:FF`
**Status:** Coming soon

### `verify`
**Description:** Verify passkey hash is correct
**Example:** `verify`
**Status:** Coming soon

### `trust <peer_mac>`
**Description:** Mark peer as trusted (priority routing)
**Example:** `trust AA:BB:CC:DD:EE:FF`
**Notes:** Trusted peers get message priority
**Status:** Coming soon

---

## 7. Power Management

### `battery`
**Alias:** `bat`
**Description:** Show battery voltage and percentage
**Output:**
- Voltage (V)
- Percentage (%)
- Charging status
- Low battery warning

**Example:** `battery` or `bat`
**Requires:** BATTERY_ENABLED=true, ADC on GPIO 35

### `sleep <seconds>`
**Description:** Enter light sleep mode for specified duration
**Example:** `sleep 60`
**Notes:** Wakes on timer or button press
**Status:** Coming soon

### `txpower <0-20>`
**Alias:** `pwr`
**Description:** Set WiFi transmission power (dBm)
**Range:** 0-20 dBm
**Example:** `txpower 10` or `pwr 15`
**Notes:** Higher = longer range but more power consumption
**Status:** Coming soon

### `deepsleep`
**Description:** Enter deep sleep mode (wake on button only)
**Example:** `deepsleep`
**Notes:** Device restarts on wake
**Status:** Coming soon

### `powerstats`
**Description:** Show power consumption statistics
**Example:** `powerstats`
**Status:** Coming soon

---

## 8. Queue Management

### `queue`
**Alias:** `q`
**Description:** Show outgoing message queue
**Output:** Pending messages, destinations, retry counts
**Example:** `queue` or `q`
**Status:** Coming soon

### `queueclear`
**Description:** Clear all pending messages
**Example:** `queueclear`
**Notes:** Requires confirmation
**Status:** Coming soon

### `retry <msg_id>`
**Description:** Manually retry failed message
**Example:** `retry 12AB34CD`
**Status:** Coming soon

### `queuestats`
**Description:** Show queue statistics
**Output:** Max size, drops, throughput
**Example:** `queuestats`
**Status:** Coming soon

---

## 9. Hardware Diagnostics

### `selftest`
**Description:** Run complete hardware self-test
**Tests:**
- OLED display
- LEDs (all colors)
- Buttons (press detection)
- Buzzer (if present)
- GPS (if present)
- WiFi/BLE radio

**Example:** `selftest`
**Status:** Coming soon

### `ledtest`
**Description:** Test LED colors and patterns
**Example:** `ledtest`
**Status:** Coming soon

### `buzztest`
**Description:** Test buzzer patterns
**Example:** `buzztest`
**Requires:** BUZZER_PIN != -1
**Status:** Coming soon

### `btntest`
**Description:** Show real-time button events
**Example:** `btntest`
**Notes:** Press buttons to see events, any key to exit
**Status:** Coming soon

### `disptest`
**Description:** Show test pattern on OLED
**Example:** `disptest`
**Status:** Coming soon

### `gpstest`
**Description:** Show raw GPS NMEA sentences
**Example:** `gpstest`
**Requires:** GPS_ENABLED=true
**Status:** Coming soon

---

## 10. Logging & Debug

### `loglevel <0-4>`
**Description:** Set logging verbosity
**Levels:**
- `0` - NONE
- `1` - ERROR
- `2` - INFO (default)
- `3` - WARN
- `4` - DEBUG

**Example:** `loglevel 4`
**Status:** Coming soon

### `logs`
**Description:** Show recent log entries from circular buffer
**Example:** `logs`
**Status:** Coming soon

### `dmesg`
**Description:** Show kernel-style system messages
**Example:** `dmesg`
**Status:** Coming soon

### `debug <on/off>`
**Description:** Enable/disable debug output
**Example:** `debug on`
**Status:** Coming soon

### `dumpmesh`
**Description:** Dump complete mesh state for debugging
**Output:** Peer table, routing table, queue state, statistics
**Example:** `dumpmesh`
**Status:** Coming soon

---

## 11. Time Management

### `time`
**Description:** Show current device time
**Example:** `time`
**Status:** Coming soon

### `settime <unix_timestamp>`
**Description:** Set device time (Unix timestamp)
**Example:** `settime 1738973523`
**Status:** Coming soon

### `timezone <offset>`
**Description:** Set timezone offset (-12 to +14 hours)
**Example:** `timezone -5`
**Status:** Coming soon

### `ntp`
**Description:** Sync time from GPS
**Example:** `ntp`
**Requires:** GPS_ENABLED=true
**Status:** Coming soon

---

## 12. File System

### `ls [path]`
**Description:** List files in filesystem
**Example:** `ls /logs`
**Requires:** FILESYSTEM_ENABLED=true
**Status:** Coming soon

### `cat <filename>`
**Description:** Display file contents
**Example:** `cat /config.json`
**Requires:** FILESYSTEM_ENABLED=true
**Status:** Coming soon

### `rm <filename>`
**Description:** Delete file
**Example:** `rm /old_log.txt`
**Requires:** FILESYSTEM_ENABLED=true
**Status:** Coming soon

### `df`
**Description:** Show filesystem usage statistics
**Example:** `df`
**Requires:** FILESYSTEM_ENABLED=true
**Status:** Coming soon

### `format`
**Description:** Format filesystem (erase all files)
**Example:** `format`
**Notes:** Requires confirmation
**Requires:** FILESYSTEM_ENABLED=true
**Status:** Coming soon

---

## 13. System Commands

### `help [command]`
**Aliases:** `?`, `h`
**Description:** Show help for all commands or specific command
**Example:**
```
help          # Show all commands
help mesh     # Show detailed help for 'mesh' command
? send        # Using alias
```

### `clear`
**Alias:** `cls`
**Description:** Clear terminal screen
**Example:** `clear` or `cls`

### `menu`
**Description:** Switch to interactive menu mode
**Example:** `menu`

### `history`
**Alias:** `hist`
**Description:** Show command history (last 10 commands)
**Example:** `history` or `hist`
**Notes:** Use UP/DOWN arrows to navigate history

### `restart`
**Aliases:** `r`, `reboot`
**Description:** Restart the device
**Example:** `restart` or `r`
**Notes:** 3-second countdown before reboot

### `uptime`
**Description:** Show system uptime
**Example:** `uptime`

### `memory`
**Alias:** `mem`
**Description:** Show memory usage (heap)
**Example:** `memory` or `mem`

### `version`
**Alias:** `ver`
**Description:** Show firmware version and build info
**Example:** `version` or `ver`

### `route <peer_mac>`
**Description:** Show routing table entry for peer
**Example:** `route AA:BB:CC:DD:EE:FF`
**Status:** Coming soon

### `hops <max>`
**Description:** Set maximum hop count (TTL)
**Range:** 1-5
**Example:** `hops 4`
**Status:** Coming soon

### `reroute`
**Description:** Force route recalculation
**Example:** `reroute`
**Status:** Coming soon

### `meshstats`
**Description:** Show detailed mesh protocol statistics
**Example:** `meshstats`
**Status:** Coming soon

### `channel [ch]`
**Description:** Show or set WiFi channel
**Range:** 1-14
**Example:**
```
channel       # Show current channel
channel 6     # Set to channel 6 (requires restart)
```
**Status:** Coming soon

### `macaddr`
**Description:** Show device MAC address
**Example:** `macaddr`

### `setrelay <on/off>`
**Description:** Enable/disable relay mode
**Example:** `setrelay on`
**Notes:** When off, device won't relay messages for others
**Status:** Coming soon

---

## Command Shortcuts

### Tab Completion
Press **TAB** to autocomplete commands or show matching options.

### Command History
- **UP arrow**: Previous command
- **DOWN arrow**: Next command
- Circular buffer stores last 10 commands

### Keyboard Shortcuts
- **Ctrl+C**: Clear current input line
- **Ctrl+L**: Clear screen and redisplay input
- **Backspace**: Delete character

---

## Terminal Modes

### QUIET Mode
- Errors only
- Minimal output
- Good for logging

**Command:** `mode quiet`

### NORMAL Mode (Default)
- Status updates
- Message notifications
- Standard output

**Command:** `mode normal`

### VERBOSE Mode
- Full debug output
- HMAC verification
- Button press events
- All system events

**Command:** `mode verbose`

### MONITOR Mode
- Live dashboard (1Hz refresh)
- Real-time statistics
- Network status
- Press any key to exit

**Command:** `mode monitor`

---

## Feature Flags

Commands require specific features to be enabled in `Config.h`:

| Feature | Flag | Commands Affected |
|---------|------|------------------|
| Battery Monitoring | `BATTERY_ENABLED` | battery, powerstats |
| File System | `FILESYSTEM_ENABLED` | ls, cat, rm, df, format |
| GPS | `GPS_ENABLED` | gps, gpstest, ntp |
| Power Management | `POWER_MANAGEMENT_ENABLED` | sleep, txpower, deepsleep |
| Mesh Networking | `MESH_ENABLED` | mesh, peers, meshsend, topology, ping |
| BLE UART | `BLE_UART_ENABLED` | Wireless terminal access |

---

## Examples

### Basic Usage
```
> help                     # Show all commands
> mesh                     # Check mesh status
> peers                    # List nearby devices
> send 1                   # Send ACK message
> status                   # Check device status
> battery                  # Check battery level
```

### Emergency Scenario
```
> emergency                # Trigger SOS broadcast
> gps                      # Share location
> meshsend Need assistance at checkpoint Alpha
> cancel                   # Cancel emergency after help arrives
```

### Network Diagnostics
```
> peers                    # Find peer MAC addresses
> ping AA:BB:CC:DD:EE:FF   # Test connectivity
> traceroute AA:BB:CC:DD:EE:FF  # Check route
> topology                 # Visualize network
> stats                    # View network health
```

### Power Optimization
```
> battery                  # Check battery level
> txpower 10               # Reduce TX power to save battery
> sleep 300                # Sleep for 5 minutes
```

---

## See Also

- [ARCHITECTURE_PLAN.md](ARCHITECTURE_PLAN.md) - Detailed implementation plan
- [README.md](README.md) - Project overview
- [Config.h](cypher-chat/Config.h) - Hardware configuration

---

**Project:** CYPHER-CHAT Off-Grid Communication System
**Repository:** https://github.com/anthropics/cypher-chat
**License:** MIT
**Last Updated:** 2026-02-09

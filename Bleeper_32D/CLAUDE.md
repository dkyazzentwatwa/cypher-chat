# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Compilation

Compile the firmware:
```bash
arduino-cli compile --fqbn esp32:esp32:esp32 Bleeper_32D.ino
```

Upload to device:
```bash
arduino-cli upload -p <PORT> --fqbn esp32:esp32:esp32 Bleeper_32D.ino
```

## Project Architecture

This is an ESP32-based BLE chat system that operates in either **server** or **client** mode, determined at boot time by holding the first button.

### Core Components

- **Bleeper_32D.ino**: Main entry point
  - Role detection (server vs client based on button state at boot)
  - Runtime passkey configuration via Serial
  - Shared utilities: display management, message history, button handling

- **Server.cpp/h**: BLE server implementation
  - Advertises BLE service and accepts client connections
  - Broadcasts messages to all connected clients
  - Sends ACK messages to confirm delivery
  - Supports multiple simultaneous client connections

- **Client.cpp/h**: BLE client implementation
  - Scans for and connects to server
  - Automatic reconnection with exponential backoff
  - Connection failure tracking with automatic ESP32 restart after 5 failures
  - Handles passkey authentication

- **Config.h**: Centralized configuration
  - Hardware pin definitions (I2C, OLED, buttons, buzzer)
  - BLE UUIDs and security settings
  - Runtime configuration via `extern` variables: `unitName`, `currentPasskey`, `isServer`

### Message Flow

**Server → Clients**: Server broadcasts button presses to all connected clients via `pTxCharacteristic->notify()`

**Client → Server**: Client writes to `pRemoteRxCharacteristic`, server echoes to all clients and sends ACK

**Message Format**: `UNIT_NAME:BUTTON_NUM:BUTTON_LABEL` (e.g., `CYPHER:1:ACK`)

**ACK Format**: `ACK:UNIT_NAME`

### BLE Architecture

- Service UUID: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- TX Characteristic (server→client): `6e400003-b5a3-f393-e0a9-e50e24dcca9e` (NOTIFY)
- RX Characteristic (client→server): `6e400002-b5a3-f393-e0a9-e50e24dcca9e` (WRITE)
- Security: Passkey authentication required (configurable at boot, default 123456)
- Uses NimBLE stack for lower memory footprint

### Hardware Configuration

- Display: SSD1306 OLED (I2C: SDA=21, SCL=22, Address=0x3C)
- Buttons: 4 active-low buttons on pins [17, 5, 18, 19]
- Button labels: ["ACK", "ENROUTE", "NEED HELP", "ALL GOOD"]
- Buzzer: Optional (BUZZER_PIN=-1 to disable)
- Debounce: 200ms

### State Management

**Global state** (in Bleeper_32D.ino):
- `isServer`: Role flag set during `detectRole()`
- `unitName`: Device identifier
- `currentPasskey`: BLE passkey for authentication
- `messageHistory[10]`: Circular buffer of last 10 messages

**Server state** (in Server.cpp):
- `deviceConnected`: Current connection status
- `pServer`, `pTxCharacteristic`: BLE objects

**Client state** (in Client.cpp):
- `connected`: Connection status
- `doConnect`: Trigger flag for connection attempt
- `isScanning`: Scan state
- `shouldRestart`: Restart flag after too many failures
- `connectionFailures`: Counter for retry logic

### Connection Lifecycle

**Client connection**:
1. Scan for server advertising SERVICE_UUID
2. Connect with passkey authentication
3. Discover service and characteristics
4. Subscribe to TX characteristic for notifications
5. On disconnect: retry with backoff, restart after 5 failures

**Server lifecycle**:
1. Initialize BLE server and service
2. Start advertising
3. Accept connections with passkey authentication
4. Relay messages between all connected clients

## Coding Conventions

- Use two spaces for indentation in all C++/Arduino and Markdown files
- Keep functions small and focused on a single task
- Prefer descriptive variable names over comments
- Message buffer size is consistently 32 bytes across the codebase

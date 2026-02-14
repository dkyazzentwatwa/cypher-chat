# cypher-chat - ESP32 Mesh Emergency Communication System

[![ESP32](https://img.shields.io/badge/ESP32-DevKit%20v4-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32-lightgrey.svg)](https://www.espressif.com/)
[![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)]()

**cypher-chat** is an off-grid, mesh-based emergency communication system built on ESP32. It provides multi-hop message relay, emergency broadcasts, and wireless terminal access for situations where traditional communication infrastructure is unavailable.

## 🌟 Features

### Core Capabilities
- **🔗 ESP-NOW Mesh Networking** - Multi-hop relay with up to 5 hops (~1.25km theoretical range)
- **📡 Store-and-Forward** - 32-message queue for offline nodes
- **🚨 Emergency Broadcast** - High-priority flooding with GPS coordinates (optional)
- **📱 Wireless Terminal** - Full CLI access via Bluetooth from mobile apps
- **🔐 Message Authentication** - HMAC-SHA256 with configurable passkey
- **👥 Automatic Peer Discovery** - Heartbeat-based neighbor detection (15s interval)
- **🔄 Message Deduplication** - Circular cache prevents relay loops
- **📊 OLED Display** - Real-time status, message history, and peer list

### Communication Modes
- **Mesh Networking** - Primary transport via ESP-NOW (250m per hop)
- **BLE UART** - Nordic UART Service for wireless terminal access
- **USB Serial** - Traditional wired terminal interface (115200 baud)

### Hardware Integration
- **3-Button Interface** - Tactile controls for emergency and screen cycling
- **RGB LED Indicators** - Visual feedback for connection state
- **Buzzer Support** - Audio alerts (optional)
- **GPS Integration** - Location tagging for emergencies (optional)

## 📋 Quick Start

### Hardware Requirements

**Minimum Configuration (per device):**
- ESP32 DevKit C V4 (38-pin)
- SSD1306 OLED Display (128x64, I2C)
- 3x Tactile Push Buttons (6x6mm)
- 3x 10kΩ Resistors (external pull-ups for input-only pins)
- USB cable (data-capable)
- Breadboard and jumper wires

**Optional Components:**
- RGB LED + 3x 220Ω resistors
- Passive buzzer
- GPS module (UART, 9600 baud)
- Power bank for portable deployment

**Total Cost:** ~$15-20 per device

### Pin Configuration

| Component | ESP32 Pin | Notes |
|-----------|-----------|-------|
| **OLED SDA** | GPIO 21 | I2C Data |
| **OLED SCL** | GPIO 22 | I2C Clock |
| **Button 1** | GPIO 39 | Requires external 10kΩ pull-up |
| **Button 2** | GPIO 34 | Requires external 10kΩ pull-up |
| **Button 3** | GPIO 36 | Requires external 10kΩ pull-up |
| **LED Red** | GPIO 0 | PWM channel |
| **LED Green** | GPIO 3 | PWM channel |
| **LED Blue** | GPIO 1 | PWM channel |
| **GPS RX** | GPIO 16 | Optional |
| **GPS TX** | GPIO 17 | Optional |

> **⚠️ Important:** GPIOs 34, 36, and 39 are input-only and require external 10kΩ pull-up resistors to 3.3V.

## 🚀 Installation

### Prerequisites

1. **Arduino CLI** or Arduino IDE
2. **ESP32 Board Support:**
   ```bash
   arduino-cli core install esp32:esp32
   ```

3. **Required Libraries:**
   - Adafruit SSD1306
   - Adafruit GFX Library
   - NimBLE-Arduino
   - ESP_NOW (built-in)
   - Preferences (built-in)

### Build and Flash

1. **Clone the repository:**
   ```bash
   git clone https://github.com/yourusername/cypher-chat.git
   cd cypher-chat/cypher-chat
   ```

2. **Configure your device:**
   Edit `Config.h`:
   ```cpp
   #define DEFAULT_UNIT_NAME "CYPHER-CHAT_01"  // Your device name
   #define DEFAULT_PASSKEY 123456            // Change for security!
   ```

3. **Compile:**
   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32 cypher-chat.ino
   ```

4. **Upload:**
   ```bash
   arduino-cli upload -p /dev/cu.SLAB_USBtoUART --fqbn esp32:esp32:esp32 cypher-chat.ino
   ```

## 🎮 Usage

### Physical Controls

| Button | Action | Function |
|--------|--------|----------|
| **Button 3** | Short press | Cycle OLED screens (Status → Messages → History) |
| **Button 3** | Long press (>2s) | **Emergency broadcast** (30-second window) |
| **Button 1** | Long press (>2s) | Cancel active emergency |

### LED Status Indicators

| Color | Meaning |
|-------|---------|
| 🔵 Blue | Idle / Ready |
| 🟡 Yellow | Connecting |
| 🟢 Green | Connected to mesh |
| 🔴 Red | Emergency active |

### Terminal Access

**Via USB Serial (115200 baud):**
```bash
screen /dev/cu.SLAB_USBtoUART 115200
```

**Via Bluetooth (Mobile Apps):**
- [Serial Bluetooth Terminal](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal) (Android)
- [nRF Connect](https://apps.apple.com/app/nrf-connect/id1054362403) (iOS/Android)
- iESP32 Terminal (iOS)

### Essential Commands

```bash
# Network Status
status              # Show device and mesh status
peers              # List discovered mesh peers with RSSI
mesh               # Full mesh network statistics

# Messaging
emergency          # Trigger emergency broadcast
cancel             # Cancel active emergency
send <1-4>         # Simulate button press

# Configuration
name <newname>     # Change device name
passkey <123456>   # Set authentication passkey
mode <quiet|normal|verbose|monitor>  # Terminal verbosity

# System
help               # Show all commands
restart            # Reboot ESP32
memory             # Display heap/PSRAM usage
uptime             # Show system uptime
```

## 📡 Mesh Networking Architecture

### Message Flow

```
[Device A] --250m--> [Device B] --250m--> [Device C] --250m--> [Device D]
   TTL=3                TTL=2                TTL=1                TTL=0

Each hop decrements TTL (Time To Live)
Emergency broadcasts use TTL=5 for maximum reach
```

### Key Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| **Max Hops** | 5 | Emergency broadcasts |
| **Default TTL** | 3 | Normal messages |
| **Heartbeat Interval** | 15 seconds | Peer discovery |
| **Peer Timeout** | 60 seconds | Mark peer offline |
| **Message Queue** | 32 messages | Store-and-forward buffer |
| **Dedup Cache** | 64 entries | Prevent relay loops |
| **WiFi Channel** | 1 | ESP-NOW channel |

### Message Types

- **DATA** - Normal peer-to-peer messages
- **EMERGENCY** - High-priority broadcasts (TTL=5)
- **HEARTBEAT** - Peer discovery and keepalive
- **ACK** - Message acknowledgments

## 📖 Documentation

- **[SETUP_GUIDE.md](docs/SETUP_GUIDE.md)** - Detailed hardware assembly and wiring
- **[CYPHER_CHAT_HANDBOOK.md](docs/CYPHER_CHAT_HANDBOOK.md)** - User operation manual
- **[GUIDES_INDEX.md](docs/guides/GUIDES_INDEX.md)** - Field guide, teacher pack, student guides, activist pack, and more
- **[CLAUDE.md](CLAUDE.md)** - Developer guide and architecture reference
- **[COMPILATION_STATUS.md](docs/development/COMPILATION_STATUS.md)** - Build status and known issues

## 🔧 Configuration Reference

### Feature Flags (`Config.h`)

```cpp
// Core Features
#define MESH_ENABLED true              // ESP-NOW mesh networking
#define BLE_UART_ENABLED true          // Wireless terminal via BLE
#define TERMINAL_ENABLED true          // CLI interface
#define GPS_ENABLED false              // GPS location tagging

// Mesh Parameters
#define MESH_CHANNEL 1                 // WiFi channel (1-13)
#define MESH_MAX_PEERS 20              // Max simultaneous peers
#define MESH_DEFAULT_TTL 3             // Standard hop limit
#define MESH_MAX_TTL 5                 // Emergency hop limit
#define MESH_HEARTBEAT_MS 15000        // Peer discovery interval
#define MESH_STORE_QUEUE_SIZE 32       // Store-and-forward buffer
```

## 🛠️ Development

### Build System

This project uses **Arduino CLI** for compilation and flashing. The firmware is built for the `esp32:esp32:esp32` FQBN (Fully Qualified Board Name).

### Architecture

- **Manager Pattern** - Modular components (MeshManager, OutputManager, TerminalManager, etc.)
- **Event-Driven** - Callback-based message handling
- **Thread-Safe** - FreeRTOS semaphores for shared state
- **Dual Output** - All terminal output mirrors to USB + Bluetooth

### Key Components

| Component | Responsibility |
|-----------|---------------|
| **MeshManager** | ESP-NOW networking, relay, deduplication |
| **OutputManager** | Dual serial output (USB + BLE) with batching |
| **TerminalManager** | CLI with command history and tab completion |
| **BLEUARTManager** | Nordic UART Service for wireless terminal |
| **DisplayManager** | OLED UI with 3-screen rotation |
| **ButtonManager** | Debounced input with long-press detection |
| **MessageAuth** | HMAC-SHA256 authentication |

### Recent Improvements (v1.0.0)

- ✅ **Fixed garbled Bluetooth output** - Batched BLE writes instead of character-by-character
- ✅ **Increased BLE chunk size** - 64-byte packets for better throughput
- ✅ **Removed obsolete code** - Cleaned up old BLE server/client architecture
- ✅ **Added missing pin definitions** - Complete hardware abstraction
- ✅ **Comprehensive documentation** - Updated for mesh networking

## 🤝 Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Development Guidelines

- Follow the existing code style (2-space indentation)
- Test on actual hardware before submitting
- Update documentation for user-facing changes
- Add CLAUDE.md notes for architectural changes

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## ⚠️ Disclaimer

**cypher-chat is designed for emergency communication and off-grid scenarios.**

- Not a replacement for professional emergency services
- Range estimates are theoretical (obstacles reduce range)
- Battery life depends on usage patterns
- No warranty expressed or implied

## 🙏 Acknowledgments

- Built on the ESP32 platform by Espressif
- Uses Adafruit libraries for OLED display
- NimBLE stack for Bluetooth Low Energy
- ESP-NOW protocol for mesh networking

## 📞 Support

- **Issues:** [GitHub Issues](https://github.com/yourusername/cypher-chat/issues)
- **Discussions:** [GitHub Discussions](https://github.com/yourusername/cypher-chat/discussions)
- **Documentation:** [Wiki](https://github.com/yourusername/cypher-chat/wiki)

---

**Made with ❤️ for emergency preparedness and off-grid communication**

*Last Updated: January 2026*

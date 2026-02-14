# cypher-chat

A minimal ESP32-based BLE chat for quick communication in emergencies.

## Quick start
1. Wire an ESP32 with an SSD1306 OLED display, four momentary buttons (pins 32,33,25,26) and an optional buzzer (pin 27).
2. Install the required libraries: NimBLE-Arduino, Adafruit GFX, and Adafruit SSD1306.
3. Compile the firmware:
   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32 cypher-chat.ino
   ```
4. Upload to at least two devices.
5. On boot, hold the first button to start **server** mode; otherwise the device runs as a **client**.
6. When prompted, send a custom BLE passkey over Serial within 10 seconds or the default `123456` is used.
7. Use the buttons to send preset messages. The OLED and buzzer provide status feedback and delivery acknowledgements.

## Features
- **Mesh Networking (ESP-NOW)**: Multi-hop relay up to 1.25km range (5 hops × 250m)
- **Wireless Terminal Access**: BLE UART for remote device management from iOS/Android
- **Comprehensive CLI**: 85 terminal commands across 13 categories
- **Battery Monitoring**: Real-time voltage and percentage tracking
- **Power Management**: TX power control, sleep modes for extended battery life
- **Network Diagnostics**: Ping, traceroute, topology visualization, signal monitoring
- **Security Features**: Peer blocklist, trust list, HMAC-SHA256 message authentication
- **Hardware Diagnostics**: Self-test routines for all peripherals
- **File System Support**: SPIFFS for logs, configuration, and message persistence
- **GPS Integration**: Optional GPS tagging for emergency broadcasts
- **Runtime configuration**: BLE passkey, unit name, terminal modes
- **OLED/buzzer feedback**: Connection status and message delivery
- **Message history**: Last 10 messages with metadata

### Terminal Interface
The device includes a powerful terminal interface accessible via USB or BLE UART:
- **85 commands** organized in 13 categories
- **Tab completion** and command history (UP/DOWN arrows)
- **4 terminal modes**: Quiet, Normal, Verbose, Monitor
- **ANSI color support** for better readability
- **Interactive menus** or command-line mode

See [COMMANDS.md](COMMANDS.md) for complete terminal reference.

## Emergency workflows
- **Hub and spokes**: Keep one server device stationary as a relay while multiple clients move around.
- **Ad-hoc switch**: If the server fails, reboot any client while holding the first button to promote it to server.

## Security
Messages are exchanged over BLE. Use unique passkeys for private sessions and consider enabling further encryption if needed.

## Contributing
Please read [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on building and submitting changes.

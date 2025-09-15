# cypher-chat

A minimal ESP32-based BLE chat for quick communication in emergencies.

## Quick start
1. Wire an ESP32 with an SSD1306 OLED display, four momentary buttons (pins 32,33,25,26) and an optional buzzer (pin 27).
2. Install the required libraries: NimBLE-Arduino, Adafruit GFX, and Adafruit SSD1306.
3. Compile the firmware:
   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32 Bleeper.ino
   ```
4. Upload to at least two devices.
5. On boot, hold the first button to start **server** mode; otherwise the device runs as a **client**.
6. When prompted, send a custom BLE passkey over Serial within 10 seconds or the default `123456` is used.
7. Use the buttons to send preset messages. The OLED and buzzer provide status feedback and delivery acknowledgements.

## Features
- Runtime selection between server and client.
- Configurable BLE passkey at startup.
- Simple delivery confirmation via `ACK` messages.
- OLED/buzzer feedback for connection status and incoming messages.
- Stores the last ten messages in memory for basic history.

## Emergency workflows
- **Hub and spokes**: Keep one server device stationary as a relay while multiple clients move around.
- **Ad-hoc switch**: If the server fails, reboot any client while holding the first button to promote it to server.

## Security
Messages are exchanged over BLE. Use unique passkeys for private sessions and consider enabling further encryption if needed.

## Contributing
Please read [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on building and submitting changes.

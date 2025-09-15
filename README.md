# cypher-chat
ESP32 hacker chat service.

## Hardware
- SSD1306 OLED: SDA pin 21, SCL pin 22
- Buttons (active‑low): GPIOs 32, 33, 25, 26
- Optional buzzer: GPIO 27

## Building
### Arduino CLI
1. Install dependencies:
   ```bash
   arduino-cli lib install NimBLE-Arduino Adafruit_GFX Adafruit_SSD1306
   ```
2. Compile:
   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32 .
   ```

### PlatformIO
```bash
pio run
```

## Quick Start
1. Set device names in `Config.h` (`UNIT_NAME` for clients, `SERVER_NAME` for server).
2. Uncomment `#define ROLE_SERVER` in `Config.h` and build/flash to create a server. Leave it commented for clients.
3. Power on the server, then clients. Use passkey `123456` when prompted to pair.
4. Press a button to send its preconfigured message.

### Security
Devices must pair using BLE passkey `123456` before exchanging messages.

# 🔐 Cypher Chat

<div align="center">

[![ESP32](https://img.shields.io/badge/ESP32-DevKitC_V4-blue?style=flat&logo=espressif)](https://www.espressif.com/en/products/esp32)
[![License: MIT](https://img.shields.io/badge/License-MIT-green?style=flat)](LICENSE)
[![Arduino](https://img.shields.io/badge/Arduino-CLI-orange?style=flat&logo=arduino)](https://www.arduino.cc/)
[![Mesh Networking](https://img.shields.io/badge/ESP--NOW_Mesh-purple?style=flat)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html)
[![BLE UART](https://img.shields.io/badge/BLE_UART-black?style=flat&logo=bluetooth)](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-mobile)

**Off-grid emergency mesh communication for $5 ESP32 boards**

[Quick Start](#-quick-start) • [Features](#-features) • [Hardware](#-hardware-needed) • [Commands](#-commands) • [Security](#-security)

</div>

---

## 🎯 What is Cypher Chat?

Cypher Chat turns cheap ESP32 boards into a **secure mesh communication network** that works without internet, cell towers, or WiFi infrastructure. Perfect for:

- 🏕️ **Outdoor adventures** — Stay connected in the backcountry
- 🏚️ **Emergency preparedness** — Communication when infrastructure fails
- 🎓 **Education** — Learn mesh networking, embedded systems
- 👷 **Field operations** — Team coordination without dependencies

> **$5 ESP32 + zero infrastructure = decentralized mesh chat** 🚀

---

## ⚡ Quick Start

### 1. Install Requirements

```bash
# macOS
brew install arduino-cli

# Linux (Debian/Ubuntu)
sudo apt install arduino-cli

# Windows
# Download from https://arduino-cli.github.io/
```

### 2. Setup ESP32 Core

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.x.x
```

### 3. Install Libraries

```bash
arduino-cli lib install "NimBLE-Arduino"
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "Adafruit BusIO"
```

### 4. Build & Flash

```bash
# Full version (OLED + buttons)
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app cypher-chat-32D/cypher-chat-32D.ino
arduino-cli upload -p /dev/cu.SLAB_USBtoUART --fqbn esp32:esp32:esp32:PartitionScheme=huge_app cypher-chat-32D/cypher-chat-32D.ino

# Basic version (terminal only)
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app cypher-chat-basic/cypher-chat-basic.ino
```

> **Tip:** Find your port with `arduino-cli board list`

---

## 🧰 Additional Board Builds

### Cardputer-Adv Field Console

The M5Stack Cardputer-Adv port lives in `cypher-chat-cardputer-adv/`. It adds the built-in ESP32-S3, color display, full keyboard, speaker, USB serial, BLE UART, ESP-NOW mesh radio, richer peer views, message logs, persisted settings, and Normal/Quiet/Monitor/Base Relay modes.

```bash
arduino-cli compile --fqbn "m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB,USBMode=hwcdc,CDCOnBoot=cdc" cypher-chat-cardputer-adv
```

On device, type chat messages directly and press Enter to send. Tab or Fn cycles screens, and the Menu screen exposes Status, Messages, Mesh, Peers, Settings, Modes, System, Diagnostics, and Emergency tools.

Useful Cardputer terminal commands:

```text
dump
logs
brightness 180
speaker mute
relay base
peername 3FA4 Base
trust 3FA4 trusted
resetsettings
```

### Unified ESP32-S3 Profiles

The profile-based ESP32-S3 sketch lives in `cypher-chat-s3/`. It keeps the mesh chat, BLE UART, terminal commands, settings, emergency broadcast, and message history behavior while swapping the display/input layer at compile time.

Install the shared dependencies:

```bash
arduino-cli lib install "NimBLE-Arduino" "Adafruit GFX Library" "Adafruit ST7735 and ST7789 Library" "Adafruit XCA9554" "GFX Library for Arduino" "Arduino_DriveBus" "XPowersLib" "M5Cardputer"
```

Supported profile macros:

- `BOARD_PROFILE_CARDPUTER_ADV`
- `BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18`
- `BOARD_PROFILE_WAVESHARE_TOUCH_LCD_147`

Compile one profile by changing `BOARD_PROFILE`:

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=default,CDCOnBoot=cdc,PartitionScheme=custom" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18" \
  cypher-chat-s3
```

For Waveshare upload from macOS, use the 1200-baud touch flow first:

```bash
arduino-cli board list
stty -f /dev/cu.usbmodemXXXX 1200
sleep 2
arduino-cli board list
arduino-cli upload -p /dev/cu.usbmodemYYYY --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=default,CDCOnBoot=cdc,PartitionScheme=custom" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18" \
  cypher-chat-s3
```

Serial monitor runs at `115200`:

```bash
arduino-cli monitor -p /dev/cu.usbmodemYYYY -c baudrate=115200
```

---

## 📦 Hardware Needed

| Component | Cost | Notes |
|-----------|------|-------|
| ESP32 DevKit C V4 | $5 | Main board |
| USB-C cable | $3 | Power + programming |
| OLED Display 128×64 (I2C) | $4 | Optional, 32D only |
| 3× push buttons | $1 | Optional, 32D only |
| **Total** | **~$10** | Fully functional |

### Pinout (32D variant)

```
ESP32 DevKit C V4
├── GPIO 21 → OLED SDA
├── GPIO 22 → OLED SCL  
├── GPIO 39 → Button 1 (ACK)
├── GPIO 34 → Button 2 (ENROUTE)
├── GPIO 36 → Button 3 (SOS)
└── GPIO 35 → Battery ADC (optional)
```

---

## 🎮 Features

### Mesh Networking
- **ESP-NOW** — Low-latency peer-to-peer communication
- **Multi-hop relay** — Up to 5 hops (1.25km+ range)
- **Auto topology** — Self-healing mesh
- **MAC randomization** — Anti-tracking protection

### Communication
- **4 preset messages** — ACK, ENROUTE, NEED HELP, ALL GOOD
- **Emergency broadcast** — SOS to all peers for 30 seconds
- **Message history** — Last 10 messages stored

### Terminal Interface
- **85 commands** — Full system control
- **BLE UART** — Wireless terminal from phone (nRF Connect, Bluefruit)
- **Tab completion** — Type `hel` → Tab → `help`
- **ANSI colors** — Easy reading

### Hardware Support
- **OLED display** — Status, messages, battery
- **LED indicators** — TX/RX activity
- **Battery monitoring** — Voltage + percentage
- **GPS integration** — Location tagging (optional)

### Security
- **HMAC-SHA256** — Message authentication
- **Peer blocklist** — Block malicious nodes
- **Peer trustlist** — Whitelist trusted devices
- **BLE PIN** — Custom pairing code

---

## 💬 Commands

### Send Messages

| Command | Alias | Description |
|---------|-------|-------------|
| `send 1` | `s 1` | Send ACK |
| `send 2` | `s 2` | Send ENROUTE |
| `send 3` | `s 3` | Send NEED HELP |
| `send 4` | `s 4` | Send ALL GOOD |
| `emergency` | `sos` | Broadcast SOS to all |
| `cancel` | | Cancel emergency |

### Network Status

| Command | Description |
|---------|-------------|
| `status` | Full system status |
| `mesh` | Mesh network info |
| `peers` | Connected peers list |
| `signal` | Signal strength report |

### Configuration

| Command | Description |
|---------|-------------|
| `name <callsign>` | Set unit name |
| `blepin <6digits>` | Change BLE PIN |
| `passphrase` | Change mesh key |
| `quiet` / `verbose` | Terminal mode |

> Run `help` on device for full command list!

---

## 🔒 Security

### First Boot Setup
On first boot, you'll be prompted to set a **mesh passphrase** (min 8 characters). This key encrypts all mesh traffic.

### BLE Pairing
Change the default BLE PIN before deployment:
```
blepin 847291
```

### Best Practices
- Use strong, unique passphrases per network
- Change BLE PIN from default `123456`
- Enable blocklist for untrusted peers
- Rotate keys periodically

---

## 📻 Emergency Workflows

### Hub & Spokes
```
Device A (Server) ←→ Device B ←→ Device C ←→ Device D
     ↑                    ↑              ↑
  Stationary          Mobile          Mobile
```

1. Designate one device as **server** (keep stationary)
2. Other devices auto-relay through mesh
3. If server fails → promote any client to server

### Ad-Hoc Switch
If server goes down, hold **Button 1** during reboot to promote that device to server.

---

## 📁 Project Structure

```
cypher-chat/
├── cypher-chat-32D/          # Full build (OLED, buttons, buzzer)
│   ├── cypher-chat-32D.ino  # Main sketch
│   ├── Config.h              # Hardware configuration
│   ├── src/                  # Manager classes
│   └── docs/                 # Setup guides
├── cypher-chat-basic/        # Minimal build (terminal only)
│   └── cypher-chat-basic.ino
├── cypher-chat-cardputer-adv/ # M5Stack Cardputer-Adv field console
├── cypher-chat-s3/            # ESP32-S3 board-profile sketch
├── COMMANDS.md               # Terminal command reference
├── ARCHITECTURE_PLAN.md      # System design doc
└── README.md                 # This file
```

---

## 🤝 Contributing

Contributions welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for:
- Coding standards (2-space indent, 100 char lines)
- Commit message format
- Pull request process

---

## ⚠️ Disclaimer

This software is provided **as-is** for educational and emergency preparedness purposes. Mesh communication reliability depends on hardware, environment, and configuration. Always have backup communication methods for critical situations.

---

## 📜 License

MIT License — See [LICENSE](LICENSE)

---

<div align="center">

**Built with ❤️ for the mesh community**

[↑ Back to top](#-cypher-chat)

</div>

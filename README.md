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
arduino-cli lib install "NimBLE-Arduino" "Adafruit GFX Library" "Adafruit SSD1306" "Adafruit ST7735 and ST7789 Library" "Adafruit XCA9554" "GFX Library for Arduino" "Arduino_DriveBus" "XPowersLib" "M5Cardputer"
```

### 4. Build & Flash

```bash
arduino-cli compile --fqbn "<board-fqbn>" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=<profile>" \
  cypher-chat-firmware

arduino-cli upload -p /dev/cu.usbserial-XXXX --fqbn "<board-fqbn>" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=<profile>" \
  cypher-chat-firmware
```

> **Tip:** Find your port with `arduino-cli board list`

---

## 🧰 Unified Board Profiles

The canonical firmware lives in `cypher-chat-firmware/`. All current boards now build from the same app, mesh, BLE, terminal, display, input, GPS, power, and settings code. `cypher-chat-firmware/BoardProfiles.h` is the source of truth for board-specific behavior.

Legacy folders are still kept as compatibility references while the migration settles, but new board work should go into `cypher-chat-firmware/`.

Install the shared dependencies:

```bash
arduino-cli lib install "NimBLE-Arduino" "Adafruit GFX Library" "Adafruit SSD1306" "Adafruit ST7735 and ST7789 Library" "Adafruit XCA9554" "GFX Library for Arduino" "Arduino_DriveBus" "XPowersLib" "M5Cardputer"
```

Profile table:

| Profile | Board FQBN | Hardware |
|---------|------------|----------|
| `BOARD_PROFILE_BASIC_ESP32` | `esp32:esp32:esp32:PartitionScheme=huge_app` | Bare ESP32, terminal-only |
| `BOARD_PROFILE_ESP32_32D` | `esp32:esp32:esp32:PartitionScheme=huge_app` | ESP32 + SSD1306 + 3 buttons |
| `BOARD_PROFILE_CARDPUTER_ADV` | `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB,USBMode=hwcdc,CDCOnBoot=cdc` | M5Stack Cardputer-Adv |
| `BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18` | `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=default,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB` | Waveshare ESP32-S3 1.8 AMOLED touch |
| `BOARD_PROFILE_WAVESHARE_TOUCH_LCD_147` | `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=default,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB` | Waveshare ESP32-S3 1.47 LCD touch |
| `BOARD_PROFILE_XIAO_ESP32C3_OLED_GPS_3BTN` | `esp32:esp32:XIAO_ESP32C3:PartitionScheme=no_ota` | XIAO ESP32-C3 + SSD1306 + GPS + 3 buttons |

Compile one profile by changing only the FQBN and `BOARD_PROFILE`:

```bash
arduino-cli compile --fqbn "<board-fqbn>" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=<profile>" \
  cypher-chat-firmware
```

Examples:

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32:PartitionScheme=huge_app" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=BOARD_PROFILE_BASIC_ESP32" \
  cypher-chat-firmware

arduino-cli compile --fqbn "esp32:esp32:XIAO_ESP32C3:PartitionScheme=no_ota" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=BOARD_PROFILE_XIAO_ESP32C3_OLED_GPS_3BTN" \
  cypher-chat-firmware
```

### XIAO ESP32-C3 OLED GPS 3-Button

Wiring:

| Feature | XIAO Pin |
|---------|----------|
| SSD1306 SDA | GPIO8 |
| SSD1306 SCL | GPIO9 |
| GPS TX into XIAO RX | GPIO20 |
| XIAO TX to GPS RX | GPIO21 |
| Button 1 | GPIO1, active-low to ground |
| Button 2 | GPIO2, active-low to ground |
| Button 3 | GPIO3, active-low to ground |

Button behavior:

- GPIO1 short: up / previous
- GPIO1 long: back
- GPIO2 short: down / next
- GPIO3 short: enter / menu
- GPIO3 long: emergency / SOS

On the OLED UI, GPIO1/GPIO2 cycle screens outside the menu and move selection
inside the menu. GPIO3 opens/selects the menu.

### Upload And Monitor

For Waveshare S3 boards on macOS, use the 1200-baud touch flow first:

```bash
arduino-cli board list
stty -f /dev/cu.usbmodemXXXX 1200
sleep 2
arduino-cli board list
```

Then compile and upload with the same FQBN/profile:

```bash
arduino-cli compile --upload -p /dev/cu.usbmodemYYYY --fqbn "<board-fqbn>" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=<profile>" \
  cypher-chat-firmware
```

For the Waveshare S3 USB terminal, keep DTR/RTS off so opening the monitor does
not kick the board back through USB re-enumeration:

```bash
arduino-cli monitor -p /dev/cu.usbmodemYYYY -c baudrate=115200,dtr=off,rts=off --raw
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

## Mesh Compatibility

All current Cypher-Chat firmware variants use the same default mesh language so mixed fleets can find each other without extra setup:

- `cypher-chat-firmware` profiles
- legacy `cypher-chat-basic`
- legacy `cypher-chat-32D`
- legacy `cypher-chat-cardputer-adv`
- legacy `cypher-chat-s3`

The shared default is protocol `0x01` with the starter mesh key/passkey `123456`. Basic and 32D still understand the newer encrypted `0x02` packets on receive, but they transmit the shared compatibility format by default so Cardputer, S3, and bare ESP32 boards can discover each other through normal 15-second mesh heartbeats.

Press Enter at first boot, or wait 15 seconds on a power-only boot, to accept the shared default. Change the mesh key/passkey on every device when you want a private fleet.

---

## 🔒 Security

### First Boot Setup
On first boot, you can press Enter to use the shared starter mesh key `123456`, or enter a stronger custom key/passphrase for a private fleet. Power-only boots use the starter key automatically after 15 seconds.

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
├── cypher-chat-firmware/     # Canonical unified board-profile sketch
│   ├── cypher-chat-firmware.ino
│   ├── BoardProfiles.h       # Central profile registry
│   ├── DisplayPort.*         # SSD1306, ST7789, SH8601, M5GFX, null
│   ├── InputPort.*           # Keyboard, touch, GPIO buttons, terminal-only
│   ├── GpsPort.*             # NMEA UART GPS or disabled
│   └── PowerPort.*           # M5/AXP/null power reporting
├── cypher-chat-32D/          # Full build (OLED, buttons, buzzer)
│   ├── cypher-chat-32D.ino  # Main sketch
│   ├── Config.h              # Hardware configuration
│   ├── src/                  # Manager classes
│   └── docs/                 # Setup guides
├── cypher-chat-basic/        # Minimal build (terminal only)
│   └── cypher-chat-basic.ino
├── cypher-chat-cardputer-adv/ # Legacy Cardputer compatibility sketch
├── cypher-chat-s3/            # Legacy ESP32-S3 profile sketch
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

# cypher-chat - Hardware Setup Guide

A comprehensive guide to assembling and deploying your cypher-chat mesh communication devices.

**Time Required:** 2-4 hours
**Difficulty:** Intermediate
**Cost:** ~$15-20 USD per device (3-device minimum recommended)

---

## Table of Contents

1. [Bill of Materials](#bill-of-materials)
2. [Tools Required](#tools-required)
3. [Hardware Assembly](#hardware-assembly)
4. [Software Installation](#software-installation)
5. [Initial Configuration](#initial-configuration)
6. [System Testing](#system-testing)
7. [Field Deployment](#field-deployment)
8. [Troubleshooting](#troubleshooting)

---

## Bill of Materials

### Per Device Components

| Component | Quantity | Est. Cost | Notes |
|-----------|----------|-----------|-------|
| **ESP32 DevKit C V4** | 1 | $8-12 | 38-pin version required |
| **SSD1306 OLED Display** | 1 | $5-8 | 128x64, I2C, 0.96" |
| **Tactile Buttons (6x6mm)** | 3 | $0.30 | Momentary, normally-open |
| **10kΩ Resistors** | 3 | $0.15 | Pull-up resistors for buttons |
| **220Ω Resistors** | 3 | $0.15 | LED current limiting (optional) |
| **RGB LED (Common Cathode)** | 1 | $0.30 | Or 3x individual LEDs (optional) |
| **Breadboard** | 1 | $3-5 | 830-point full-size |
| **Jumper Wires** | 20-30 | $2-3 | Male-male, assorted colors |
| **USB Cable (Micro-USB)** | 1 | $2-3 | Data-capable (not power-only) |
| **Project Enclosure (Optional)** | 1 | $5-10 | Plastic case for field deployment |
| **Power Bank (Optional)** | 1 | $10-15 | For portable/off-grid use |
| **Total per device** | | **$15-20** | Without optional components |

### Optional Components

- **Passive Buzzer** ($0.50) - Audio alerts
- **GPS Module** ($10-15) - NEO-6M or similar, UART interface
- **Antenna Extension** ($5) - External WiFi antenna for better range
- **Heat Shrink Tubing** ($2) - Wire insulation
- **Dupont Connectors** ($5) - Easy disconnect for troubleshooting

---

## Tools Required

### Essential
- Computer with USB port (Windows/macOS/Linux)
- Internet connection (for software download)
- Wire strippers or scissors

### Recommended
- Multimeter (for testing connections)
- Soldering iron + solder (for permanent builds)
- Helping hands or PCB holder
- Small needle-nose pliers
- Label maker or masking tape (for device identification)

---

## Hardware Assembly

### Step 1: Prepare the ESP32

1. **Inspect** the ESP32 DevKit C V4 for damage
2. **Identify** pin labels (GPIO numbers printed on silkscreen)
3. **Insert** into breadboard center, straddling the gap
4. **Verify** all pins are seated properly

⚠️ **Critical:** Ensure you have the **38-pin** DevKit C V4 version. The 30-pin version has different pinouts.

---

### Step 2: Wire the OLED Display

**Pin Connections:**

| OLED Pin | ESP32 Pin | Wire Color (Suggested) |
|----------|-----------|------------------------|
| VCC      | 3.3V      | Red |
| GND      | GND       | Black |
| SCL      | GPIO 22   | Yellow |
| SDA      | GPIO 21   | Green |

**Wiring Steps:**
```
1. Connect OLED VCC → ESP32 3.3V (NOT 5V!)
2. Connect OLED GND → ESP32 GND
3. Connect OLED SCL → ESP32 GPIO 22 (I2C Clock)
4. Connect OLED SDA → ESP32 GPIO 21 (I2C Data)
```

💡 **Tip:** If display shows garbled text, swap SDA/SCL. Reversed connections won't damage anything.

---

### Step 3: Wire the Buttons with Pull-up Resistors

**⚠️ CRITICAL:** GPIOs 34, 36, and 39 are **input-only** and require **external 10kΩ pull-up resistors**. Do not skip this step!

**Button Pin Connections:**

| Button | Function | ESP32 GPIO | Pull-up Resistor |
|--------|----------|------------|------------------|
| BTN1   | Cancel Emergency | GPIO 39 | 10kΩ to 3.3V |
| BTN2   | Reserved | GPIO 34 | 10kΩ to 3.3V |
| BTN3   | Cycle Display / Emergency | GPIO 36 | 10kΩ to 3.3V |

**Wiring Each Button:**

For each button, follow this pattern:

```
         3.3V
          |
        [10kΩ]  <-- Pull-up resistor
          |
          +-------- GPIO Pin (39, 34, or 36)
          |
      [Button]
          |
         GND
```

**Detailed Steps:**
1. Place 10kΩ resistor between 3.3V rail and GPIO pin
2. Connect one button leg to same GPIO pin (after resistor)
3. Connect other button leg to GND rail
4. Repeat for all 3 buttons

💡 **Why pull-ups?** GPIOs 34, 36, 39 don't have internal pull-ups. External resistors prevent floating inputs.

---

### Step 4: Wire RGB LED (Optional)

**LED Pin Connections:**

| LED Pin | ESP32 Pin | Resistor | Notes |
|---------|-----------|----------|-------|
| Red     | GPIO 0    | 220Ω     | PWM capable |
| Green   | GPIO 3    | 220Ω     | PWM capable (RXD0) |
| Blue    | GPIO 1    | 220Ω     | PWM capable (TXD0) |
| Common  | GND       | None     | Common cathode |

**Wiring Pattern:**
```
GPIO 0 → [220Ω] → LED Red Anode
GPIO 3 → [220Ω] → LED Green Anode
GPIO 1 → [220Ω] → LED Blue Anode
                  LED Cathode → GND
```

⚠️ **Note:** GPIOs 1 and 3 are used for USB serial. LEDs will flicker during terminal communication. This is normal.

---

### Step 5: Wire GPS Module (Optional)

If adding GPS for location-tagged emergency broadcasts:

**GPS Pin Connections:**

| GPS Pin | ESP32 Pin | Notes |
|---------|-----------|-------|
| VCC     | 5V or 3.3V | Check GPS module voltage |
| GND     | GND       | Common ground |
| TX      | GPIO 16   | GPS TX → ESP32 RX |
| RX      | GPIO 17   | GPS RX → ESP32 TX |

Enable in `Config.h`:
```cpp
#define GPS_ENABLED true
```

---

### Step 6: Double-Check Connections

**Verification Checklist:**

- [ ] All 3.3V connections are to 3.3V rail (NOT 5V)
- [ ] All GND connections share common ground
- [ ] OLED SDA/SCL on correct pins (21/22)
- [ ] All 3 buttons have 10kΩ pull-up resistors to 3.3V
- [ ] Button other legs connect to GND
- [ ] LED resistors in series (not parallel)
- [ ] No short circuits between power and ground
- [ ] ESP32 USB port accessible

---

## Software Installation

### Step 1: Install Arduino CLI

**macOS:**
```bash
brew install arduino-cli
```

**Linux:**
```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
```

**Windows:**
Download from [arduino.cc](https://arduino.github.io/arduino-cli/latest/installation/)

### Step 2: Install ESP32 Board Support

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

### Step 3: Install Required Libraries

```bash
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit BusIO"
arduino-cli lib install "NimBLE-Arduino"
```

---

## Initial Configuration

### Step 1: Clone Repository

```bash
git clone https://github.com/yourusername/cypher-chat.git
cd cypher-chat/cypher-chat
```

### Step 2: Configure Device Identity

Edit `Config.h`:

```cpp
#define DEFAULT_UNIT_NAME "CYPHER-CHAT_01"  // Change to unique name
#define DEFAULT_PASSKEY 123456            // Change for security!
```

**Device Naming Convention:**
- Use unique names for each device (CYPHER-CHAT_01, CYPHER-CHAT_02, etc.)
- Keep names short (max 16 characters)
- Avoid special characters

**Passkey Security:**
- All devices must use the **same passkey**
- Use 6 digits (100000-999999)
- Don't use obvious codes (111111, 123456, etc.)
- Document your passkey securely

### Step 3: Compile Firmware

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 cypher-chat.ino
```

**Expected Output:**
```
Sketch uses 1235615 bytes (94%) of program storage space.
Global variables use 45628 bytes (13%) of dynamic memory.
```

### Step 4: Upload to Device

1. **Connect** ESP32 via USB
2. **Identify** serial port:
   ```bash
   arduino-cli board list
   ```

3. **Upload:**
   ```bash
   arduino-cli upload -p /dev/cu.SLAB_USBtoUART --fqbn esp32:esp32:esp32 cypher-chat.ino
   ```

4. **Monitor** startup:
   ```bash
   screen /dev/cu.SLAB_USBtoUART 115200
   ```

### Step 5: Repeat for All Devices

Flash 2-3 more devices with **different unit names** but **identical passkeys**.

---

## System Testing

### Test 1: Power-On Self Test

**Expected Behavior:**
1. ✅ OLED displays boot logo
2. ✅ LED shows blue (idle)
3. ✅ Terminal shows banner with unit name
4. ✅ Mesh networking initializes

**Serial Output:**
```
==========================================
CYPHER-CHAT MESH COMMUNICATION SYSTEM
==========================================
Mesh networking enabled (ESP-NOW)
  - Range: ~250m (vs BLE ~30m)
  - TTL: 3 hops max
  - Store-forward queue: 32 messages
```

### Test 2: Button Functionality

**Button 3 (GPIO 36):**
- Short press → OLED cycles screens (Status → Messages → History)
- Long press (>2s) → Emergency broadcast, LED red

**Button 1 (GPIO 39):**
- Long press (>2s) → Cancel emergency, LED blue

**Troubleshooting:**
- Buttons not responding? Check 10kΩ pull-up resistors
- Stuck at one state? Verify GND connections

### Test 3: Mesh Peer Discovery

**Requirements:** 2+ powered devices within 250m

**Test Steps:**
1. Power on all devices
2. Wait 15-20 seconds (heartbeat interval)
3. Run `peers` command on device 1
4. Verify other devices appear in peer list

**Expected Output:**
```
Discovered Mesh Peers:
──────────────────────────────────
Peer #1: CYPHER-CHAT_02
  MAC: c0:5d:89:de:68:d5
  RSSI: -45 dBm (Excellent)
  Last seen: 3 seconds ago
  Status: RUNNING
```

### Test 4: Message Relay

**Requirements:** 3 devices (A, B, C)

**Setup:**
- Device A and C out of direct range
- Device B in middle (relay node)

**Test:**
1. Device A: `emergency`
2. Device C should receive via Device B relay
3. Check `mesh` command on B to verify relay count

---

## Field Deployment

### Placement Strategy

**Urban Environment:**
- Place devices 150-200m apart (buildings reduce range)
- Elevate devices (2nd floor windows, rooftops)
- Avoid metal structures that block signals

**Rural/Open Environment:**
- Place devices 200-250m apart
- Clear line of sight preferred
- Hilltops provide extended range

**Emergency Relay Chain:**
```
[Base Station] --200m-- [Relay 1] --200m-- [Relay 2] --200m-- [Remote Node]
                          ↓ 200m
                      [Mobile Unit]
```

### Power Considerations

**USB Power Bank:**
- 10,000mAh provides ~40-50 hours runtime
- Solar panel + power bank for extended deployment

**Expected Current Draw:**
- Idle: ~80-100mA
- Active mesh relay: ~120-150mA
- Emergency broadcast: ~180-200mA

---

## Troubleshooting

### OLED Display Issues

**Blank Screen:**
- ✅ Verify 3.3V power (NOT 5V)
- ✅ Check I2C connections (SDA=21, SCL=22)
- ✅ Try swapping SDA/SCL
- ✅ Run I2C scanner sketch to verify address 0x3C

**Garbled/Partial Display:**
- ✅ Poor connection - reseat wires
- ✅ Voltage drop - check power supply
- ✅ Interference - keep away from motors/relays

### Button Not Responding

**Symptoms:** Button press has no effect

**Solutions:**
- ✅ Verify 10kΩ pull-up resistor to 3.3V
- ✅ Check button wiring (one leg to GPIO, other to GND)
- ✅ Test continuity with multimeter
- ✅ Try different button

**For GPIO 34, 36, 39 specifically:**
- These pins are INPUT ONLY
- MUST have external pull-up resistors
- Internal pull-ups don't exist on these pins

### Mesh Not Connecting

**No Peers Discovered:**
- ✅ Verify all devices use same PASSKEY
- ✅ Check WiFi channel (default: 1) in Config.h
- ✅ Ensure MESH_ENABLED=true
- ✅ Restart all devices simultaneously
- ✅ Check distance (max ~250m in open air)

**Intermittent Connections:**
- ✅ Interference from WiFi routers (change MESH_CHANNEL)
- ✅ Weak signal (move closer or elevate)
- ✅ Power supply issues (check USB cable)

### Bluetooth Terminal Garbled

**Symptoms:** Text appears split mid-word on phone app

**Solution:** This was fixed in v1.0.0. Update firmware.

**If still occurring:**
- ✅ Ensure using latest firmware
- ✅ Try different Bluetooth terminal app
- ✅ Reduce terminal verbosity: `mode quiet`

### Compilation Errors

**Missing Libraries:**
```bash
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "NimBLE-Arduino"
```

**Wrong Board Selected:**
```bash
arduino-cli compile --fqbn esp32:esp32:esp32 cypher-chat.ino
```

**ESP32 Core Not Installed:**
```bash
arduino-cli core install esp32:esp32
```

---

## Advanced Configuration

### Optimizing Range

**Increase TX Power:**
Edit `MeshManager.cpp`:
```cpp
esp_wifi_set_max_tx_power(80);  // 80 = 20dBm (max)
```

**Change WiFi Channel:**
Edit `Config.h`:
```cpp
#define MESH_CHANNEL 6  // Try channels 1, 6, 11 (non-overlapping)
```

### Extending Battery Life

**Reduce Heartbeat Frequency:**
```cpp
#define MESH_HEARTBEAT_MS 30000  // 30 seconds (from 15s)
```

**Lower Display Brightness:**
Add to `DisplayManager.cpp`:
```cpp
display.dim(true);  // Reduce OLED brightness
```

---

## Next Steps

- ✅ Read [CYPHER_CHAT_HANDBOOK.md](CYPHER_CHAT_HANDBOOK.md) for usage guide
- ✅ Review [CLAUDE.md](../CLAUDE.md) for architecture details
- ✅ Join community discussions on GitHub
- ✅ Test emergency scenarios in controlled environment

---

**Document Version:** 1.0.0
**Last Updated:** January 2026
**Tested Hardware:** ESP32 DevKit C V4 (38-pin)

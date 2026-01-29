# Bleeper_32D - Complete Setup Guide

A step-by-step guide to building and deploying a 2-3 device emergency communication network.

**Time Required:** 2-4 hours
**Difficulty:** Intermediate
**Cost:** ~$60-90 USD for 3 devices

---

## Table of Contents

1. [What You'll Need](#what-youll-need)
2. [Hardware Assembly](#hardware-assembly)
3. [Software Installation](#software-installation)
4. [Flashing Firmware](#flashing-firmware)
5. [Device Configuration](#device-configuration)
6. [System Testing](#system-testing)
7. [Field Deployment](#field-deployment)
8. [Troubleshooting](#troubleshooting)

---

## What You'll Need

### Bill of Materials (Per Device)

| Component | Quantity | Estimated Cost | Notes |
|-----------|----------|----------------|-------|
| ESP32 DevKit v1 | 1 | $8-12 | 38-pin version recommended |
| SSD1306 OLED Display (128x64) | 1 | $5-8 | I2C interface (4 pins) |
| Tactile Push Buttons (6x6mm) | 4 | $0.50 | Momentary, normally-open |
| RGB LED (Common Cathode) | 1 | $0.30 | Or 3x individual LEDs |
| 220Ω Resistors | 3 | $0.10 | For RGB LED current limiting |
| Breadboard or Perfboard | 1 | $3-5 | Full-size recommended |
| Jumper Wires (Male-Male) | 20-30 | $2-3 | Assorted colors |
| USB Cable (Micro-USB) | 1 | $2-3 | Data-capable (not power-only) |
| Enclosure (Optional) | 1 | $5-10 | Plastic project box |
| **Total per device** | | **$21-30** | |
| **Total for 3 devices** | | **$63-90** | |

### Optional Components

- **Passive Buzzer** ($0.50) - Audio feedback
- **10KΩ Resistors** (4x, $0.20) - External button pull-ups (ESP32 has internal)
- **Power Bank** ($10-15) - Portable power supply
- **Heat Shrink Tubing** ($2) - Wire protection and insulation
- **Dupont Connectors** ($5) - Easy disconnect for troubleshooting

### Tools Required

- Soldering iron and solder (optional, for permanent builds)
- Wire strippers
- Multimeter (for testing connections)
- Computer with USB port (Windows/macOS/Linux)
- Internet connection (for downloading software)

---

## Hardware Assembly

### Device 1: SERVER Configuration

#### Step 1: Prepare the ESP32

1. Inspect the ESP32 board for damage
2. Identify pin labels (GPIO numbers printed on board)
3. If using breadboard, insert ESP32 in center (straddling the gap)

#### Step 2: Wire the OLED Display

**OLED Pin Connections:**

| OLED Pin | ESP32 Pin | Wire Color (Suggested) |
|----------|-----------|------------------------|
| VCC      | 3.3V      | Red                    |
| GND      | GND       | Black                  |
| SCL      | GPIO 22   | Yellow                 |
| SDA      | GPIO 21   | Green                  |

**Wiring Steps:**
```
1. Connect OLED VCC to ESP32 3.3V rail
2. Connect OLED GND to ESP32 GND rail
3. Connect OLED SCL to ESP32 GPIO 22 (I2C Clock)
4. Connect OLED SDA to ESP32 GPIO 21 (I2C Data)
```

💡 **Tip:** Double-check I2C connections - reversed SDA/SCL won't damage anything but display won't work.

#### Step 3: Wire the Buttons

**Button Pin Connections:**

| Button | Function | ESP32 GPIO | Notes |
|--------|----------|------------|-------|
| BTN0   | ACK / Cancel Emergency | GPIO 17 | Hold at boot = Server mode |
| BTN1   | ENROUTE | GPIO 5 | |
| BTN2   | NEED HELP / Emergency | GPIO 18 | Long-press = Emergency |
| BTN3   | ALL GOOD | GPIO 19 | |

**Wiring Each Button:**
```
1. Connect one leg of button to GPIO pin
2. Connect other leg of button to GND
3. ESP32 internal pull-up will be enabled in software
```

**Breadboard Layout Example:**
```
ESP32 GPIO 17 ----[BTN0]---- GND
ESP32 GPIO 5  ----[BTN1]---- GND
ESP32 GPIO 18 ----[BTN2]---- GND
ESP32 GPIO 19 ----[BTN3]---- GND
```

#### Step 4: Wire the RGB LED

**RGB LED Pinout:**
```
Common Cathode RGB LED (4 pins):
   ___
  |   |
  R G B C
  | | | |
 (Longest leg = Common Cathode/GND)
```

**Wiring with Current Limiting Resistors:**

| LED Pin | Resistor | ESP32 Pin | Wire Color |
|---------|----------|-----------|------------|
| Red Anode | 220Ω | GPIO 25 | Red |
| Green Anode | 220Ω | GPIO 26 | Green |
| Blue Anode | 220Ω | GPIO 27 | Blue |
| Common Cathode | None | GND | Black |

**Circuit Diagram:**
```
ESP32 GPIO 25 ----[220Ω]----[R]
                              |
ESP32 GPIO 26 ----[220Ω]----[G]---- (Common Cathode)
                              |               |
ESP32 GPIO 27 ----[220Ω]----[B]            ESP32 GND
```

💡 **Tip:** If using separate LEDs, connect each with its own 220Ω resistor to GND.

#### Step 5: Power Connections

**Power Rail Setup:**
1. Connect ESP32 GND to breadboard ground rail (black/blue)
2. Connect ESP32 3.3V to breadboard power rail (red)
3. Add multiple GND connections if needed (ESP32 has multiple GND pins)

#### Step 6: Final Check (Before Power-On)

**Visual Inspection Checklist:**
- [ ] No short circuits between power rails (test with multimeter)
- [ ] All button connections secure
- [ ] OLED connections correct (VCC, GND, SCL, SDA)
- [ ] RGB LED polarity correct (longest leg to GND)
- [ ] Resistors in series with LED (not bypassed)
- [ ] No loose wires or components

**Multimeter Test:**
- Set multimeter to continuity mode (beep)
- Test GND connections: ESP32 GND to breadboard GND rail (should beep)
- Test power isolation: 3.3V to GND (should NOT beep)

---

### Devices 2 & 3: CLIENT Configuration

**Repeat the exact same wiring for clients!**

The only difference between server and client is:
- **Software role detection** (hold Button 0 at boot for server)
- **Unit name** (set via terminal or code)

All hardware is identical across all devices.

---

## Software Installation

### Option A: Arduino CLI (Recommended for Production)

#### 1. Install Arduino CLI

**macOS:**
```bash
brew install arduino-cli
```

**Linux:**
```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
export PATH=$PATH:$HOME/bin
```

**Windows:**
```powershell
# Download from: https://arduino.github.io/arduino-cli/latest/installation/
# Extract arduino-cli.exe to C:\arduino-cli\
# Add to PATH
```

#### 2. Configure Arduino CLI

```bash
# Initialize configuration
arduino-cli config init

# Add ESP32 board manager URL
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# Update package index
arduino-cli core update-index

# Install ESP32 core
arduino-cli core install esp32:esp32
```

#### 3. Install Libraries

```bash
# Install Adafruit libraries
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit SSD1306"

# Clone NimBLE-Arduino
cd ~/Arduino/libraries  # macOS/Linux
# OR cd %USERPROFILE%\Documents\Arduino\libraries  # Windows

git clone https://github.com/h2zero/NimBLE-Arduino.git
```

#### 4. Clone Bleeper_32D Project

```bash
cd ~/Documents  # or your preferred location
git clone https://github.com/dkyazzentwatwa/cypher-chat.git
cd cypher-chat/Bleeper_32D
```

---

### Option B: Arduino IDE (Easier for Beginners)

#### 1. Install Arduino IDE

Download from: https://www.arduino.cc/en/software

**Install version 2.x or later** (recommended)

#### 2. Add ESP32 Board Support

1. Open Arduino IDE
2. Go to **File → Preferences**
3. In "Additional Board Manager URLs" add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Click **OK**
5. Go to **Tools → Board → Board Manager**
6. Search for "esp32"
7. Install "esp32 by Espressif Systems"

#### 3. Install Libraries

1. Go to **Sketch → Include Library → Manage Libraries**
2. Search and install:
   - "Adafruit GFX Library"
   - "Adafruit SSD1306"
3. Manually install NimBLE:
   - Download: https://github.com/h2zero/NimBLE-Arduino/archive/refs/heads/master.zip
   - Extract to: `Documents/Arduino/libraries/NimBLE-Arduino`

#### 4. Download Bleeper_32D

1. Go to: https://github.com/dkyazzentwatwa/cypher-chat
2. Click **Code → Download ZIP**
3. Extract to `Documents/Arduino/`
4. Open `Bleeper_32D/Bleeper_32D.ino` in Arduino IDE

---

## Flashing Firmware

### Pre-Flight Check

Before flashing, verify:
- [ ] USB cable is **data-capable** (not power-only)
- [ ] ESP32 is connected to computer via USB
- [ ] Correct port selected
- [ ] Correct board selected (ESP32 Dev Module)

---

### Using Arduino CLI

#### 1. Find Your ESP32 Port

```bash
arduino-cli board list
```

**Expected Output:**
```
Port         Protocol Type              Board Name FQBN Core
/dev/ttyUSB0 serial   Serial Port (USB) Unknown
```

**Port Names by OS:**
- **Linux**: `/dev/ttyUSB0` or `/dev/ttyACM0`
- **macOS**: `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`
- **Windows**: `COM3`, `COM4`, etc.

#### 2. Compile the Sketch

```bash
cd cypher-chat/Bleeper_32D

arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all Bleeper_32D.ino
```

**Expected Output:**
```
Sketch uses 740767 bytes (56%) of program storage space.
Global variables use 38720 bytes (11%) of dynamic memory.
```

✅ **Success Indicators:**
- Compilation completes without errors
- Flash usage < 80%
- RAM usage < 50%

#### 3. Upload to ESP32

**Device 1 (Server):**
```bash
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 Bleeper_32D.ino
```

⏳ **Upload takes ~30-60 seconds**

**Expected Output:**
```
Connecting.....
Writing at 0x00010000... (100%)
Wrote 740767 bytes...
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

#### 4. Repeat for Devices 2 & 3

Flash the **same firmware** to all devices:

```bash
# Device 2 (Client 1)
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 Bleeper_32D.ino

# Device 3 (Client 2)
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 Bleeper_32D.ino
```

💡 **Tip:** Only one ESP32 can be connected at a time. Disconnect one before connecting the next.

---

### Using Arduino IDE

#### 1. Select Board and Port

1. **Tools → Board → ESP32 Arduino → ESP32 Dev Module**
2. **Tools → Port → [Select your COM port]**

**Board Settings (leave defaults):**
- Upload Speed: 921600
- Flash Frequency: 80MHz
- Flash Mode: QIO
- Flash Size: 4MB
- Partition Scheme: Default

#### 2. Compile and Upload

1. Click **Verify** button (✓) to compile
2. Wait for "Done compiling" message
3. Click **Upload** button (→)
4. Watch progress bar

**Troubleshooting Upload:**
- If "Connecting..." hangs, press **BOOT** button on ESP32
- If still failing, reduce Upload Speed to 115200

#### 3. Repeat for All Devices

Flash same firmware to Devices 2 & 3.

---

## Device Configuration

### Device 1: Configure as SERVER

#### 1. Connect Serial Terminal

**Arduino CLI:**
```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

**Arduino IDE:**
1. **Tools → Serial Monitor**
2. Set baud rate to **115200**

**Alternative (macOS/Linux):**
```bash
screen /dev/ttyUSB0 115200
```

#### 2. First Boot - Role Detection

**CRITICAL:** Hold **Button 0** during power-on!

```
Power on ESP32 while holding Button 0
         ↓
Device detects server mode
         ↓
Unit name set to "CYPHER_Server"
```

**Serial Output:**
```
╔════════════════════════════════════════════════╗
║     Bleeper_32D Emergency Communication        ║
╚════════════════════════════════════════════════╝

Press 'M' for menu or any other key for command mode...
```

Press **Enter** to select command mode.

#### 3. Configure Passkey

**Serial Prompt:**
```
Enter 6-digit passkey (or press Enter for default 123456):
```

**Option 1: Use Custom Passkey (RECOMMENDED)**
```
Generate strong passkey:
Python: python3 -c "import random; print(random.randint(100000, 999999))"
Output: 847291

Type: 847291 [Enter]
```

**Option 2: Use Default (TESTING ONLY)**
```
Press: [Enter]
⚠️ WARNING: Default passkey is insecure!
```

💡 **Important:** Write down your passkey! All devices must use the same passkey.

#### 4. Verify Server Started

**Serial Output:**
```
Starting Server...
Server Ready
0 client(s)
```

**OLED Display:**
```
┌──────────────────┐
│ Server Ready     │
│ 0 client(s)      │
└──────────────────┘
```

**RGB LED:** Blue dim (idle state)

✅ **Server is ready!**

---

### Device 2 & 3: Configure as CLIENTS

#### 1. Connect Serial Terminal

Same as server configuration.

#### 2. First Boot - Auto Client Mode

**DO NOT hold Button 0** - just power on normally.

```
Power on ESP32 (no button held)
         ↓
Device detects client mode
         ↓
Unit name set to "CYPHER"
```

#### 3. Configure SAME Passkey

**CRITICAL:** Use the **exact same passkey** as server!

```
Enter 6-digit passkey:
847291 [Enter]

✓ Passkey set to: 847291
```

#### 4. Verify Client Scanning

**Serial Output:**
```
Starting BLE scan...
[SCAN] Found device: CYPHER_Server (RSSI: -45)
[CONN] Connecting to CYPHER_Server...
[AUTH] Passkey authentication required
[CONN] Connected successfully
```

**OLED Display:**
```
┌──────────────────┐
│ Client Connected │
│ CYPHER           │
└──────────────────┘
```

**RGB LED:**
- Yellow pulsing → Scanning
- Yellow blinking → Connecting
- Green solid → Connected ✅

#### 5. Repeat for Device 3

Same process as Device 2.

---

### Terminal Interface Setup (Optional)

#### Change Unit Names

Distinguish devices by giving them unique names:

**Device 2 (Client 1):**
```
> name CYPHER_Alpha
Unit name changed to: CYPHER_Alpha
Note: Restart required for BLE advertising to update.

> restart
Restarting ESP32 in 3 seconds...
```

**Device 3 (Client 2):**
```
> name CYPHER_Bravo
Unit name changed to: CYPHER_Bravo
> restart
```

#### Set Terminal Mode

**For field deployment (minimal output):**
```
> mode quiet
Terminal mode: QUIET (errors only)
```

**For debugging (full logging):**
```
> mode verbose
Terminal mode: VERBOSE (full debug)
```

---

## System Testing

### Test 1: Basic Message Send

**Setup:**
- Device 1 (Server) powered on and ready
- Device 2 (Client) connected (green LED)
- Device 3 (Client) connected (green LED)

**Test Steps:**

1. **On Device 2:** Press Button 1 (ENROUTE)

**Expected Results:**

| Device | OLED Display | RGB LED | Buzzer | Serial Output |
|--------|--------------|---------|--------|---------------|
| Device 2 | "Sent: CYPHER_Alpha:2:ENROUTE" | Cyan flash | Short beep | [SEND] Sending: CYPHER_Alpha:2:ENROUTE |
| Device 1 | "Incoming: CYPHER_Alpha:2:ENROUTE" | Blue flash | Double beep | [RECV] Message from CYPHER_Alpha |
| Device 3 | "Incoming: CYPHER_Alpha:2:ENROUTE" | Blue flash | Double beep | [RECV] Message from CYPHER_Alpha |

✅ **Pass:** All devices show message, LED flashes, buzzer beeps

❌ **Fail:** See [Troubleshooting](#troubleshooting)

---

### Test 2: HMAC Authentication (Security)

**Setup:**
- All devices connected
- Access to Device 2 serial terminal

**Test Steps:**

1. **On Device 2 terminal:**
```
> config
Device Configuration:
====================
Unit Name: CYPHER_Alpha
Passkey: 847291 (configured)
```

2. **Change passkey to wrong value:**
```
> passkey 999999
Passkey changed to: 999999
Note: Restart required to apply new passkey.

> restart
```

3. **After restart, press Button 1**

**Expected Results:**

| Device | Behavior |
|--------|----------|
| Device 2 | "Sent: CYPHER_Alpha:2:ENROUTE" |
| Device 1 | **"SPOOFED MESSAGE - HMAC mismatch"** ⚠️ |
| Device 3 | **"SPOOFED MESSAGE - HMAC mismatch"** ⚠️ |

**Serial on Device 1/3:**
```
[HMAC] ✗ Verification FAILED: CYPHER_Alpha:2:ENROUTE
Server: HMAC verification FAILED - spoofed message!
```

✅ **Pass:** Server/clients reject message with wrong HMAC

4. **Restore correct passkey:**
```
Device 2> passkey 847291
Device 2> restart
```

---

### Test 3: Delivery Confirmation

**Setup:**
- All devices with correct passkey
- Device 2 connected

**Test Steps:**

1. **On Device 2:** Press Button 3 (ALL GOOD)

2. **Watch Device 2 OLED display:**

**Before ACK received:**
```
[00:15:23] → CYPHER_Alpha:4:ALL GOOD
```

**After ACK received (within 500ms):**
```
[00:15:23] → CYPHER_Alpha:4:ALL GOOD ✓
```

**Serial on Device 2:**
```
[SEND] Sending: CYPHER_Alpha:4:ALL GOOD
[HMAC] ✓ Signature generated
[ACK]  ✓ Delivery confirmed
```

✅ **Pass:** Checkmark (✓) appears next to sent message

---

### Test 4: Emergency Broadcast

**Setup:**
- All devices connected
- Clear area (buzzer will be loud)

**Test Steps:**

1. **On Device 2:** **LONG-PRESS** Button 2 (hold for 1+ second)

**Expected Results:**

| Device | OLED Display | RGB LED | Buzzer |
|--------|--------------|---------|--------|
| Device 2 | "EMERGENCY ACTIVE / Broadcasting..." | Red blinking | Continuous urgent beeps |
| Device 1 | "Incoming: CYPHER_Alpha:0:EMERGENCY" | Red blinking | Continuous urgent beeps |
| Device 3 | "Incoming: CYPHER_Alpha:0:EMERGENCY" | Red blinking | Continuous urgent beeps |

**Serial Output (All Devices):**
```
[EMERGENCY] Emergency broadcast activated!
[EMERGENCY] Transmission 1/3 sent
[EMERGENCY] Transmission 2/3 sent
[EMERGENCY] Transmission 3/3 sent
EMERGENCY BROADCAST SENT (3x redundancy)
```

2. **Watch countdown on Device 2:**
```
EMERGENCY ACTIVE
Auto-cancel: 29s
```

3. **Manual cancel (before 30s):**

**On Device 2:** **LONG-PRESS** Button 0 (hold for 1+ second)

**Expected Results:**
- All devices: RGB LED returns to green solid
- All devices: Buzzer stops
- Display: "Emergency canceled"

✅ **Pass:** Emergency broadcasts to all, auto-cancels after 30s, manual cancel works

---

### Test 5: Reconnection After Disconnect

**Setup:**
- All devices connected

**Test Steps:**

1. **Power off Device 1 (Server)**

2. **Watch Device 2 & 3:**

**OLED Display:**
```
State: DISCONNECTED
Retry in 1s...
```

**RGB LED:** Yellow blinking

**Serial Output:**
```
[CONN] State changed to: DISCONNECTED
Retry count: 1
Retry count: 2
Retry in 4s...
Retry count: 3
Retry in 8s...
```

✅ **Pass:** Exponential backoff visible (1s → 2s → 4s → 8s...)

3. **Power on Device 1 (Server) after 20 seconds**

**Watch Device 2 & 3 auto-reconnect:**

**Serial Output:**
```
[SCAN] Found device: CYPHER_Server (RSSI: -42)
[CONN] Connecting to CYPHER_Server...
[CONN] Connected successfully
Retry count reset (successful connection)
```

**OLED Display:**
```
Client Connected
CYPHER_Alpha
```

**RGB LED:** Green solid

✅ **Pass:** Clients automatically reconnect when server returns

---

### Test 6: Terminal Interface

**Setup:**
- Device 2 connected via USB
- Serial terminal open (115200 baud)

**Test Steps:**

1. **Test status command:**
```
> status

╔════════════════════════════════════════════════╗
║ Bleeper_32D - Emergency Communication Device   ║
╠════════════════════════════════════════════════╣
║ Role: CLIENT                Unit: CYPHER_Alpha ║
║ State: CONNECTED            Retry: 0           ║
║ Uptime: 00:05:43                               ║
╚════════════════════════════════════════════════╝
```

2. **Test send command:**
```
> send 1
Sending: ACK
```

**Verify:** All devices receive "CYPHER_Alpha:1:ACK" message

3. **Test emergency command:**
```
> emergency
[EMERGENCY] Triggering emergency broadcast...
```

**Verify:** Emergency broadcasts to all devices

4. **Test memory command:**
```
> memory
Memory usage: 108KB / 320KB (33%)
Free heap: 212KB
```

5. **Test monitor mode:**
```
> mode monitor
Terminal mode: MONITOR (live dashboard)
Press any key to exit monitor mode...

╔════════════════════════════════════════════════╗
║ Bleeper_32D - LIVE MONITOR                    ║
╠════════════════════════════════════════════════╣
║ State: CONNECTED           Uptime: 00:06:15   ║
║ Retry: 0                   Memory: 212KB free ║
║ Role: CLIENT               Unit: CYPHER_Alpha ║
╠════════════════════════════════════════════════╣
║ Recent Activity:                               ║
║ (Activity log not yet implemented)            ║
╚════════════════════════════════════════════════╝
```

Press any key to exit monitor mode.

✅ **Pass:** All terminal commands work correctly

---

## Field Deployment

### Pre-Deployment Checklist

Before taking devices into the field:

- [ ] All devices have **same passkey** (not default 123456)
- [ ] Unique unit names assigned (CYPHER_Alpha, CYPHER_Bravo, etc.)
- [ ] Emergency broadcast tested and working
- [ ] All devices successfully reconnect after power cycle
- [ ] Terminal interface accessible (if needed for headless mode)
- [ ] Power supply tested (batteries or power banks)
- [ ] Enclosures secured (if using)
- [ ] RGB LED patterns understood by all users
- [ ] Button functions known by all users

### Battery Power

**Using USB Power Bank:**

1. Connect ESP32 via USB cable
2. Power bank should provide 5V/1A minimum
3. Battery life: ~8-12 hours (10,000mAh power bank)

**Battery Monitoring:**
```
> memory
Free heap: 212KB
```

💡 **Tip:** Free heap doesn't indicate battery level (ESP32 has no built-in voltage monitoring without additional hardware)

### Recommended Operating Procedure

**Startup Sequence:**

1. **Power on Server** (Device 1) first
   - Hold Button 0 during power-on
   - Wait for "Server Ready" on display
   - Verify green LED is dim blue

2. **Power on Clients** (Devices 2 & 3)
   - Normal power-on (no button)
   - Wait for "Client Connected"
   - Verify green LED is solid

3. **Test communication:**
   - Each client sends test message (Button 1)
   - Verify all devices receive

**Message Priority:**

| Message | Button | Priority | Use Case |
|---------|--------|----------|----------|
| ALL GOOD | 3 | Low | Status check-in |
| ACK | 0 | Normal | Acknowledgment |
| ENROUTE | 1 | Normal | Position update |
| NEED HELP | 2 | High | Assistance needed |
| EMERGENCY | 2 (LONG) | CRITICAL | Life-threatening situation |

**Emergency Procedure:**

1. **Sender:**
   - Long-press Button 2 (1+ second)
   - Hold device steady (3x transmission)
   - Wait for delivery confirmation

2. **Receivers:**
   - Red blinking LED = emergency alert
   - Continuous urgent beeps
   - Read message on OLED display
   - Respond appropriately

3. **Cancel:**
   - Sender: Long-press Button 0
   - Auto-cancel: 30 seconds

### Range Testing

**BLE Range Factors:**
- **Line of sight:** ~30-50 meters
- **Indoor/obstacles:** ~10-20 meters
- **Metal/concrete:** Significantly reduced
- **Human bodies:** Blocks signal (keep device exposed)

**Field Test:**
1. Start with devices 10m apart
2. Send test message
3. Gradually increase distance
4. Note maximum reliable range
5. Add 20% safety margin

**Extending Range:**
- Elevate devices (hold higher)
- Reduce obstacles between devices
- Use external antenna (requires hardware mod)

---

## Troubleshooting

### Issue: Device Won't Power On

**Symptoms:**
- No lights on ESP32
- OLED display blank
- No serial output

**Solutions:**

1. **Check USB cable:**
   - Try different cable (must be data-capable)
   - Test cable with phone charger (should show charging)

2. **Check power source:**
   - Try different USB port
   - Try different computer
   - Try USB power bank

3. **Check ESP32:**
   - Look for damaged components
   - Press RESET button
   - Check for burns/smell (indicates short circuit)

4. **Multimeter test:**
   - Measure voltage: ESP32 5V pin to GND (should be ~5V)
   - Measure voltage: ESP32 3.3V pin to GND (should be ~3.3V)

---

### Issue: OLED Display Shows Nothing

**Symptoms:**
- ESP32 powered on (LED on board)
- OLED display completely blank
- Serial output shows normal operation

**Solutions:**

1. **Check I2C connections:**
   ```
   OLED VCC → ESP32 3.3V (NOT 5V)
   OLED GND → ESP32 GND
   OLED SCL → ESP32 GPIO 22
   OLED SDA → ESP32 GPIO 21
   ```

2. **Test I2C address:**
   - Upload I2C scanner sketch
   - Verify OLED shows at address 0x3C
   - If different, update OLED_ADDR in Config.h

3. **Check OLED power:**
   - Multimeter: OLED VCC to GND (should be 3.3V)
   - Look for dim backlight (OLED may be working but contrast low)

4. **Replace OLED:**
   - Possible dead display
   - Try spare OLED if available

---

### Issue: Client Shows "SCANNING..." Forever

**Symptoms:**
- Client OLED: "State: SCANNING"
- Yellow pulsing LED
- Serial: "Starting BLE scan..." with no devices found

**Solutions:**

1. **Verify server is running:**
   - Server OLED should show "Server Ready"
   - Server LED should be blue (idle)

2. **Check passkey match:**
   ```
   Server> config
   Client> config
   # Passkeys MUST be identical
   ```

3. **Reduce distance:**
   - Move client within 2 meters of server
   - Remove obstacles between devices

4. **Restart both devices:**
   ```
   # Power cycle server first
   # Wait 10 seconds
   # Power cycle client
   ```

5. **Check BLE interference:**
   - Turn off other Bluetooth devices
   - Move away from WiFi routers
   - Try different room

6. **Serial debug:**
   ```
   [SCAN] Starting BLE scan...
   # Should see:
   [SCAN] Found device: CYPHER_Server (RSSI: -45)
   # If no devices found, check server
   ```

---

### Issue: "SPOOFED MESSAGE" Error

**Symptoms:**
- OLED: "SPOOFED MESSAGE - HMAC mismatch"
- Red LED flash
- Message not displayed

**Solutions:**

1. **Verify passkey match:**
   ```
   All devices> config
   Passkey: 847291 (configured)
   # MUST be identical on all devices
   ```

2. **Update passkey:**
   ```
   Device> passkey 847291
   Device> restart
   ```

3. **Check for corruption:**
   - Power cycle all devices
   - Re-flash firmware if persistent

4. **Verify HMAC is working:**
   ```
   # Serial output should show:
   [HMAC] ✓ Signature generated    # On sender
   [HMAC] ✓ Signature verified     # On receiver
   # If showing ✗, HMAC verification is failing
   ```

---

### Issue: Messages Not Delivered

**Symptoms:**
- Sender shows message sent
- No checkmark (✓) appears
- Receiver doesn't show message

**Solutions:**

1. **Check connection:**
   - Verify all clients show "CONNECTED" state
   - LED should be green solid (not yellow/red)

2. **Watch retry attempts:**
   ```
   [SEND] Sending: CYPHER_Alpha:2:ENROUTE
   # Wait 5 seconds
   Retry attempt 1/5
   # Wait 5 seconds
   Retry attempt 2/5
   # After 5 retries:
   Delivery FAILED - Max retries
   ```

3. **Check server:**
   - Server should show "Incoming: ..." message
   - Server should broadcast to all connected clients

4. **Restart devices:**
   - Power cycle receiver
   - Re-establish connection
   - Try sending again

---

### Issue: Emergency Broadcast Doesn't Trigger

**Symptoms:**
- Long-press Button 2
- No emergency broadcast
- Normal message sent instead

**Solutions:**

1. **Verify long-press duration:**
   - Must hold for **1+ second**
   - Too short = regular "NEED HELP" message
   - Try holding for 2 seconds

2. **Check button wiring:**
   ```
   Multimeter continuity test:
   - Button NOT pressed: Open circuit (no beep)
   - Button pressed: Closed circuit (beep)
   ```

3. **Serial debug:**
   ```
   [BUTTON] Button 2 (NEED HELP): PRESS        # Short press
   [BUTTON] Button 2 (NEED HELP): LONG PRESS   # Correct!
   ```

4. **Test with terminal:**
   ```
   > emergency
   [EMERGENCY] Triggering emergency broadcast...
   # If this works, button issue. If not, firmware issue.
   ```

---

### Issue: RGB LED Wrong Colors or Not Working

**Symptoms:**
- LED shows wrong colors
- LED doesn't change with state
- LED stays off

**Solutions:**

1. **Check LED type:**
   - **Common Cathode**: Longest leg to GND ✓
   - **Common Anode**: Longest leg to 3.3V (NOT supported - need code change)

2. **Verify resistors:**
   ```
   Each LED leg needs 220Ω resistor:
   GPIO 25 ----[220Ω]----[R LED]
   GPIO 26 ----[220Ω]----[G LED]
   GPIO 27 ----[220Ω]----[B LED]
   All connected to common GND
   ```

3. **Test individual colors:**
   ```cpp
   // Upload test sketch:
   void loop() {
     ledcWrite(LED_PIN_R, 255); delay(1000); ledcWrite(LED_PIN_R, 0);
     ledcWrite(LED_PIN_G, 255); delay(1000); ledcWrite(LED_PIN_G, 0);
     ledcWrite(LED_PIN_B, 255); delay(1000); ledcWrite(LED_PIN_B, 0);
   }
   ```

4. **Check wiring:**
   - R leg to GPIO 25
   - G leg to GPIO 26
   - B leg to GPIO 27
   - Common (longest) to GND

---

### Issue: Serial Terminal Shows Garbage

**Symptoms:**
- Serial output is unreadable characters
- Looks like: `����þ�ÿ���`

**Solutions:**

1. **Check baud rate:**
   - MUST be **115200**
   - Set in Serial Monitor settings
   - Set in terminal app

2. **Reset device:**
   - Press RESET button on ESP32
   - First few characters may be garbage (normal during boot)
   - After boot should be readable

3. **Try different terminal:**
   - Arduino Serial Monitor
   - screen
   - minicom
   - PuTTY

---

### Issue: Upload Failed / Can't Connect

**Symptoms:**
- `Failed to connect to ESP32: Timed out waiting for packet header`
- Upload hangs at "Connecting....."

**Solutions:**

1. **Hold BOOT button:**
   - Press and hold BOOT button on ESP32
   - Click Upload
   - Release BOOT when upload starts

2. **Reduce upload speed:**
   ```
   Arduino IDE: Tools → Upload Speed → 115200
   OR
   arduino-cli compile with --upload-speed 115200
   ```

3. **Try different USB port:**
   - Use USB 2.0 port (not USB 3.0)
   - Try port directly on computer (not hub)

4. **Install USB drivers:**
   - **CP2102**: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
   - **CH340**: https://sparks.gogo.co.nz/ch340.html

5. **Check for serial monitor:**
   - Close ALL serial monitors/terminals
   - Only one program can access serial port at a time

---

## Advanced Configuration

### Custom Button Labels

Edit `Config.h`:

```cpp
// Before:
const char* BUTTON_LABELS[] = { "ACK", "ENROUTE", "NEED HELP", "ALL GOOD" };

// After (customize):
const char* BUTTON_LABELS[] = { "ROGER", "MOVING", "ASSIST", "CLEAR" };
```

Recompile and upload.

### Adjust Message History Size

Edit `DisplayManager.h`:

```cpp
// Default: 10 messages
#define MAX_HISTORY_SIZE 10

// Increase to 20 (uses more RAM):
#define MAX_HISTORY_SIZE 20
```

### Disable Buzzer

Edit `Config.h`:

```cpp
// Disable buzzer:
#define BUZZER_PIN -1
```

### Change LED Pins

Edit `Config.h`:

```cpp
// Default pins:
#define LED_PIN_R     25
#define LED_PIN_G     26
#define LED_PIN_B     27

// Change to different GPIOs:
#define LED_PIN_R     32
#define LED_PIN_G     33
#define LED_PIN_B     14
```

---

## Next Steps

### After Successful Setup

1. **Document your configuration:**
   - Write down passkey (secure location)
   - Note unit names for each device
   - Record any custom button labels

2. **Create backup firmware:**
   ```bash
   # Save compiled binary
   cp /tmp/arduino/sketches/*/Bleeper_32D.ino.bin ~/bleeper_backup.bin
   ```

3. **Test in real conditions:**
   - Range test in deployment environment
   - Test with obstacles (walls, trees)
   - Verify emergency broadcast distance

4. **Train users:**
   - Demonstrate each button function
   - Practice emergency broadcast
   - Show LED color meanings

5. **Plan for expansion:**
   - Consider adding more client devices
   - Explore WiFi gateway option (future)
   - Implement GPS coordinates (future)

---

## Support & Resources

- **Documentation:** [README.md](README.md) - Complete feature list
- **Architecture:** [CLAUDE.md](CLAUDE.md) - Technical details
- **Issues:** [GitHub Issues](https://github.com/dkyazzentwatwa/cypher-chat/issues)
- **ESP32 Resources:** https://docs.espressif.com/projects/arduino-esp32/

---

## Safety & Compliance

⚠️ **Important Notices:**

- **Not for life-critical systems:** This is a development project, not certified for emergency services
- **BLE range limitations:** Maximum ~50m line-of-sight, much less with obstacles
- **No encryption at rest:** Messages stored in plaintext in device memory
- **Battery safety:** Use quality power banks, monitor for overheating
- **FCC/CE compliance:** Verify local regulations for BLE transmitter use

---

**Setup Guide Version:** 1.0
**Last Updated:** January 2025
**Tested on:** ESP32-WROOM-32, Arduino CLI 0.35.0, ESP32 Core 3.0.0

---

**Questions?** Open an issue on GitHub or check the troubleshooting section above.

**Happy Building! 🛠️**

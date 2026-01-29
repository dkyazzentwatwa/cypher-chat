# 🛰️ Starbeam / Bleeper_32D Handbook
### *Beginner-Friendly Setup & Operation Guide*

Welcome to your secure, off-grid emergency communication system. This handbook will guide you through setting up your **Starbeam** hardware and using the **Bleeper_32D** firmware.

---

## 1. Hardware Overview (Starbeam)

Your Starbeam board is a specialized gateway. Note the specific requirements:

*   **Processor:** ESP32 DevKit C V4
*   **Buttons (Crucial):** 
    *   **KEY1 (GPIO 39):** Send Message 1 / Cancel Emergency.
    *   **KEY2 (GPIO 34):** Send Message 2.
    *   **KEY3 (GPIO 36):** Cycle Screens / Emergency (Long-press).
    *   *Note:* These pins **require external 10kΩ resistors** connected to 3.3V (Pull-ups).
*   **Modules:** Supports NRF24L01 and CC1101 for long-range RF.
*   **Display:** 0.96" OLED (SSD1306).

---

## 2. Software Setup

### Prerequisites
1.  **Arduino CLI:** [Download here](https://arduino.github.io/arduino-cli/latest/installation/) or use Arduino IDE.
2.  **Required Libraries:**
    *   `NimBLE-Arduino` (For BLE)
    *   `Adafruit SSD1306` & `Adafruit GFX` (For Display)
    *   `ESP32_NOW` (Built-in)

### Configuration
Open `Config_Starbeam.h` to set your identity:
```cpp
#define DEFAULT_UNIT_NAME "STARBEAM_01" // Your device name
#define DEFAULT_PASSKEY 123456          // Change this for security!
```

### Flashing (Uploading)
Connect your device via USB. Run these commands in your terminal:
```bash
# 1. Compile the code
arduino-cli compile --fqbn esp32:esp32:esp32 Bleeper_32D.ino

# 2. Upload (Replace PORT with your serial port, e.g., /dev/cu.SLAB_USBtoUART)
arduino-cli upload -p PORT --fqbn esp32:esp32:esp32 Bleeper_32D.ino
```

---

## 3. Operating Instructions

### Physical Interface (Buttons)
*   **Short Press KEY1/KEY2:** Sends a pre-defined message (e.g., ACK).
*   **Short Press KEY3:** Cycles the OLED display through three modes:
    1.  **Status:** Connection info and Signal strength.
    2.  **Messages:** Shows the last 3 received messages.
    3.  **History:** A scrollable list of the last 10 activities.
*   **Long Press KEY3 (1 sec):** Triggers a high-priority **Emergency Broadcast**.
*   **Long Press KEY1 (1 sec):** Cancels an active Emergency.

### LED Indicators
*   **LED 1 (GPIO 0):** Solid when the device is idle/powered.
*   **LED 3 (RXD):** Flashes when a message is received.
*   **LED 4 (TXD):** Flashes when a message is sent or an Emergency is active.

---

## 4. Android Mobile Setup (USB Serial)

You can control your Starbeam entirely from your phone without a computer.

### Required Tools
*   **OTG Adapter:** (USB-C or Micro-USB to USB-A female).
*   **App:** [Serial USB Terminal](https://play.google.com/store/apps/details?id=de.kai_morich.serial_usb_terminal) (highly recommended).

### Connection Steps
1.  Connect the Starbeam to your phone using the OTG adapter and USB cable.
2.  Open **Serial USB Terminal**.
3.  Go to **Settings > Serial**:
    *   **Baudrate:** 115200
    *   **Data bits:** 8
    *   **Parity:** None
    *   **Stop bits:** 1
4.  Tap the **Connect** icon (top right).
5.  **Role Selection:** On boot, the terminal will ask for a role. 
    *   Press `M` for the **Interactive Menu**.
    *   Wait 3 seconds for **Command Line Mode**.

---

## 5. Command List (Terminal Only)

Type these into your Serial app or Computer terminal:

| Command | Alias | Description |
| :--- | :--- | :--- |
| `status` | `st` | Shows connection state and battery/system health. |
| `peers` | `p` | Lists all other Starbeam nodes discovered in the Mesh. |
| `send <1-3>` | `s` | Sends message assigned to KEY 1, 2, or 3. |
| `emergency` | `sos` | Triggers the system-wide emergency alert. |
| `cancel` | - | Stops the emergency alert. |
| `meshsend <msg>`| `ms` | Sends a custom text message to the whole mesh network. |
| `gps` | - | Shows coordinates (if GPS module is attached). |
| `name <name>` | - | Changes your device name (temporary). |
| `restart` | - | Reboots the ESP32. |
| `help` | `h` | Displays the full help menu. |

---

## 6. Troubleshooting
*   **Buttons don't respond:** Check your external resistors. GPIO 34/36/39 will not work without them.
*   **OLED is blank:** Ensure the I2C address is `0x3C` in `Config.h`.
*   **Phone doesn't see device:** Ensure your Android supports OTG and it is enabled in Android System Settings.
*   **Permission Denied (PC):** If you can't upload, close any open Serial Monitors (like `screen` or Arduino Monitor).

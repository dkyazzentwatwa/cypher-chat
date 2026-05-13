# Cypher Chat Unified Firmware

`cypher-chat-firmware/` is the canonical Cypher Chat sketch. Every supported board builds from the same mesh, BLE UART, terminal, display, input, GPS, settings, and emergency-runtime code. Board-specific wiring and feature choices live in `BoardProfiles.h` and are selected at compile time with `BOARD_PROFILE`.

Legacy folders are kept as compatibility references during migration. New boards should be added here, not as another duplicated sketch folder.

## Shared Defaults

- Mesh protocol: `0x01`
- Starter mesh key/passkey: `123456`
- First-boot unattended setup timeout: 15 seconds
- ESP-NOW heartbeat discovery: 15 seconds
- Terminal baud: `115200`

## Supported Profiles

| Profile | Board FQBN | Display/Input/GPS |
|---------|------------|-------------------|
| `BOARD_PROFILE_BASIC_ESP32` | `esp32:esp32:esp32:PartitionScheme=huge_app` | No display, terminal-only, GPS off |
| `BOARD_PROFILE_ESP32_32D` | `esp32:esp32:esp32:PartitionScheme=huge_app` | SSD1306 on GPIO21/22, 3 GPIO buttons, GPS off |
| `BOARD_PROFILE_CARDPUTER_ADV` | `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB,USBMode=hwcdc,CDCOnBoot=cdc` | M5GFX display, Cardputer keyboard, M5 power, raw 135x240 panel rotated to 240x135 |
| `BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18` | `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=default,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB` | SH8601 AMOLED, touch, AXP2101 power |
| `BOARD_PROFILE_WAVESHARE_TOUCH_LCD_147` | `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=default,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB` | ST7789 LCD, touch |
| `BOARD_PROFILE_XIAO_ESP32C3_OLED_GPS_3BTN` | `esp32:esp32:XIAO_ESP32C3:PartitionScheme=no_ota` | SSD1306, 3 GPIO buttons, UART GPS |

## Dependencies

```bash
arduino-cli lib install "NimBLE-Arduino" "Adafruit GFX Library" "Adafruit SSD1306" "Adafruit ST7735 and ST7789 Library" "Adafruit XCA9554" "GFX Library for Arduino" "Arduino_DriveBus" "XPowersLib" "M5Cardputer"
```

## Compile Pattern

```bash
arduino-cli compile --fqbn "<board-fqbn>" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=<profile>" \
  cypher-chat-firmware
```

Example for XIAO ESP32-C3 with OLED, GPS, and 3 buttons:

```bash
arduino-cli compile --fqbn "esp32:esp32:XIAO_ESP32C3:PartitionScheme=no_ota" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=BOARD_PROFILE_XIAO_ESP32C3_OLED_GPS_3BTN" \
  cypher-chat-firmware
```

## XIAO ESP32-C3 Wiring

| Feature | Pin |
|---------|-----|
| SSD1306 SDA | GPIO8 |
| SSD1306 SCL | GPIO9 |
| GPS TX into XIAO RX | GPIO20 |
| XIAO TX to GPS RX | GPIO21 |
| Button 1 | GPIO1, active-low `INPUT_PULLUP` |
| Button 2 | GPIO2, active-low `INPUT_PULLUP` |
| Button 3 | GPIO3, active-low `INPUT_PULLUP` |

Button mapping:

- GPIO1 short: up / previous
- GPIO1 long: back
- GPIO2 short: down / next
- GPIO3 short: enter / menu
- GPIO3 long: emergency / SOS

On the XIAO OLED UI, GPIO1/GPIO2 cycle screens outside the menu and move
selection inside the menu. GPIO3 opens/selects the menu.

Waveshare touch dashboard controls:

- Boot short press: next page
- Swipe left/right: Status, Messages, Mesh, SOS pages
- Swipe up/down: scroll Messages history or Mesh peers
- Tap Messages: jump back to latest messages
- Tap SOS: shows terminal command guidance only

The Waveshare AMOLED and 1.47 LCD profiles are display-first: settings,
passkey changes, restart, and SOS broadcast sending stay in the USB/BLE
terminal commands.

## Upload And Monitor

For boards that expose a USB serial bootloader normally:

```bash
arduino-cli compile --upload -p /dev/cu.usbserial-XXXX --fqbn "<board-fqbn>" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=<profile>" \
  cypher-chat-firmware
arduino-cli monitor -p /dev/cu.usbserial-XXXX -c baudrate=115200
```

For S3 boards that need bootloader touch on macOS, use the 1200-baud touch first:

```bash
arduino-cli board list
stty -f /dev/cu.usbmodemXXXX 1200
sleep 2
arduino-cli board list
arduino-cli compile --upload -p /dev/cu.usbmodemYYYY --fqbn "<board-fqbn>" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=<profile>" \
  cypher-chat-firmware
arduino-cli monitor -p /dev/cu.usbmodemYYYY -c baudrate=115200,dtr=off,rts=off --raw
```

## Useful Commands

```text
status
peers
send 1
emergency
gps
dump
logs
brightness 180
speaker mute
relay base
peername 3FA4 Base
trust 3FA4 trusted
resetsettings
```

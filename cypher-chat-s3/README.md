# Cypher Chat S3 Board Profiles

`cypher-chat-s3/` is a profile-based ESP32-S3 sketch for newer display boards. It keeps the Cypher Chat mesh, BLE UART, terminal commands, settings, emergency broadcast, and message history behavior while swapping display/input support at compile time.

## Supported Profiles

- `BOARD_PROFILE_CARDPUTER_ADV`
- `BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18`
- `BOARD_PROFILE_WAVESHARE_TOUCH_LCD_147`

## Dependencies

```bash
arduino-cli lib install "NimBLE-Arduino" "Adafruit GFX Library" "Adafruit ST7735 and ST7789 Library" "Adafruit XCA9554" "GFX Library for Arduino" "Arduino_DriveBus" "XPowersLib" "M5Cardputer"
```

## Compile

Use the ESP32-S3 FQBN and set the board profile through `build.extra_flags`:

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=default,CDCOnBoot=cdc,PartitionScheme=custom" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18" \
  cypher-chat-s3
```

Change only the `BOARD_PROFILE=...` value to build the other profiles.

## Upload And Monitor

For Waveshare S3 boards on macOS, use the touch-first upload flow:

```bash
arduino-cli board list
stty -f /dev/cu.usbmodemXXXX 1200
sleep 2
arduino-cli board list
arduino-cli upload \
  -p /dev/cu.usbmodemYYYY \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=default,CDCOnBoot=cdc,PartitionScheme=custom" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18" \
  cypher-chat-s3
```

Serial monitor runs at 115200 baud:

```bash
arduino-cli monitor -p /dev/cu.usbmodemYYYY -c baudrate=115200
```

## Useful Commands

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

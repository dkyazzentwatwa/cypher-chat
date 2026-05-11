# Cypher Chat S3 Board Profiles

> Migration note: this folder is kept as a legacy compatibility sketch. New S3 board work should use `../cypher-chat-firmware/` and select `BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18` or `BOARD_PROFILE_WAVESHARE_TOUCH_LCD_147` from `BoardProfiles.h`.

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
arduino-cli compile --upload \
  -p /dev/cu.usbmodemYYYY \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=default,CDCOnBoot=cdc,PartitionScheme=custom" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18" \
  cypher-chat-s3
```

For the Waveshare S3 USB terminal, keep DTR/RTS off:

```bash
arduino-cli monitor -p /dev/cu.usbmodemYYYY -c baudrate=115200,dtr=off,rts=off --raw
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

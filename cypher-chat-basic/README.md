# Cypher Chat Basic

This folder is kept as a legacy compatibility sketch for bare ESP32 boards.

New work should use the canonical unified firmware in `../cypher-chat-firmware/` with `BOARD_PROFILE_BASIC_ESP32`.

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32:PartitionScheme=huge_app" \
  --build-property build.extra_flags="-DESP32 -DBOARD_PROFILE=BOARD_PROFILE_BASIC_ESP32" \
  ../cypher-chat-firmware
```

The unified profile keeps the same shared mesh defaults: protocol `0x01`, starter key/passkey `123456`, 15-second unattended setup, BLE UART, USB serial terminal, and 15-second ESP-NOW heartbeats.

# Cypher Chat Cardputer-Adv Field Console

M5Stack Cardputer-Adv version of Cypher Chat. This is the flagship field-console sketch: ESP-NOW mesh chat, HMAC passkey, USB serial terminal, BLE UART terminal, color display, full keyboard, speaker, battery monitor, richer peer views, local message logs, and persisted device settings.

## Build

```bash
arduino-cli compile --fqbn "m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB,USBMode=hwcdc,CDCOnBoot=cdc" cypher-chat-cardputer-adv
```

Upload with the same FQBN plus the detected serial port:

```bash
arduino-cli upload -p /dev/cu.usbmodemXXXX --fqbn "m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB,USBMode=hwcdc,CDCOnBoot=cdc" cypher-chat-cardputer-adv
```

If upload mode is stubborn, power the Cardputer-Adv off, hold `G0`, power it on, then run the upload command again.

## Controls

- Type a message on the keyboard, then press Enter to send it over mesh.
- Backspace deletes from the compose line.
- Tab or Fn cycles Status, Chat, Peers, and Menu.
- In menus, use the blue up/down arrow keys or `W`/`S` to move, Enter to open/save/run, and Backspace/Del or `Q` to go back.
- Global quick presets outside the menu: `Fn+1` sends ACK, `Fn+2` sends ENROUTE, `Fn+3` sends NEED HELP, and `Fn+4` sends ALL GOOD.
- Inside the Messages menu, plain `1` through `4` send those same presets.
- The top-level menu includes Status, Messages, Mesh, Peers, Settings, Modes, System, Diagnostics, and Emergency.
- Settings can edit the device name, edit the six-digit passkey, set brightness, mute the speaker, cycle terminal mode, toggle ANSI output, save default screen, and save preferred mesh channel.
- Modes include Normal, Quiet, Monitor, and Base Relay. Quiet mutes audio and dims the screen; Base Relay brightens the screen and uses verbose terminal output.
- Device name and passkey are saved in flash. Restart after editing them so BLE advertising and mesh authentication both start with the saved values.
- Emergency opens a confirmation flow. Press Enter to broadcast SOS or Backspace to cancel.

## Terminal

USB serial and BLE UART still expose the existing command interface:

```text
status
peers
messages
send 1
emergency
cancel
dump
logs
brightness 180
speaker mute
relay base
```

Useful V2 commands:

- `dump` exports settings, battery, heap, mesh counters, peers, and message counts.
- `logs` prints recent local messages with age and direction.
- `brightness <40-255>` saves display brightness.
- `speaker <on|off|mute>` saves speaker preference.
- `relay <normal|quiet|monitor|base>` saves field mode.
- `peername <mac-suffix> <name>` saves a local peer nickname after discovery.
- `trust <mac-suffix> <unknown|known|trusted>` saves a local peer trust label.
- `resetsettings` clears saved Cardputer settings and restarts.
- `name <newname>` and `passkey <6dig>` save identity/passkey to flash.

## Two-Device Flow

1. Flash two Cardputers with the same sketch and passkey.
2. Give each device a different name from Settings or `name <newname>`.
3. Open Peers and wait for the heartbeat to show the other node.
4. Send typed chat with Enter, or send quick presets with `Fn+1` through `Fn+4`.
5. Use `dump` over USB/BLE to inspect mesh counters and local logs.

# Starbeam Maintenance and Troubleshooting

Keep devices healthy and easy to recover in the field.

**Routine maintenance**
- Recharge power banks after each use.
- Check button response and OLED visibility.
- Inspect wiring for loose connections.
- Update firmware on a fixed schedule.

**Common issues**
- No peers listed
- OLED is blank
- Buttons do not register
- Messages are delayed or missing

**Quick fixes**
- Reboot and move closer to another node.
- Check I2C wiring and address 0x3C.
- Verify pullup resistors on input only pins.
- Reduce traffic and send shorter messages.

**Firmware checks**
- Confirm the board is `esp32:esp32:esp32`.
- Confirm required libraries are installed.
- Keep a known good build on a USB drive.

**Field repairs**
- Swap to a spare ESP32 if a node fails.
- Move a relay to close a gap in the mesh.
- Use USB serial when Bluetooth is unreliable.

**Log what happened**
- Note time, location, and symptoms.
- Record steps taken and results.
- Capture command output when possible.

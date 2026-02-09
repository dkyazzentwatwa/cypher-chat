# Starbeam Field Guide

A quick, practical guide for operating Starbeam devices in the field. Keep this with your kit.

**Who this is for**
Operators, volunteers, and responders who need to deploy the mesh fast and keep it running.

**Before you deploy**
- Charge power banks and verify cables.
- Label each device with a unique name and passkey.
- Set a primary hub device and one backup hub device.
- Confirm each device boots and shows the status screen.
- Pack spare buttons, resistors, and one extra ESP32.

**Deployment patterns**
- Hub and spokes: Keep one server/hub stationary and move clients outward.
- Leapfrog: Move one node at a time to extend range without breaking the chain.
- Perimeter ring: Place nodes to create overlapping coverage around a site.

**Range realities**
- ESP-NOW range is highly sensitive to walls, metal, and elevation.
- Assume 50 to 200 meters per hop in urban areas.
- Line of sight gives the best performance.

**Operating basics**
- Button 3 short press cycles OLED screens.
- Button 3 long press triggers emergency broadcast.
- Button 1 long press cancels emergency.
- Use the terminal to check status, peers, and mesh health.

**Status checks**
- Use `status` to confirm uptime and role.
- Use `peers` to confirm neighbor count and RSSI.
- Use `mesh` to confirm queue depth and last heartbeat.

**Messaging etiquette**
- Keep messages short and structured.
- Use plain language and avoid acronyms unless shared.
- Confirm critical messages by asking for an ACK.
- Use time stamps in messages when possible.

**Emergency broadcast rules**
- Use emergency broadcast only for life safety.
- Keep a 30 second window clear for emergency traffic.
- If GPS is enabled, confirm coordinates are included.

**If a node goes dark**
1. Confirm power and cable.
2. Reboot the node.
3. If it was the hub, promote a backup hub.
4. Move nodes to close gaps in the mesh.

**Quick troubleshooting**
- No peers: verify channel and distance.
- Garbled terminal output: use USB serial for diagnostics.
- OLED blank: check I2C wiring and address 0x3C.

**After action**
- Export logs if you used USB serial.
- Note any dead zones and adjust placement next time.
- Rotate batteries and recharge all power banks.

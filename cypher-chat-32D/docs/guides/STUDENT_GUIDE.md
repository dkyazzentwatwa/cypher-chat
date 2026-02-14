# cypher-chat Student Guide

Welcome to cypher-chat, a small mesh radio like network that runs on ESP32 boards. You will build a device, send messages, and learn how multi hop networks work when the internet is not available.

**What you are building**
- A mesh node that can relay messages.
- A small screen that shows status and history.
- A simple button interface for sending alerts.

**Key ideas**
- Mesh networks use multiple small nodes instead of one big tower.
- Each hop extends range but adds delay.
- Authentication helps keep messages trusted.

**Build steps**
1. Wire the OLED to SDA 21 and SCL 22.
2. Wire buttons to GPIO 39, 34, and 36 with 10k pullup resistors.
3. Connect USB to your computer.
4. Flash the firmware using Arduino CLI or Arduino IDE.
5. Open a serial terminal at 115200 baud.

**First boot**
- Choose a passkey if prompted.
- Confirm the device name on the screen.
- Make sure at least two devices are powered.

**Try this**
- Use `status` to see your role and uptime.
- Use `peers` to see nearby nodes.
- Use `mesh` to view the store and forward queue.
- Trigger an emergency broadcast and then cancel it.

**Vocabulary**
- TTL: how many hops a message can travel.
- Peer: another node that can relay your message.
- ACK: a message that confirms delivery.
- ESP-NOW: the wireless protocol used for mesh.

**Tips**
- Place devices in line of sight for best range.
- Keep messages short and clear.
- If the mesh breaks, move one node closer and retry.

**Reflection questions**
- What is the minimum number of nodes for a reliable mesh?
- What changes would improve range without changing hardware?
- How would you secure the network if it was used in public?

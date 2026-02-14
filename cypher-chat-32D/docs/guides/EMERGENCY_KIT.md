# cypher-chat Emergency Kit

A fast checklist for power down and power up situations.

**Grab and go kit**
- 2 to 5 cypher-chat nodes
- Power banks and USB cables
- Spare ESP32 and OLED
- Small screwdriver and jumper wire
- Printed quick card

**Power up sequence**
1. Power the hub device first.
2. Power the relay nodes next.
3. Power the mobile clients last.
4. Confirm peers list grows on each device.

**Emergency broadcast protocol**
- Confirm the incident and location.
- Use emergency broadcast only once per incident.
- Ask for an ACK from the nearest peer.
- Cancel when resolved.

**Fallback plan**
- If the hub fails, promote a backup hub.
- If a relay fails, move another node to close the gap.
- If no peers, reduce distance and retry.

**Quick checks**
- OLED shows status screen.
- At least one peer is visible.
- Button 3 long press triggers alert.
- Buzzer or LED feedback is active.

**Post incident**
- Recharge all power banks.
- Rotate passkeys.
- Document issues and fixes.

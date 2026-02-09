# Starbeam Operations Playbook

A structured guide for planning, staging, and running Starbeam at scale.

**Roles**
- Comms lead: owns passkeys and configuration.
- Field tech: handles hardware and placement.
- Runner: moves nodes to extend range.
- Scribe: logs incidents and updates.

**Pre deployment**
- Verify firmware version on all devices.
- Set unit names to a shared naming scheme.
- Set a shared passkey and record it securely.
- Run a 10 minute soak test on power banks.

**Deployment steps**
1. Start hub device and confirm ready status.
2. Place relays at strategic intervals.
3. Confirm each relay appears in `peers`.
4. Deploy mobile clients.
5. Send a test message across the mesh.

**Message discipline**
- Use a standardized prefix for message type.
- Confirm receipt for high priority messages.
- Keep emergency channel clear when active.

**Scaling tips**
- Use a star and chain hybrid pattern.
- Avoid placing nodes behind heavy concrete.
- Elevate nodes to reduce signal blockage.

**Failure modes**
- Peer timeout: check power and distance.
- Queue growth: reduce traffic or add relays.
- Emergency stuck: cancel from a nearby node.

**Shutdown**
- Announce network shutdown.
- Power down clients first, hub last.
- Archive logs and rotate passkeys.

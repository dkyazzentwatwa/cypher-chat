# cypher-chat Scenario Drills

Short exercises to build confidence and speed.

**Drill 1: Cold start**
- Goal: build a working mesh in 5 minutes.
- Steps: power hub, place relays, confirm peers, send a test.
- Success: message reaches a node two hops away.

**Drill 2: Hub failure**
- Goal: promote a backup hub in 2 minutes.
- Steps: power down hub, reboot backup as hub, recheck peers.
- Success: mesh returns with at least one relay.

**Drill 3: Range extension**
- Goal: extend coverage across a hallway or block.
- Steps: place relays, verify RSSI, send messages at each hop.
- Success: message reaches the farthest node with TTL remaining.

**Drill 4: Emergency traffic**
- Goal: use emergency broadcast and clear the channel.
- Steps: broadcast, confirm ACK, cancel, resume normal traffic.
- Success: only one emergency broadcast is used.

**Drill 5: Silent operation**
- Goal: use low visibility comms.
- Steps: reduce noise, use short messages, avoid names.
- Success: comms remain clear without alerts.

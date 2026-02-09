# Cypher Chat Teacher Pack

This pack supports a 2 to 6 hour workshop or a 1 to 2 week classroom unit.

**Learning goals**
- Explain why off grid mesh networks are useful.
- Wire and build a basic ESP32 node.
- Configure a passkey and verify message authentication.
- Use the CLI to inspect mesh state and message flow.
- Describe tradeoffs in range, latency, and reliability.

**Audience and prerequisites**
- Ages 14+ with basic electronics and programming exposure.
- No prior networking knowledge required.
- Recommended group size: 6 to 20.

**Materials per group of 3**
- 3x ESP32 DevKit C V4
- 3x SSD1306 OLED displays
- 9x 10k resistors
- 3x tactile buttons
- Breadboard and jumpers
- 3x USB cables
- 3x power banks

**Lesson outline**
1. Context and safety briefing, 15 minutes
2. Hardware build, 45 minutes
3. Firmware flash, 30 minutes
4. Mesh demo and message flow, 30 minutes
5. Scenario exercise and debrief, 30 minutes

**Assessment ideas**
- Explain TTL with a diagram.
- Identify the role of MeshManager and OutputManager.
- Demonstrate a working emergency broadcast.
- Write a short report on signal range tests.

**Safety notes**
- Use only 3.3V pins for sensors and buttons.
- Do not power external components from 5V unless rated.
- Avoid powering from computer USB during field tests.

**Suggested discussion prompts**
- When is mesh better than a single relay?
- What does authentication protect against?
- How would you improve resilience with limited power?

**Rubric snapshot**
- Build quality: wiring and stability.
- Mesh reliability: consistent peer discovery.
- Communication clarity: structured message format.
- Reflection: tradeoffs and lessons learned.

**Extensions**
- Add GPS and include coordinates in emergency messages.
- Use RGB LEDs to indicate routing health.
- Compare ESP-NOW with BLE range in the same space.

**Troubleshooting guidance**
- No compile: verify Arduino core and libraries.
- OLED blank: check address 0x3C and I2C pins 21 and 22.
- Buttons not responding: input only pins need external pullups.

**Handouts to print**
- Quick cards for terminal commands.
- Student worksheet for scenario planning.
- Field guide for deployment.

# Contributing to Starbeam

Thank you for your interest in contributing to Starbeam! This document provides guidelines for contributing to the project.

## 🚀 Quick Start

1. **Fork** the repository
2. **Clone** your fork:
   ```bash
   git clone https://github.com/yourusername/starbeam.git
   cd starbeam/Bleeper_32D
   ```
3. **Create** a feature branch:
   ```bash
   git checkout -b feature/your-feature-name
   ```
4. **Make** your changes
5. **Test** on actual hardware
6. **Commit** and push
7. **Open** a Pull Request

## 📋 Code Style

### C++ / Arduino

- **Indentation:** 2 spaces (no tabs)
- **Line Length:** Max 100 characters
- **Naming:**
  - Classes: `PascalCase` (e.g., `MeshManager`)
  - Functions: `camelCase` (e.g., `sendMessage`)
  - Constants: `UPPER_SNAKE_CASE` (e.g., `MESH_MAX_PEERS`)
  - Variables: `camelCase` (e.g., `peerCount`)

**Example:**
```cpp
class MeshManager {
public:
  void sendMessage(const char* msg);

private:
  static const int MAX_RETRIES = 3;
  int messageCount;
};
```

### Comments

- Use `//` for single-line comments
- Use `/* */` for multi-line block comments
- Document public APIs with brief descriptions
- Avoid obvious comments

```cpp
// Good
void updatePeerList();  // Refreshes peer discovery cache

// Avoid
int count = 0;  // Initialize count to zero (obvious)
```

## 🔧 Development Workflow

### 1. Setting Up Development Environment

**Required Tools:**
- Arduino CLI or Arduino IDE
- ESP32 board support (v3.3.6 or later)
- Required libraries (see README.md)

**Recommended Tools:**
- VS Code with Arduino extension
- `clang-format` for code formatting
- Serial monitor (screen, minicom, or Arduino Serial Monitor)

### 2. Building and Testing

**Compile:**
```bash
arduino-cli compile --fqbn esp32:esp32:esp32 Bleeper_32D.ino
```

**Upload to hardware:**
```bash
arduino-cli upload -p /dev/cu.SLAB_USBtoUART --fqbn esp32:esp32:esp32 Bleeper_32D.ino
```

**Test checklist:**
- [ ] Compiles without warnings
- [ ] Boots successfully on ESP32
- [ ] OLED display shows correct information
- [ ] Buttons respond correctly
- [ ] Mesh networking connects to peers
- [ ] Emergency broadcast works
- [ ] Bluetooth terminal access functional
- [ ] No memory leaks (check with `memory` command)

### 3. Documentation

**Update documentation when:**
- Adding new features
- Changing user-facing behavior
- Modifying hardware requirements
- Changing configuration options

**Documentation files to consider:**
- `README.md` - Main project overview
- `docs/SETUP_GUIDE.md` - Hardware assembly
- `docs/STARBEAM_HANDBOOK.md` - User guide
- `CLAUDE.md` - Developer architecture reference

## 🐛 Bug Reports

**Before submitting:**
1. Check existing [GitHub Issues](https://github.com/yourusername/starbeam/issues)
2. Verify you're running the latest firmware
3. Test with known-good hardware
4. Gather debug information (see below)

**Include in bug report:**
- **Description:** Clear summary of the issue
- **Steps to reproduce:** Numbered list
- **Expected behavior:** What should happen
- **Actual behavior:** What actually happens
- **Hardware:** ESP32 model, components used
- **Firmware version:** Run `version` command
- **Serial output:** Copy relevant terminal output
- **Configuration:** Any changes to `Config_Starbeam.h`

**Template:**
```markdown
## Bug Description
Brief summary of the issue

## Steps to Reproduce
1. Power on device
2. Run command `status`
3. Observe unexpected output

## Expected Behavior
Device should show connection status

## Actual Behavior
Display shows garbled text

## Environment
- **ESP32:** DevKit C V4 (38-pin)
- **Firmware:** v1.0.0
- **Configuration:** Default (MESH_ENABLED=true)
- **Serial Output:**
```
[paste output here]
```
```

## ✨ Feature Requests

**Before requesting:**
- Check [existing issues](https://github.com/yourusername/starbeam/issues) for duplicates
- Consider if it fits the project scope (emergency communication)
- Think about backwards compatibility

**Include in request:**
- **Use case:** Why is this needed?
- **Proposed solution:** How should it work?
- **Alternatives considered:** Other approaches
- **Impact:** Who benefits from this feature?

## 🔀 Pull Request Process

### 1. Before Submitting

- [ ] Create issue describing the change (for larger features)
- [ ] Fork and create feature branch
- [ ] Make focused, atomic commits
- [ ] Test on actual ESP32 hardware
- [ ] Update documentation
- [ ] Add yourself to contributors list (if first contribution)

### 2. Commit Messages

Follow conventional commit format:

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types:**
- `feat` - New feature
- `fix` - Bug fix
- `docs` - Documentation changes
- `style` - Code style (formatting, no logic change)
- `refactor` - Code restructuring
- `perf` - Performance improvement
- `test` - Adding tests
- `chore` - Build process, dependencies

**Examples:**
```
feat(mesh): add message encryption support

Implements AES-128 encryption for mesh messages using shared passkey.
- Add crypto functions to MeshManager
- Update message packet structure
- Maintain backwards compatibility via encryption flag

Closes #42
```

```
fix(ble): resolve garbled Bluetooth terminal output

Batch BLE writes instead of character-by-character transmission.
Increases chunk size from 20 to 64 bytes for better performance.

Fixes #38
```

### 3. Pull Request Template

**Title:** Short, descriptive summary (e.g., "Add GPS coordinates to emergency broadcasts")

**Description:**
```markdown
## Changes
- Added GPS integration to emergency broadcasts
- Updated Config_Starbeam.h with GPS pin definitions
- Modified emergency packet structure to include lat/long

## Testing
- [x] Tested with NEO-6M GPS module
- [x] Verified emergency broadcast includes location
- [x] Confirmed backwards compatibility (GPS disabled)

## Documentation
- [x] Updated README.md with GPS feature
- [x] Added GPS wiring to SETUP_GUIDE.md
- [x] Updated CLAUDE.md architecture notes

## Related Issues
Closes #25
```

### 4. Code Review

**Reviewers check for:**
- Code quality and style compliance
- Adequate testing on hardware
- Documentation updates
- No breaking changes (unless major version)
- Security considerations
- Memory usage impact

**Authors should:**
- Respond to feedback constructively
- Make requested changes promptly
- Squash commits before merge (if requested)

## 🏗️ Architecture Guidelines

### Manager Pattern

Starbeam uses a manager pattern for modular components:

```cpp
class SomeManager {
public:
  void begin();      // Initialize
  void update();     // Main loop tick
  void shutdown();   // Cleanup

private:
  // Implementation details
};
```

### Event-Driven Design

Use callbacks for asynchronous events:

```cpp
// Good: Callback pattern
meshMgr.onMessage([](const MeshPacket* packet) {
  handleIncomingMessage(packet);
});

// Avoid: Blocking/polling
while (!messageReceived) {
  delay(100);  // Blocks other operations
}
```

### Memory Management

ESP32 has limited memory. Be mindful:

```cpp
// Good: Stack allocation for small, fixed-size data
char buffer[32];

// Good: Static allocation for globals
static const char* ERROR_MSG = "Failed";

// Avoid: Unnecessary heap allocation
char* buffer = new char[32];  // Rarely needed

// Avoid: Large stack buffers
char hugeBuffer[10000];  // May cause stack overflow
```

### Thread Safety

MeshManager callbacks run in WiFi task context:

```cpp
// Protect shared state with FreeRTOS semaphores
if (xSemaphoreTake(stateSemaphore, portMAX_DELAY) == pdTRUE) {
  // Access shared state
  sharedVariable = newValue;
  xSemaphoreGive(stateSemaphore);
}
```

## 📦 Adding Dependencies

**Before adding a library:**
1. Check if functionality exists in Arduino/ESP32 core
2. Verify library is actively maintained
3. Consider flash/RAM impact
4. Check license compatibility (prefer MIT/Apache/BSD)

**Document in:**
- README.md (Installation section)
- CLAUDE.md (Dependencies section)
- This file (Development Workflow)

## 🔐 Security

**Report security vulnerabilities privately:**
- Email: [your-email@example.com]
- Do NOT open public issues for security bugs

**Security considerations:**
- Message authentication (HMAC-SHA256)
- Passkey strength (6 digits minimum)
- No hardcoded credentials
- Safe handling of user input
- Bounds checking on buffers

## 🧪 Testing

### Hardware Testing

**Required tests:**
1. **Mesh connectivity** - 2-3 devices, verify peer discovery
2. **Message relay** - 3 devices in line, test multi-hop
3. **Emergency broadcast** - Verify all peers receive
4. **Bluetooth terminal** - Test command execution
5. **Power cycle** - Devices survive restart

### Edge Cases

Test uncommon scenarios:
- Empty passkey
- Very long device names
- Rapid button presses
- Poor signal conditions (far apart)
- Many simultaneous peers (>10)

## 📜 License

By contributing, you agree that your contributions will be licensed under the MIT License.

## 🙏 Recognition

Contributors are recognized in:
- GitHub Contributors page
- Release notes for significant features
- Project documentation

## ❓ Questions?

- **General:** Open a [Discussion](https://github.com/yourusername/starbeam/discussions)
- **Bugs:** Create an [Issue](https://github.com/yourusername/starbeam/issues)
- **Chat:** Join our [Discord/Slack] (if available)

---

**Thank you for contributing to Starbeam!** 🛰️

Every contribution, no matter how small, helps build better emergency communication tools for everyone.

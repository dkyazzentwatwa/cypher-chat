#pragma once

#include "Config_Starbeam.h"

// Button events
enum ButtonEvent {
  BUTTON_NONE,
  BUTTON_PRESS,
  BUTTON_LONG_PRESS,
  BUTTON_DOUBLE_PRESS
};

// Button state tracking
struct ButtonState {
  bool lastState;
  unsigned long pressStartTime;
  unsigned long lastReleaseTime;
  int pressCount;
};

#define DEBOUNCE_TIME_MS 50
#define LONG_PRESS_TIME_MS 1000
#define DOUBLE_PRESS_WINDOW_MS 300

class ButtonManager {
public:
  ButtonManager();

  // Initialize button manager
  void begin();

  // Poll buttons and return event (call from main loop)
  ButtonEvent poll(int& buttonIndex);

private:
  ButtonState buttonStates[NUM_BUTTONS];

  // Check if button is currently pressed (active-low)
  bool isPressed(int buttonIndex);

  // Update state for a single button
  ButtonEvent updateButton(int buttonIndex);
};

extern ButtonManager buttonMgr;

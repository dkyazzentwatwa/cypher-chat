#include "ButtonManager.h"
#include "TerminalManager.h"

ButtonManager buttonMgr;
extern TerminalManager terminalMgr;

ButtonManager::ButtonManager() {
  memset(buttonStates, 0, sizeof(buttonStates));
}

void ButtonManager::begin() {
  // Initialize button states
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttonStates[i].lastState = HIGH; // Active-low, so HIGH = not pressed
    buttonStates[i].pressStartTime = 0;
    buttonStates[i].lastReleaseTime = 0;
    buttonStates[i].pressCount = 0;
  }
}

bool ButtonManager::isPressed(int buttonIndex) {
  return digitalRead(BUTTON_PINS[buttonIndex]) == LOW;
}

ButtonEvent ButtonManager::poll(int& buttonIndex) {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    ButtonEvent event = updateButton(i);
    if (event != BUTTON_NONE) {
      buttonIndex = i;

      // Log to terminal
      terminalMgr.logButtonPress(i, event);

      return event;
    }
  }

  buttonIndex = -1;
  return BUTTON_NONE;
}

ButtonEvent ButtonManager::updateButton(int buttonIndex) {
  ButtonState& state = buttonStates[buttonIndex];
  bool currentState = isPressed(buttonIndex);
  unsigned long now = millis();

  // Check for state change with debouncing
  if (currentState != state.lastState) {
    // Button state changed
    if (currentState) {
      // Button just pressed
      if (state.pressStartTime > 0 && (now - state.pressStartTime) < DEBOUNCE_TIME_MS) {
        // Too soon, ignore (bounce)
        return BUTTON_NONE;
      }

      state.pressStartTime = now;

      // Check for double-press
      if (state.pressCount > 0 && (now - state.lastReleaseTime) < DOUBLE_PRESS_WINDOW_MS) {
        state.pressCount = 0; // Reset for next double-press detection
        state.lastState = currentState;
        return BUTTON_DOUBLE_PRESS;
      }

      state.lastState = currentState;
      return BUTTON_NONE; // Wait to see if it's a long press

    } else {
      // Button just released
      unsigned long pressDuration = now - state.pressStartTime;

      state.lastState = currentState;
      state.lastReleaseTime = now;
      state.pressCount = 1;

      // Check if it was held long enough to be a press (debounce)
      if (pressDuration < DEBOUNCE_TIME_MS) {
        return BUTTON_NONE; // Too short, probably bounce
      }

      // Check if it was a long press
      if (pressDuration >= LONG_PRESS_TIME_MS) {
        state.pressCount = 0; // Don't trigger double-press after long press
        return BUTTON_LONG_PRESS;
      }

      // Regular press - but don't return yet, wait for possible double-press
      return BUTTON_NONE;
    }
  }

  // No state change - check for ongoing long press
  if (currentState && state.pressStartTime > 0) {
    unsigned long pressDuration = now - state.pressStartTime;
    if (pressDuration >= LONG_PRESS_TIME_MS) {
      // Mark as handled so we don't trigger multiple times
      state.pressStartTime = 0;
      state.pressCount = 0;
      return BUTTON_LONG_PRESS;
    }
  }

  // Check for single press timeout (double-press window expired)
  if (state.pressCount > 0 && (now - state.lastReleaseTime) >= DOUBLE_PRESS_WINDOW_MS) {
    state.pressCount = 0;
    return BUTTON_PRESS;
  }

  return BUTTON_NONE;
}

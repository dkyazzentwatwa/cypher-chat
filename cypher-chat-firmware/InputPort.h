#pragma once

#include "Config.h"
#include "TouchInput.h"

enum InputEventType {
  INPUT_NONE,
  INPUT_CHAR,
  INPUT_ENTER,
  INPUT_BACK,
  INPUT_UP,
  INPUT_DOWN,
  INPUT_PREV_SCREEN,
  INPUT_NEXT_SCREEN,
  INPUT_TAB,
  INPUT_MENU,
  INPUT_TAP,
  INPUT_SWIPE_LEFT,
  INPUT_SWIPE_RIGHT,
  INPUT_SWIPE_UP,
  INPUT_SWIPE_DOWN,
  INPUT_QUICK_1,
  INPUT_QUICK_2,
  INPUT_QUICK_3,
  INPUT_QUICK_4,
  INPUT_EMERGENCY
};

struct InputEvent {
  InputEventType type = INPUT_NONE;
  char ch = '\0';
  bool fn = false;
};

class InputPort {
public:
  bool begin();
  void update();
  bool nextEvent(InputEvent &event);
  bool hasTextInput() const;
  const char *label() const;

private:
  InputEvent _queue[16];
  uint8_t _head = 0;
  uint8_t _tail = 0;
  uint32_t _lastTouchMs = 0;
  uint32_t _touchStartMs = 0;
  uint32_t _lastTouchSampleMs = 0;
  uint16_t _touchStartX = 0;
  uint16_t _touchStartY = 0;
  uint16_t _touchLastX = 0;
  uint16_t _touchLastY = 0;
  bool _touchActive = false;
  bool _bootWasPressed = false;
  bool _buttonWasPressed[3] = {false, false, false};
  bool _buttonLongSent[3] = {false, false, false};
  uint32_t _buttonPressedAt[3] = {0, 0, 0};
  TouchInput _touch;

  void push(InputEventType type, char ch = '\0', bool fn = false);
  void updateCardputer();
  void updateTouch();
  void updateButtons();
};

extern InputPort inputPort;

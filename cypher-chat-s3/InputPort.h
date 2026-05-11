#pragma once

#include "Config_S3.h"
#include "TouchInput.h"

enum InputEventType {
  INPUT_NONE,
  INPUT_CHAR,
  INPUT_ENTER,
  INPUT_BACK,
  INPUT_UP,
  INPUT_DOWN,
  INPUT_TAB,
  INPUT_MENU,
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
  bool _bootWasPressed = false;
  TouchInput _touch;

  void push(InputEventType type, char ch = '\0', bool fn = false);
  void updateCardputer();
  void updateTouch();
};

extern InputPort inputPort;


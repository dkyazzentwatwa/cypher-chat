#include "InputPort.h"

#if BOARD_HAS_CARDPUTER_INPUT
#include <M5Cardputer.h>
#endif

InputPort inputPort;

bool InputPort::begin() {
#if BOARD_HAS_TOUCH_INPUT
  pinMode(PIN_BOOT_BUTTON, BOOT_BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  _bootWasPressed = digitalRead(PIN_BOOT_BUTTON) == (BOOT_BUTTON_ACTIVE_LOW ? LOW : HIGH);
  _touch.begin();
#endif
  return true;
}

void InputPort::update() {
#if BOARD_HAS_CARDPUTER_INPUT
  updateCardputer();
#endif
#if BOARD_HAS_TOUCH_INPUT
  updateTouch();
#endif
}

bool InputPort::nextEvent(InputEvent &event) {
  if (_head == _tail) {
    return false;
  }
  event = _queue[_tail];
  _tail = (_tail + 1) % (sizeof(_queue) / sizeof(_queue[0]));
  return true;
}

bool InputPort::hasTextInput() const {
  return BOARD_HAS_CARDPUTER_INPUT;
}

const char *InputPort::label() const {
#if BOARD_HAS_CARDPUTER_INPUT
  return "keyboard";
#elif BOARD_HAS_TOUCH_INPUT
  return "touch";
#else
  return "terminal";
#endif
}

void InputPort::push(InputEventType type, char ch, bool fn) {
  const uint8_t next = (_head + 1) % (sizeof(_queue) / sizeof(_queue[0]));
  if (next == _tail) {
    return;
  }
  _queue[_head].type = type;
  _queue[_head].ch = ch;
  _queue[_head].fn = fn;
  _head = next;
}

void InputPort::updateCardputer() {
#if BOARD_HAS_CARDPUTER_INPUT
  M5Cardputer.update();
  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
    return;
  }

  auto &keys = M5Cardputer.Keyboard.keysState();
  if (keys.enter) {
    push(INPUT_ENTER, '\n', keys.fn);
  }
  if (keys.del) {
    push(INPUT_BACK, '\b', keys.fn);
  }
  if (keys.tab) {
    push(INPUT_TAB, '\t', keys.fn);
  }

  if (keys.fn) {
    for (char c : keys.word) {
      if (c == '9') push(INPUT_UP, c, true);
      if (c == '5') push(INPUT_DOWN, c, true);
      if (c >= '1' && c <= '4') push(static_cast<InputEventType>(INPUT_QUICK_1 + (c - '1')), c, true);
      if (c == 'e' || c == 'E') push(INPUT_EMERGENCY, c, true);
    }
  }

  for (char c : keys.word) {
    if ((c == 'm' || c == 'M') && !keys.fn) {
      push(INPUT_MENU, c, false);
    }
    push(INPUT_CHAR, c, keys.fn);
  }
#endif
}

void InputPort::updateTouch() {
#if BOARD_HAS_TOUCH_INPUT
  const bool bootPressed = digitalRead(PIN_BOOT_BUTTON) == (BOOT_BUTTON_ACTIVE_LOW ? LOW : HIGH);
  if (bootPressed && !_bootWasPressed) {
    push(INPUT_BACK);
  }
  _bootWasPressed = bootPressed;

  TouchPoint points[1];
  uint8_t count = 0;
  if (!_touch.read(points, 1, count) || count == 0) {
    return;
  }

  const uint32_t now = millis();
  if (now - _lastTouchMs < 220) {
    return;
  }
  _lastTouchMs = now;

  const uint16_t x = points[0].x;
  const uint16_t y = points[0].y;
  const uint16_t w = LCD_WIDTH;
  const uint16_t h = LCD_HEIGHT;

  if (y < h / 5) {
    push(INPUT_UP);
  } else if (y > (h * 4) / 5) {
    push(INPUT_DOWN);
  } else if (x < w / 4) {
    push(INPUT_BACK);
  } else if (x > (w * 3) / 4) {
    push(INPUT_ENTER);
  } else {
    push(INPUT_MENU);
  }
#endif
}

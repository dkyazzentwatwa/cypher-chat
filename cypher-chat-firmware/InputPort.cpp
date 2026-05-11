#include "InputPort.h"

#if BOARD_HAS_CARDPUTER_INPUT
#include <M5Cardputer.h>
#endif

InputPort inputPort;

static constexpr uint32_t BUTTON_DEBOUNCE_MS = 35;
static constexpr uint32_t BUTTON_LONG_PRESS_MS = 1200;

#if BOARD_HAS_GPIO_BUTTONS
static constexpr int BUTTON_PINS[3] = {PIN_BUTTON_1, PIN_BUTTON_2, PIN_BUTTON_3};
#endif

bool InputPort::begin() {
#if BOARD_HAS_TOUCH_INPUT
  pinMode(PIN_BOOT_BUTTON, BOOT_BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  _bootWasPressed = digitalRead(PIN_BOOT_BUTTON) == (BOOT_BUTTON_ACTIVE_LOW ? LOW : HIGH);
  _touch.begin();
#endif
#if BOARD_HAS_GPIO_BUTTONS
  for (uint8_t i = 0; i < 3; i++) {
    pinMode(BUTTON_PINS[i], BUTTON_USE_INTERNAL_PULLUP ? INPUT_PULLUP : INPUT);
    _buttonWasPressed[i] = digitalRead(BUTTON_PINS[i]) == (BUTTON_ACTIVE_LOW ? LOW : HIGH);
    _buttonPressedAt[i] = _buttonWasPressed[i] ? millis() : 0;
    _buttonLongSent[i] = false;
  }
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
#if BOARD_HAS_GPIO_BUTTONS
  updateButtons();
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
#if BOARD_HAS_GPIO_BUTTONS
  return "3 buttons";
#else
  return "terminal";
#endif
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

void InputPort::updateButtons() {
#if BOARD_HAS_GPIO_BUTTONS
  const uint32_t now = millis();
  for (uint8_t i = 0; i < 3; i++) {
    const bool pressed = digitalRead(BUTTON_PINS[i]) == (BUTTON_ACTIVE_LOW ? LOW : HIGH);

    if (pressed && !_buttonWasPressed[i]) {
      _buttonPressedAt[i] = now;
      _buttonLongSent[i] = false;
    }

    if (pressed && !_buttonLongSent[i] && now - _buttonPressedAt[i] >= BUTTON_LONG_PRESS_MS) {
      if (i == 0) {
        push(INPUT_BACK);
      } else if (i == 2) {
        push(INPUT_EMERGENCY);
      }
      _buttonLongSent[i] = true;
    }

    if (!pressed && _buttonWasPressed[i]) {
      const uint32_t held = now - _buttonPressedAt[i];
      if (!_buttonLongSent[i] && held >= BUTTON_DEBOUNCE_MS) {
#if BOARD_PROFILE == BOARD_PROFILE_XIAO_ESP32C3_OLED_GPS_3BTN
        if (i == 0) {
          push(INPUT_PREV_SCREEN);
        } else if (i == 1) {
          push(INPUT_NEXT_SCREEN);
        } else {
          push(INPUT_ENTER);
        }
#else
        if (i == 0) {
          push(INPUT_UP);
        } else if (i == 1) {
          push(INPUT_DOWN);
        } else {
          push(INPUT_ENTER);
        }
#endif
      }
    }

    _buttonWasPressed[i] = pressed;
  }
#endif
}

void InputPort::updateTouch() {
#if BOARD_HAS_TOUCH_INPUT
  const bool bootPressed = digitalRead(PIN_BOOT_BUTTON) == (BOOT_BUTTON_ACTIVE_LOW ? LOW : HIGH);
  if (bootPressed && !_bootWasPressed) {
#if BOARD_IS_WAVESHARE_VISUAL_DISPLAY
    push(INPUT_NEXT_SCREEN);
#else
    push(INPUT_BACK);
#endif
  }
  _bootWasPressed = bootPressed;

  TouchPoint points[1];
  uint8_t count = 0;
  if (!_touch.read(points, 1, count) || count == 0) {
#if BOARD_IS_WAVESHARE_VISUAL_DISPLAY
    const uint32_t now = millis();
    if (_touchActive && now - _lastTouchSampleMs >= 70) {
      const int dx = static_cast<int>(_touchLastX) - static_cast<int>(_touchStartX);
      const int dy = static_cast<int>(_touchLastY) - static_cast<int>(_touchStartY);
      const int absDx = abs(dx);
      const int absDy = abs(dy);
      const uint32_t held = now - _touchStartMs;

      if (now - _lastTouchMs >= 180) {
        if (max(absDx, absDy) >= 55 && max(absDx, absDy) > min(absDx, absDy) + 20) {
          if (absDx > absDy) {
            push(dx < 0 ? INPUT_SWIPE_LEFT : INPUT_SWIPE_RIGHT);
          } else {
            push(dy < 0 ? INPUT_SWIPE_UP : INPUT_SWIPE_DOWN);
          }
        } else if (held <= 700 && absDx <= 35 && absDy <= 35) {
          push(INPUT_TAP);
        }
        _lastTouchMs = now;
      }
      _touchActive = false;
    }
#endif
    return;
  }

  const uint32_t now = millis();
#if BOARD_IS_WAVESHARE_VISUAL_DISPLAY
  const uint16_t x = points[0].x;
  const uint16_t y = points[0].y;
  if (!_touchActive) {
    _touchStartX = x;
    _touchStartY = y;
    _touchStartMs = now;
    _touchActive = true;
  }
  _touchLastX = x;
  _touchLastY = y;
  _lastTouchSampleMs = now;
  return;
#else
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
#endif
}

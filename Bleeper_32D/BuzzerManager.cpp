#include "BuzzerManager.h"

BuzzerManager buzzerMgr;

BuzzerManager::BuzzerManager()
  : currentPattern(BUZZ_NONE),
    patternStartTime(0),
    patternStep(0),
    isSounding(false),
    soundStartTime(0),
    soundDuration(0) {
}

void BuzzerManager::begin() {
#if BUZZER_PIN != -1
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#endif
}

void BuzzerManager::play(BuzzerPattern pattern) {
  if (pattern != currentPattern) {
    currentPattern = pattern;
    patternStartTime = millis();
    patternStep = 0;
    soundOff(); // Stop any current sound
  }
}

void BuzzerManager::stop() {
  currentPattern = BUZZ_NONE;
  patternStep = 0;
  soundOff();
}

void BuzzerManager::update() {
#if BUZZER_PIN != -1
  unsigned long now = millis();

  // Check if current sound should end
  if (isSounding && (now - soundStartTime >= soundDuration)) {
    soundOff();
  }

  // Update pattern state machine
  switch (currentPattern) {
    case BUZZ_NONE:
      // Nothing to do
      break;
    case BUZZ_BUTTON:
      updateButtonPattern();
      break;
    case BUZZ_MESSAGE_RX:
      updateMessageRxPattern();
      break;
    case BUZZ_CONNECTED:
      updateConnectedPattern();
      break;
    case BUZZ_ERROR:
      updateErrorPattern();
      break;
    case BUZZ_EMERGENCY:
      updateEmergencyPattern();
      break;
  }
#endif
}

void BuzzerManager::updateButtonPattern() {
  // Single short beep (50ms)
  if (patternStep == 0 && !isSounding) {
    soundOn(50);
    patternStep = 1;
  } else if (patternStep == 1 && !isSounding) {
    currentPattern = BUZZ_NONE;
    patternStep = 0;
  }
}

void BuzzerManager::updateMessageRxPattern() {
  unsigned long elapsed = millis() - patternStartTime;

  // Double beep: 50ms, 100ms gap, 50ms
  if (patternStep == 0 && !isSounding) {
    soundOn(50);
    patternStep = 1;
  } else if (patternStep == 1 && !isSounding && elapsed >= 150) {
    soundOn(50);
    patternStep = 2;
  } else if (patternStep == 2 && !isSounding) {
    currentPattern = BUZZ_NONE;
    patternStep = 0;
  }
}

void BuzzerManager::updateConnectedPattern() {
  // Single long beep (300ms)
  if (patternStep == 0 && !isSounding) {
    soundOn(300);
    patternStep = 1;
  } else if (patternStep == 1 && !isSounding) {
    currentPattern = BUZZ_NONE;
    patternStep = 0;
  }
}

void BuzzerManager::updateErrorPattern() {
  unsigned long elapsed = millis() - patternStartTime;

  // Three short beeps: 50ms, 100ms gap, 50ms, 100ms gap, 50ms
  if (patternStep == 0 && !isSounding) {
    soundOn(50);
    patternStep = 1;
  } else if (patternStep == 1 && !isSounding && elapsed >= 150) {
    soundOn(50);
    patternStep = 2;
  } else if (patternStep == 2 && !isSounding && elapsed >= 300) {
    soundOn(50);
    patternStep = 3;
  } else if (patternStep == 3 && !isSounding) {
    currentPattern = BUZZ_NONE;
    patternStep = 0;
  }
}

void BuzzerManager::updateEmergencyPattern() {
  unsigned long elapsed = millis() - patternStartTime;

  // Continuous beeping: 250ms on, 250ms off
  unsigned long phase = elapsed % 500;

  if (phase < 250 && !isSounding) {
    soundOn(250);
  } else if (phase >= 250 && isSounding) {
    soundOff();
  }
}

void BuzzerManager::soundOn(unsigned long duration) {
#if BUZZER_PIN != -1
  digitalWrite(BUZZER_PIN, HIGH);
  isSounding = true;
  soundStartTime = millis();
  soundDuration = duration;
#endif
}

void BuzzerManager::soundOff() {
#if BUZZER_PIN != -1
  digitalWrite(BUZZER_PIN, LOW);
  isSounding = false;
#endif
}

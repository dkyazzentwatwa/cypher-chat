#pragma once

#include "Config.h"

// Buzzer patterns
enum BuzzerPattern {
  BUZZ_NONE,
  BUZZ_BUTTON,        // Short beep (50ms)
  BUZZ_MESSAGE_RX,    // Double beep (50ms, 100ms gap, 50ms)
  BUZZ_CONNECTED,     // Long beep (300ms)
  BUZZ_ERROR,         // Three short beeps
  BUZZ_EMERGENCY      // Continuous urgent beeps (250ms on/off)
};

class BuzzerManager {
public:
  BuzzerManager();

  // Initialize buzzer manager
  void begin();

  // Play a pattern (non-blocking)
  void play(BuzzerPattern pattern);

  // Stop current pattern
  void stop();

  // Update buzzer state (call from main loop)
  void update();

private:
  BuzzerPattern currentPattern;
  unsigned long patternStartTime;
  int patternStep;

  bool isSounding;
  unsigned long soundStartTime;
  unsigned long soundDuration;

  // Pattern state machine
  void updateButtonPattern();
  void updateMessageRxPattern();
  void updateConnectedPattern();
  void updateErrorPattern();
  void updateEmergencyPattern();

  // Low-level control
  void soundOn(unsigned long duration);
  void soundOff();
};

extern BuzzerManager buzzerMgr;

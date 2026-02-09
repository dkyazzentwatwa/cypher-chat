#pragma once

#include "Config_Starbeam.h"
#include "StateManager.h"

// LED status patterns
enum LEDPattern {
  LED_IDLE,           // Blue dim
  LED_SCANNING,       // Yellow pulsing (smooth sine wave)
  LED_CONNECTING,     // Yellow blinking (250ms on/off)
  LED_CONNECTED,      // Green solid
  LED_MESSAGE_RX,     // Blue flash (200ms)
  LED_MESSAGE_TX,     // Cyan flash (200ms)
  LED_EMERGENCY,      // Red blinking
  LED_ERROR           // Red solid
};

// LED color constants for direct color control
enum LEDColor {
  LED_OFF,
  LED_RED,
  LED_GREEN,
  LED_BLUE,
  LED_YELLOW,
  LED_CYAN,
  LED_MAGENTA,
  LED_WHITE
};

// PWM channels for ESP32
#define PWM_CHANNEL_R 0
#define PWM_CHANNEL_G 1
#define PWM_CHANNEL_B 2

class LEDManager {
public:
  LEDManager();

  // Initialize LED manager
  void begin();

  // Set LED pattern based on connection state
  void setPattern(LEDPattern pattern);

  // Set LED to a specific color
  void set(LEDColor color);

  // Flash temporarily (for TX/RX indication)
  void flash(LEDPattern flashPattern, unsigned long duration = 200);

  // Flash a specific color temporarily
  void flash(LEDColor color, unsigned long duration = 200);

  // Update LED state (call from main loop)
  void update();

private:
  LEDPattern currentPattern;
  LEDPattern flashPattern;
  unsigned long flashStartTime;
  unsigned long flashDuration;
  bool isFlashing;

  unsigned long lastUpdateTime;
  unsigned long patternStartTime;

  // Current RGB values (0-255)
  uint8_t currentR;
  uint8_t currentG;
  uint8_t currentB;

  // Set RGB color directly
  void setColor(uint8_t r, uint8_t g, uint8_t b);

  // Pattern update functions
  void updateIdlePattern();
  void updateScanningPattern();
  void updateConnectingPattern();
  void updateConnectedPattern();
  void updateEmergencyPattern();
  void updateErrorPattern();

  // Helper to calculate pulsing brightness (sine wave)
  uint8_t calculatePulse(unsigned long period);
};

extern LEDManager ledMgr;

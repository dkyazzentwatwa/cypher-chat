#include "LEDManager.h"
#include <math.h>

LEDManager ledMgr;

LEDManager::LEDManager()
  : currentPattern(LED_IDLE),
    flashPattern(LED_IDLE),
    flashStartTime(0),
    flashDuration(0),
    isFlashing(false),
    lastUpdateTime(0),
    patternStartTime(0),
    currentR(0),
    currentG(0),
    currentB(0) {
}

void LEDManager::begin() {
#if LED_ENABLED
  // Attach pins with PWM configuration (newer ESP32 Arduino Core API)
  ledcAttach(LED_PIN_R, LED_PWM_FREQ, LED_PWM_RES);
  ledcAttach(LED_PIN_G, LED_PWM_FREQ, LED_PWM_RES);
  ledcAttach(LED_PIN_B, LED_PWM_FREQ, LED_PWM_RES);

  // Initialize to idle state
  setColor(0, 0, 20); // Blue dim
  patternStartTime = millis();
#endif
}

void LEDManager::setPattern(LEDPattern pattern) {
  if (pattern != currentPattern) {
    currentPattern = pattern;
    patternStartTime = millis();
  }
}

void LEDManager::flash(LEDPattern flashPat, unsigned long duration) {
  flashPattern = flashPat;
  flashDuration = duration;
  flashStartTime = millis();
  isFlashing = true;
}

void LEDManager::set(LEDColor color) {
#if LED_ENABLED
  switch (color) {
    case LED_OFF:     setColor(0, 0, 0); break;
    case LED_RED:     setColor(255, 0, 0); break;
    case LED_GREEN:   setColor(0, 255, 0); break;
    case LED_BLUE:    setColor(0, 0, 255); break;
    case LED_YELLOW:  setColor(255, 255, 0); break;
    case LED_CYAN:    setColor(0, 255, 255); break;
    case LED_MAGENTA: setColor(255, 0, 255); break;
    case LED_WHITE:   setColor(255, 255, 255); break;
  }
#endif
}

void LEDManager::flash(LEDColor color, unsigned long duration) {
#if LED_ENABLED
  set(color);
  flashDuration = duration;
  flashStartTime = millis();
  isFlashing = true;
#endif
}

void LEDManager::update() {
#if LED_ENABLED
  unsigned long now = millis();

  // Check if flash is active and should end
  if (isFlashing && (now - flashStartTime >= flashDuration)) {
    isFlashing = false;
  }

  // Update pattern every 20ms (50 FPS for smooth animations)
  if (now - lastUpdateTime < 20) {
    return;
  }
  lastUpdateTime = now;

  // Use flash pattern if active, otherwise use current pattern
  LEDPattern activePattern = isFlashing ? flashPattern : currentPattern;

  switch (activePattern) {
    case LED_IDLE:
      updateIdlePattern();
      break;
    case LED_SCANNING:
      updateScanningPattern();
      break;
    case LED_CONNECTING:
      updateConnectingPattern();
      break;
    case LED_CONNECTED:
      updateConnectedPattern();
      break;
    case LED_MESSAGE_RX:
      setColor(0, 0, 255); // Blue flash
      break;
    case LED_MESSAGE_TX:
      setColor(0, 255, 255); // Cyan flash
      break;
    case LED_EMERGENCY:
      updateEmergencyPattern();
      break;
    case LED_ERROR:
      updateErrorPattern();
      break;
  }
#endif
}

void LEDManager::setColor(uint8_t r, uint8_t g, uint8_t b) {
  currentR = r;
  currentG = g;
  currentB = b;

#if LED_ENABLED
  ledcWrite(LED_PIN_R, r);
  ledcWrite(LED_PIN_G, g);
  ledcWrite(LED_PIN_B, b);
#endif
}

void LEDManager::updateIdlePattern() {
  // Blue dim - static
  setColor(0, 0, 20);
}

void LEDManager::updateScanningPattern() {
  // Yellow pulsing (smooth sine wave, 2 second period)
  uint8_t brightness = calculatePulse(2000);
  setColor(brightness, brightness, 0); // Yellow
}

void LEDManager::updateConnectingPattern() {
  // Yellow blinking (250ms on/off)
  unsigned long elapsed = millis() - patternStartTime;
  bool on = (elapsed % 500) < 250;
  if (on) {
    setColor(255, 255, 0); // Yellow
  } else {
    setColor(0, 0, 0); // Off
  }
}

void LEDManager::updateConnectedPattern() {
  // Green solid
  setColor(0, 255, 0);
}

void LEDManager::updateEmergencyPattern() {
  // Red blinking (fast - 200ms on/off)
  unsigned long elapsed = millis() - patternStartTime;
  bool on = (elapsed % 400) < 200;
  if (on) {
    setColor(255, 0, 0); // Red
  } else {
    setColor(0, 0, 0); // Off
  }
}

void LEDManager::updateErrorPattern() {
  // Red solid
  setColor(255, 0, 0);
}

uint8_t LEDManager::calculatePulse(unsigned long period) {
  unsigned long elapsed = millis() - patternStartTime;
  float phase = (float)(elapsed % period) / period * 2.0 * PI;
  float brightness = (sin(phase) + 1.0) / 2.0; // 0.0 to 1.0
  return (uint8_t)(brightness * 200 + 55); // Range: 55-255 for visibility
}

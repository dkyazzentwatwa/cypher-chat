#pragma once

#include <Arduino.h>

/**
 * Connection states for mesh networking
 */
enum ConnectionState {
  STATE_IDLE,          // Ready
  STATE_SCANNING,      // Actively scanning
  STATE_CONNECTING,    // Connection in progress
  STATE_CONNECTED,     // Authenticated and ready
  STATE_DISCONNECTED,  // Lost connection, will retry
  STATE_ERROR          // Too many failures
};

/**
 * StateManager - Simple state management for ESP8266
 *
 * Simplified version without FreeRTOS mutexes (ESP8266 is single-core).
 * Tracks state duration, retry count, and implements exponential backoff.
 */
class StateManager {
private:
  ConnectionState currentState;
  unsigned long stateChangeTime;
  int retryCount;
  unsigned long lastRetryTime;

  // Exponential backoff parameters
  static const unsigned long BASE_DELAY_MS = 1000;
  static const unsigned long MAX_DELAY_MS = 30000;
  static const int MAX_BACKOFF_SHIFTS = 5;

public:
  StateManager();

  bool setState(ConnectionState newState);
  ConnectionState getState();
  unsigned long getStateTime();
  int getRetryCount();
  void incrementRetry();
  void resetRetry();
  unsigned long getBackoffDelay();
  bool isStateStale(unsigned long timeoutMs);
  void checkWatchdog();
  static const char* getStateName(ConnectionState state);
};

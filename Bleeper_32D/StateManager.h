#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * Connection states for BLE client
 */
enum ConnectionState {
  STATE_IDLE,          // Ready to scan or connect
  STATE_SCANNING,      // Actively scanning for server
  STATE_CONNECTING,    // Connection in progress
  STATE_CONNECTED,     // Authenticated and ready
  STATE_DISCONNECTED,  // Lost connection, will retry
  STATE_ERROR          // Too many failures, restarting
};

/**
 * StateManager - Thread-safe state management
 *
 * Replaces volatile flags with FreeRTOS semaphore-protected state.
 * Tracks state duration, retry count, and implements exponential backoff.
 */
class StateManager {
private:
  ConnectionState currentState;
  SemaphoreHandle_t stateMutex;
  unsigned long stateChangeTime;
  int retryCount;
  unsigned long lastRetryTime;

  // Exponential backoff parameters
  static const unsigned long BASE_DELAY_MS = 1000;   // 1 second base
  static const unsigned long MAX_DELAY_MS = 30000;   // 30 second max
  static const int MAX_BACKOFF_SHIFTS = 5;           // 2^5 = 32x multiplier max

public:
  StateManager();
  ~StateManager();

  /**
   * Set new connection state (thread-safe)
   * @param newState State to transition to
   * @return true if state change successful
   */
  bool setState(ConnectionState newState);

  /**
   * Get current connection state (thread-safe)
   * @return Current state
   */
  ConnectionState getState();

  /**
   * Get time since state changed (milliseconds)
   * @return Elapsed time in current state
   */
  unsigned long getStateTime();

  /**
   * Get current retry count
   * @return Number of retry attempts
   */
  int getRetryCount();

  /**
   * Increment retry counter
   */
  void incrementRetry();

  /**
   * Reset retry counter (call on successful connection)
   */
  void resetRetry();

  /**
   * Get exponential backoff delay for current retry count
   * @return Delay in milliseconds before next retry
   */
  unsigned long getBackoffDelay();

  /**
   * Check if current state has been active too long
   * @param timeoutMs Timeout threshold in milliseconds
   * @return true if state is stale
   */
  bool isStateStale(unsigned long timeoutMs);

  /**
   * Check for stale states and auto-transition if needed
   * Call this from main loop to implement watchdog
   */
  void checkWatchdog();

  /**
   * Get human-readable state name
   * @param state State to convert
   * @return State name string
   */
  static const char* getStateName(ConnectionState state);
};

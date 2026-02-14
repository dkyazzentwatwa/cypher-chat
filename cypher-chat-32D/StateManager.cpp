#include "StateManager.h"
#include "TerminalManager.h"

extern TerminalManager terminalMgr;

StateManager::StateManager() {
  currentState = STATE_IDLE;
  stateChangeTime = millis();
  retryCount = 0;
  lastRetryTime = 0;

  // Create mutex for thread-safe state access
  stateMutex = xSemaphoreCreateMutex();
  if (stateMutex == NULL) {
    Serial.println("StateManager: Failed to create mutex!");
  }
}

StateManager::~StateManager() {
  if (stateMutex != NULL) {
    vSemaphoreDelete(stateMutex);
  }
}

bool StateManager::setState(ConnectionState newState) {
  if (stateMutex == NULL) {
    return false;
  }

  // Acquire mutex with timeout
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ConnectionState oldState = currentState;
    currentState = newState;
    stateChangeTime = millis();

    xSemaphoreGive(stateMutex);

    // Log state transitions for debugging
    if (oldState != newState) {
      Serial.print("State: ");
      Serial.print(getStateName(oldState));
      Serial.print(" -> ");
      Serial.println(getStateName(newState));

      // Log to terminal
      terminalMgr.logConnection(newState);
    }

    return true;
  }

  return false;
}

ConnectionState StateManager::getState() {
  if (stateMutex == NULL) {
    return currentState;  // Unsafe fallback
  }

  ConnectionState state = STATE_IDLE;

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    state = currentState;
    xSemaphoreGive(stateMutex);
  }

  return state;
}

unsigned long StateManager::getStateTime() {
  return millis() - stateChangeTime;
}

int StateManager::getRetryCount() {
  if (stateMutex == NULL) {
    return retryCount;  // Unsafe fallback
  }

  int count = 0;

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    count = retryCount;
    xSemaphoreGive(stateMutex);
  }

  return count;
}

void StateManager::incrementRetry() {
  if (stateMutex == NULL) {
    return;
  }

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    retryCount++;
    lastRetryTime = millis();
    xSemaphoreGive(stateMutex);

    Serial.print("Retry count: ");
    Serial.println(retryCount);
  }
}

void StateManager::resetRetry() {
  if (stateMutex == NULL) {
    return;
  }

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (retryCount > 0) {
      Serial.println("Retry count reset (successful connection)");
    }
    retryCount = 0;
    lastRetryTime = 0;
    xSemaphoreGive(stateMutex);
  }
}

unsigned long StateManager::getBackoffDelay() {
  // Exponential backoff: 1s, 2s, 4s, 8s, 16s, 32s (capped at 30s)
  // delay = BASE_DELAY_MS * 2^(min(retryCount, MAX_BACKOFF_SHIFTS))

  int count = getRetryCount();
  int shifts = (count < MAX_BACKOFF_SHIFTS) ? count : MAX_BACKOFF_SHIFTS;

  unsigned long delay = BASE_DELAY_MS * (1 << shifts);  // 2^shifts

  // Cap at maximum
  return (delay < MAX_DELAY_MS) ? delay : MAX_DELAY_MS;
}

bool StateManager::isStateStale(unsigned long timeoutMs) {
  return getStateTime() > timeoutMs;
}

void StateManager::checkWatchdog() {
  ConnectionState state = getState();

  switch (state) {
    case STATE_SCANNING:
      // Scan should complete within 10 seconds
      if (isStateStale(10000)) {
        Serial.println("Watchdog: Scan timeout, returning to idle");
        setState(STATE_IDLE);
      }
      break;

    case STATE_CONNECTING:
      // Connection should complete within 15 seconds
      if (isStateStale(15000)) {
        Serial.println("Watchdog: Connect timeout, disconnected");
        setState(STATE_DISCONNECTED);
        incrementRetry();
      }
      break;

    case STATE_CONNECTED:
      // No timeout for connected state
      break;

    case STATE_IDLE:
    case STATE_DISCONNECTED:
    case STATE_ERROR:
      // These states don't have timeouts
      break;
  }
}

const char* StateManager::getStateName(ConnectionState state) {
  switch (state) {
    case STATE_IDLE:         return "IDLE";
    case STATE_SCANNING:     return "SCANNING";
    case STATE_CONNECTING:   return "CONNECTING";
    case STATE_CONNECTED:    return "CONNECTED";
    case STATE_DISCONNECTED: return "DISCONNECTED";
    case STATE_ERROR:        return "ERROR";
    default:                 return "UNKNOWN";
  }
}

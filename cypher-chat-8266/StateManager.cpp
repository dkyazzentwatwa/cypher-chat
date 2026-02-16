#include "StateManager.h"
#include "TerminalManager.h"

extern TerminalManager terminalMgr;

StateManager::StateManager() {
  currentState = STATE_IDLE;
  stateChangeTime = millis();
  retryCount = 0;
  lastRetryTime = 0;
}

bool StateManager::setState(ConnectionState newState) {
  ConnectionState oldState = currentState;
  currentState = newState;
  stateChangeTime = millis();

  if (oldState != newState) {
    Serial.print("State: ");
    Serial.print(getStateName(oldState));
    Serial.print(" -> ");
    Serial.println(getStateName(newState));
    terminalMgr.logConnection(newState);
  }

  return true;
}

ConnectionState StateManager::getState() {
  return currentState;
}

unsigned long StateManager::getStateTime() {
  return millis() - stateChangeTime;
}

int StateManager::getRetryCount() {
  return retryCount;
}

void StateManager::incrementRetry() {
  retryCount++;
  lastRetryTime = millis();
  Serial.print("Retry count: ");
  Serial.println(retryCount);
}

void StateManager::resetRetry() {
  if (retryCount > 0) {
    Serial.println("Retry count reset (successful connection)");
  }
  retryCount = 0;
  lastRetryTime = 0;
}

unsigned long StateManager::getBackoffDelay() {
  int shifts = (retryCount < MAX_BACKOFF_SHIFTS) ? retryCount : MAX_BACKOFF_SHIFTS;
  unsigned long d = BASE_DELAY_MS * (1 << shifts);
  return (d < MAX_DELAY_MS) ? d : MAX_DELAY_MS;
}

bool StateManager::isStateStale(unsigned long timeoutMs) {
  return getStateTime() > timeoutMs;
}

void StateManager::checkWatchdog() {
  switch (currentState) {
    case STATE_SCANNING:
      if (isStateStale(10000)) {
        Serial.println("Watchdog: Scan timeout, returning to idle");
        setState(STATE_IDLE);
      }
      break;

    case STATE_CONNECTING:
      if (isStateStale(15000)) {
        Serial.println("Watchdog: Connect timeout, disconnected");
        setState(STATE_DISCONNECTED);
        incrementRetry();
      }
      break;

    default:
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

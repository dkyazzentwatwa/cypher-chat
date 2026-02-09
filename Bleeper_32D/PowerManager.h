#pragma once

#include <Arduino.h>
#include "Config_Starbeam.h"
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <driver/adc.h>

/**
 * PowerManager - Battery monitoring and power management for off-grid operation
 *
 * Features:
 * - Battery voltage monitoring via ADC
 * - Battery percentage calculation
 * - Light sleep mode (wake on timer or GPIO)
 * - Deep sleep mode (wake on GPIO only)
 * - WiFi TX power adjustment for range/power trade-off
 * - Power consumption estimation
 */

// Power states
enum PowerState {
  POWER_ACTIVE,        // Normal operation
  POWER_LIGHT_SLEEP,   // CPU paused, peripherals active
  POWER_DEEP_SLEEP     // Everything off except RTC and wake circuit
};

// Battery status
struct BatteryStatus {
  float voltage;        // Battery voltage in volts
  uint8_t percent;      // Battery percentage (0-100%)
  bool charging;        // True if charging detected
  bool lowBattery;      // True if below warning threshold
};

class PowerManager {
public:
  PowerManager();

  /**
   * Initialize power management
   * @return true if successful
   */
  bool begin();

  /**
   * Update power readings - call periodically from loop()
   */
  void update();

  /**
   * Get current battery status
   * @return BatteryStatus struct with voltage, percentage, etc.
   */
  BatteryStatus getBatteryStatus();

  /**
   * Get battery voltage in volts
   * @return Voltage (e.g., 3.7V)
   */
  float getBatteryVoltage();

  /**
   * Get battery percentage (0-100%)
   * @return Percentage based on voltage curve
   */
  uint8_t getBatteryPercent();

  /**
   * Check if battery is low (below warning threshold)
   * @return true if low battery
   */
  bool isLowBattery();

  /**
   * Enter light sleep mode for specified duration
   * Wakes on timer or GPIO interrupt
   * @param seconds Duration in seconds
   */
  void enterLightSleep(uint32_t seconds);

  /**
   * Enter deep sleep mode
   * Only wakes on GPIO interrupt (button press)
   * CAUTION: Requires hardware reset or button press to wake
   */
  void enterDeepSleep();

  /**
   * Set WiFi transmission power
   * @param dBm Power level (0-20 dBm). Higher = longer range, more power
   * @return true if successful
   */
  bool setTXPower(int8_t dBm);

  /**
   * Get current WiFi TX power setting
   * @return Power in dBm (0-20)
   */
  int8_t getTXPower();

  /**
   * Get estimated power consumption
   * @return Estimated current draw in milliamps
   */
  uint32_t getEstimatedCurrentDraw();

  /**
   * Get power state
   * @return Current PowerState
   */
  PowerState getPowerState() { return _state; }

  /**
   * Get total uptime in milliseconds
   * @return Uptime since last boot
   */
  uint32_t getUptime() { return millis(); }

private:
  // Battery monitoring
  float readBatteryVoltage();
  uint8_t voltageToPercent(float voltage);
  float _lastVoltage;
  uint32_t _lastBatteryCheck;
  static const uint32_t BATTERY_CHECK_INTERVAL_MS = 10000; // Check every 10s

  // Power management
  PowerState _state;
  int8_t _txPower;

  // Wake up pin for deep sleep
  gpio_num_t _wakeupPin;
};

// Global instance
extern PowerManager powerMgr;

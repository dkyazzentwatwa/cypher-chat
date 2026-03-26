#include "PowerManager.h"
#include "OutputManager.h"

// Global instance
PowerManager powerMgr;

PowerManager::PowerManager()
  : _lastVoltage(0.0f)
  , _lastBatteryCheck(0)
  , _state(POWER_ACTIVE)
  , _txPower(POWER_TX_POWER_DEFAULT)
  , _wakeupPin((gpio_num_t)POWER_SLEEP_WAKEUP_PIN) {
}

bool PowerManager::begin() {
  if (!BATTERY_ENABLED && !POWER_MANAGEMENT_ENABLED) {
    return false;  // Power management disabled
  }

#if BATTERY_ENABLED
  // Configure ADC for battery monitoring
  adc1_config_width(ADC_WIDTH_BIT_12);  // 12-bit resolution (0-4095)
  adc1_config_channel_atten(BATTERY_ADC_CHANNEL, ADC_ATTEN_DB_11);  // 0-3.3V range

  // Initial battery reading
  _lastVoltage = readBatteryVoltage();
  _lastBatteryCheck = millis();
#endif

#if POWER_MANAGEMENT_ENABLED
  // Set default WiFi TX power
  esp_wifi_set_max_tx_power(_txPower * 4);  // API uses 0.25dBm units

  // Configure wake-up pin for deep sleep
  esp_sleep_enable_ext0_wakeup(_wakeupPin, 0);  // Wake on LOW (button press)
#endif

  return true;
}

void PowerManager::update() {
  // Periodic battery voltage check (every 10 seconds)
  if (millis() - _lastBatteryCheck >= BATTERY_CHECK_INTERVAL_MS) {
    _lastVoltage = readBatteryVoltage();
    _lastBatteryCheck = millis();
  }
}

BatteryStatus PowerManager::getBatteryStatus() {
  BatteryStatus status;

#if BATTERY_ENABLED
  status.voltage = _lastVoltage;
  status.percent = voltageToPercent(_lastVoltage);
  status.charging = false;  // TODO: Add charging detection if hardware supports it
  status.lowBattery = (_lastVoltage < BATTERY_MIN_VOLTAGE + 0.2f);  // 20% margin
#else
  status.voltage = 0.0f;
  status.percent = 100;  // Assume powered if no battery monitoring
  status.charging = false;
  status.lowBattery = false;
#endif

  return status;
}

float PowerManager::getBatteryVoltage() {
#if BATTERY_ENABLED
  return _lastVoltage;
#else
  return 0.0f;
#endif
}

uint8_t PowerManager::getBatteryPercent() {
#if BATTERY_ENABLED
  return voltageToPercent(_lastVoltage);
#else
  return 100;
#endif
}

bool PowerManager::isLowBattery() {
#if BATTERY_ENABLED
  return (_lastVoltage < BATTERY_MIN_VOLTAGE + 0.2f);
#else
  return false;
#endif
}

void PowerManager::enterLightSleep(uint32_t seconds) {
#if POWER_MANAGEMENT_ENABLED
  output.println("[POWER] Entering light sleep mode...");
  delay(100);  // Allow serial to flush

  // Configure timer wake-up
  esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);  // Convert to microseconds

  // Configure GPIO wake-up (button press)
  esp_sleep_enable_ext0_wakeup(_wakeupPin, 0);  // Wake on LOW

  // Enter light sleep
  _state = POWER_LIGHT_SLEEP;
  esp_light_sleep_start();

  // Wake up - restore state
  _state = POWER_ACTIVE;
  output.println("[POWER] Woke up from light sleep");
#else
  output.println("[POWER] Power management not enabled");
#endif
}

void PowerManager::enterDeepSleep() {
#if POWER_MANAGEMENT_ENABLED
  output.println("[POWER] Entering deep sleep mode...");
  output.println("[POWER] Press KEY1 button to wake");
  delay(100);  // Allow serial to flush

  // Configure GPIO wake-up (button press only - no timer)
  esp_sleep_enable_ext0_wakeup(_wakeupPin, 0);  // Wake on LOW

  // Enter deep sleep (will not return - device resets on wake)
  _state = POWER_DEEP_SLEEP;
  esp_deep_sleep_start();
#else
  output.println("[POWER] Power management not enabled");
#endif
}

bool PowerManager::setTXPower(int8_t dBm) {
#if POWER_MANAGEMENT_ENABLED
  // Clamp to valid range
  if (dBm < 0) dBm = 0;
  if (dBm > 20) dBm = 20;

  // Set WiFi TX power (API uses 0.25dBm units)
  esp_err_t err = esp_wifi_set_max_tx_power(dBm * 4);
  if (err == ESP_OK) {
    _txPower = dBm;
    return true;
  }
  return false;
#else
  return false;
#endif
}

int8_t PowerManager::getTXPower() {
  return _txPower;
}

uint32_t PowerManager::getEstimatedCurrentDraw() {
  // Rough estimates for ESP32 power consumption
  uint32_t current = 0;

  switch (_state) {
    case POWER_ACTIVE:
      current = 160;  // Active WiFi: ~160mA
      break;
    case POWER_LIGHT_SLEEP:
      current = 1;    // Light sleep: ~1mA
      break;
    case POWER_DEEP_SLEEP:
      current = 0;    // Deep sleep: ~10μA (effectively 0)
      break;
  }

  // Add TX power contribution (higher power = more current)
  current += (_txPower * 2);  // Rough estimate: +2mA per dBm

  return current;
}

// Private methods

float PowerManager::readBatteryVoltage() {
#if BATTERY_ENABLED
  // Take multiple samples and average
  uint32_t sum = 0;
  for (int i = 0; i < BATTERY_ADC_SAMPLES; i++) {
    sum += adc1_get_raw(BATTERY_ADC_CHANNEL);
    delay(1);
  }
  uint32_t adcValue = sum / BATTERY_ADC_SAMPLES;

  // Convert ADC reading to voltage
  // ADC range: 0-4095 (12-bit)
  // Voltage range: 0-3.3V (with 11dB attenuation)
  // Apply voltage divider ratio
  float voltage = (adcValue / 4095.0f) * 3.3f * BATTERY_DIVIDER_RATIO;

  return voltage;
#else
  return 0.0f;
#endif
}

uint8_t PowerManager::voltageToPercent(float voltage) {
  // LiPo battery discharge curve (approximate)
  // 4.2V = 100%
  // 3.7V = 50%
  // 3.0V = 0%

  if (voltage >= BATTERY_MAX_VOLTAGE) {
    return 100;
  }
  if (voltage <= BATTERY_MIN_VOLTAGE) {
    return 0;
  }

  // Linear interpolation (good enough for most applications)
  float range = BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE;
  float normalized = (voltage - BATTERY_MIN_VOLTAGE) / range;
  uint8_t percent = (uint8_t)(normalized * 100.0f);

  return percent;
}

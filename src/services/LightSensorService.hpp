#pragma once

#include <Arduino.h>
#include "app/AppConfig.hpp"

// LDR placeholder. With pin = -1 the service is invalid and BrightnessService
// falls back to the manual brightness. Calibration maps raw ADC to 0..100%.
class LightSensorService {
 public:
  LightSensorService();

  void begin(const LightConfig& config);
  void loop(uint32_t nowMs);

  bool valid() const { return valid_; }
  uint16_t raw() const { return raw_; }
  uint8_t percent() const { return percent_; }
  const LightConfig& config() const { return config_; }

 private:
  uint8_t mapToPercent(uint16_t raw) const;

  LightConfig config_;
  uint32_t lastSampleMs_;
  uint16_t raw_;
  uint8_t percent_;
  bool valid_;
  bool firstSample_;
};

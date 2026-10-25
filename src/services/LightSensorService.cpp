#include "LightSensorService.hpp"

#include <algorithm>
#include "util/Logger.hpp"

LightSensorService::LightSensorService()
    : config_(),
      lastSampleMs_(0),
      raw_(0),
      percent_(100),
      valid_(false),
      firstSample_(true) {}

void LightSensorService::begin(const LightConfig& config) {
  config_ = config;
  lastSampleMs_ = 0;
  raw_ = 0;
  percent_ = 100;
  valid_ = false;
  firstSample_ = true;

  if (config_.pin >= 0) {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    pinMode(static_cast<uint8_t>(config_.pin), INPUT);
    LOGI("ldr", "enabled on GPIO %d", config_.pin);
  } else {
    LOGI("ldr", "disabled (no pin configured)");
  }
}

void LightSensorService::loop(uint32_t nowMs) {
  if (config_.pin < 0 || !config_.enabled) return;

  const uint16_t interval = config_.sampleIntervalMs == 0 ? 500 : config_.sampleIntervalMs;
  if (nowMs - lastSampleMs_ < interval) return;
  lastSampleMs_ = nowMs;

  uint16_t samples[5];
  for (uint8_t i = 0; i < 5; ++i) {
    samples[i] = static_cast<uint16_t>(analogRead(static_cast<uint8_t>(config_.pin)));
  }
  std::sort(samples, samples + 5);
  raw_ = samples[2];

  const uint8_t target = mapToPercent(raw_);
  if (firstSample_) {
    percent_ = target;
    firstSample_ = false;
  } else {
    const uint8_t smoothing = config_.smoothing > 100 ? 100 : config_.smoothing;
    percent_ = static_cast<uint8_t>(
        (static_cast<uint16_t>(percent_) * smoothing +
         static_cast<uint16_t>(target) * (100u - smoothing)) /
        100u);
  }
  valid_ = true;
}

uint8_t LightSensorService::mapToPercent(uint16_t raw) const {
  uint16_t dark = config_.rawDark;
  uint16_t bright = config_.rawBright;
  if (dark == bright) return 0;

  if (dark > bright) {
    const uint16_t tmp = dark;
    dark = bright;
    bright = tmp;
  }

  if (raw <= dark) return config_.invert ? 100 : 0;
  if (raw >= bright) return config_.invert ? 0 : 100;

  const uint32_t value = ((static_cast<uint32_t>(raw - dark) * 100u) / (bright - dark));
  return static_cast<uint8_t>(config_.invert ? (100u - value) : value);
}

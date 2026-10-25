#include "PresenceService.hpp"

#include "util/Logger.hpp"

PresenceService::PresenceService()
    : config_(), lastMotionMs_(0), lastSampleMs_(0), presentState_(true), motion_(false), raw_(false) {}

void PresenceService::begin(const PresenceConfig& config) {
  config_ = config;
  lastMotionMs_ = millis();
  lastSampleMs_ = 0;
  presentState_ = true;
  motion_ = false;
  raw_ = false;

  if (config_.pin >= 0) {
    pinMode(static_cast<uint8_t>(config_.pin), config_.activeHigh ? INPUT : INPUT_PULLUP);
    LOGI("pir", "enabled on GPIO %d, active %s", config_.pin,
         config_.activeHigh ? "high" : "low");
  } else {
    LOGI("pir", "disabled (no pin configured)");
  }
}

void PresenceService::loop(uint32_t nowMs) {
  if (config_.pin < 0) {
    presentState_ = true;
    motion_ = false;
    raw_ = false;
    return;
  }

  const uint16_t debounce = config_.debounceMs == 0 ? 50 : config_.debounceMs;
  if (nowMs - lastSampleMs_ < debounce) return;
  lastSampleMs_ = nowMs;

  const int value = digitalRead(static_cast<uint8_t>(config_.pin));
  raw_ = value == HIGH;
  const bool active = config_.activeHigh ? raw_ : !raw_;

  if (active) {
    lastMotionMs_ = nowMs;
    presentState_ = true;
    motion_ = true;
  } else {
    motion_ = false;
    const uint32_t timeoutMs = static_cast<uint32_t>(config_.timeoutSec) * 1000u;
    if (nowMs - lastMotionMs_ >= timeoutMs) presentState_ = false;
  }
}

bool PresenceService::present() const {
  return config_.pin < 0 ? true : presentState_;
}

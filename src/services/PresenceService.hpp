#pragma once

#include <Arduino.h>
#include "app/AppConfig.hpp"

// PIR placeholder. With pin = -1 the service reports "present" so the display
// behaves normally. The policy lives in BrightnessService.
class PresenceService {
 public:
  PresenceService();

  void begin(const PresenceConfig& config);
  void loop(uint32_t nowMs);

  bool present() const;
  bool motion() const { return motion_; }
  bool raw() const { return raw_; }
  uint32_t lastMotionMs() const { return lastMotionMs_; }
  const PresenceConfig& config() const { return config_; }

 private:
  PresenceConfig config_;
  uint32_t lastMotionMs_;
  uint32_t lastSampleMs_;
  bool presentState_;
  bool motion_;
  bool raw_;
};

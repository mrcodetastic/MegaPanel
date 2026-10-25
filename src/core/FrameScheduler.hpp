#pragma once

#include <stdint.h>

// Fixed-deadline frame scheduler. It prevents drift by basing the next deadline
// on the previous one, and resynchronises when the loop falls behind.
class FrameScheduler {
 public:
  FrameScheduler();

  void begin(uint32_t nowMs, uint8_t maxFps = 60);
  bool due(uint32_t nowMs, uint8_t targetFps);

  uint32_t deltaMs() const { return deltaMs_; }
  uint32_t frameIndex() const { return frameIndex_; }
  uint32_t lastFrameMs() const { return lastFrameMs_; }
  uint32_t targetIntervalMs() const { return targetIntervalMs_; }

 private:
  uint32_t nextDeadlineMs_;
  uint32_t lastFrameMs_;
  uint32_t deltaMs_;
  uint32_t frameIndex_;
  uint32_t targetIntervalMs_;
  uint8_t maxFps_;
};

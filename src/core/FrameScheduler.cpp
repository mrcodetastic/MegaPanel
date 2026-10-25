#include "FrameScheduler.hpp"

FrameScheduler::FrameScheduler()
    : nextDeadlineMs_(0),
      lastFrameMs_(0),
      deltaMs_(0),
      frameIndex_(0),
      targetIntervalMs_(0),
      maxFps_(60) {}

void FrameScheduler::begin(uint32_t nowMs, uint8_t maxFps) {
  maxFps_ = maxFps == 0 ? 60 : maxFps;
  nextDeadlineMs_ = nowMs;
  lastFrameMs_ = nowMs;
  deltaMs_ = 0;
  frameIndex_ = 0;
  targetIntervalMs_ = 0;
}

bool FrameScheduler::due(uint32_t nowMs, uint8_t targetFps) {
  uint8_t fps = targetFps == 0 ? 1 : targetFps;
  if (fps > maxFps_) fps = maxFps_;
  uint32_t interval = 1000u / fps;
  if (interval == 0) interval = 1;
  targetIntervalMs_ = interval;

  if (static_cast<int32_t>(nowMs - nextDeadlineMs_) < 0) return false;

  deltaMs_ = static_cast<uint32_t>(nowMs - lastFrameMs_);
  if (deltaMs_ == 0) deltaMs_ = interval;
  lastFrameMs_ = nowMs;
  ++frameIndex_;

  nextDeadlineMs_ += interval;
  // If the loop stalled for more than one interval, resynchronise instead of
  // trying to catch up with a burst of frames.
  if (static_cast<int32_t>(nowMs - nextDeadlineMs_) > static_cast<int32_t>(interval)) {
    nextDeadlineMs_ = nowMs + interval;
  }
  return true;
}

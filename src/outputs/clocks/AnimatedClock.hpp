#pragma once

#include <ArduinoJson.h>

#include "core/IOutput.hpp"
#include "core/PixelCanvas.hpp"

// Shared lifecycle for the Phase 3 animated clocks.
//
// The base owns everything the genre has in common: the current hour/minute,
// which digit changed, the colon blink, the digit geometry derived from the
// real screen size, and the config schema. Each clock overrides updateClock()
// and drawClock() and keeps its own POD state, allocated in resetClock() and
// released in releaseClock() so start()/stop() stay the only allocation
// boundaries.
class AnimatedClock : public IOutput {
 public:
  bool configure(const ScreenSpec& screen, JsonObjectConst config) override;
  bool start(OutputContext& ctx) override;
  void stop() override;
  void render(PixelCanvas& target, const FrameContext& frame) override;
  void onConfigChanged(JsonObjectConst config) override;
  void writeConfig(JsonObject& out) const override;

  bool isAnimated() const override { return true; }
  uint8_t preferredFps() const override { return fps_; }

  static void commonSchema(JsonObject& out);

 protected:
  AnimatedClock();

  virtual void resetClock() = 0;
  virtual void releaseClock() = 0;
  virtual void updateClock(const FrameContext& frame, float dt) = 0;
  virtual void drawClock(PixelCanvas& target, const FrameContext& frame) = 0;

  uint8_t hour() const { return hour_; }
  uint8_t minute() const { return minute_; }
  bool haveTime() const { return haveTime_; }
  bool minuteChanged() const { return minuteChanged_; }
  // 0,1 = hour digits, 3,4 = minute digits, 255 = nothing changed.
  uint8_t changedIndex() const { return changedIndex_; }
  float secondsSinceChange() const { return secondsSinceChange_; }
  bool colonVisible(const FrameContext& frame) const;
  // 0..1 progress since the last minute change, clamped.
  float changeProgress(float durationSec) const;

  uint8_t digitSize() const { return digitSize_; }
  int16_t digitY() const { return digitY_; }
  int16_t digitX(uint8_t index) const { return digitX_[index]; }
  int16_t digitWidth() const { return static_cast<int16_t>(6 * digitSize_); }
  int16_t digitHeight() const { return static_cast<int16_t>(8 * digitSize_); }
  int16_t digitsWidth() const { return static_cast<int16_t>(30 * digitSize_); }
  char digitChar(uint8_t index) const;

  void drawDigits(PixelCanvas& target, bool plates, const int8_t* offsets) const;
  void drawDateRow(PixelCanvas& target, const FrameContext& frame, CRGB color) const;
  void drawSyncNotice(PixelCanvas& target, const FrameContext& frame) const;

  // Rows use '.' for transparent and '#' for the supplied solid colour. The
  // mapped variant resolves any other character through keys/colors.
  static void drawSolidSprite(PixelCanvas& target, int16_t x, int16_t y,
                              const char* const* rows, uint8_t rowCount, CRGB color);
  static void drawSprite(PixelCanvas& target, int16_t x, int16_t y,
                         const char* const* rows, uint8_t rowCount, const char* keys,
                         const CRGB* colors, uint8_t colorCount);

  ScreenSpec screen_;
  CRGB color_;
  uint8_t speed_;
  bool showDate_;
  bool plates_;
  bool colonBlink_;
  uint8_t fps_;
  uint8_t digitSize_;
  int16_t digitX_[5];
  int16_t digitY_;

  uint8_t hour_;
  uint8_t minute_;
  bool haveTime_;
  bool minuteChanged_;
  bool colonOn_;
  uint8_t changedIndex_;
  float secondsSinceChange_;
  bool started_;
};

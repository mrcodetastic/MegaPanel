#pragma once

#include <ArduinoJson.h>
#include "core/IOutput.hpp"
#include "core/PixelCanvas.hpp"

// Shared lifecycle and config handling for procedural background patterns.
class PatternOutput : public IOutput {
 public:
  bool configure(const ScreenSpec& screen, JsonObjectConst config) override;
  bool start(OutputContext& ctx) override;
  void stop() override;
  void render(PixelCanvas& target, const FrameContext& frame) override;
  void buildConfigSchema(JsonObject& out) const override = 0;
  void writeConfig(JsonObject& out) const override;
  void onConfigChanged(JsonObjectConst config) override;

  static void commonSchema(JsonObject& out);

 protected:
  PatternOutput();

  virtual void resetPattern() {}
  virtual void releasePattern() {}
  virtual void drawPattern(PixelCanvas& target, const FrameContext& frame) = 0;

  ScreenSpec screen_;
  CRGB color_;
  uint8_t speed_;
  bool started_;
  bool firstFrame_;
};

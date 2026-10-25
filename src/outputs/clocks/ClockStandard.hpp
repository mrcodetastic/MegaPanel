#pragma once

#include <ArduinoJson.h>
#include "core/IOutput.hpp"
#include "core/PixelCanvas.hpp"

// First real output: a resolution-aware HH:MM clock with an optional date row.
// It is statically instantiated and allocates nothing until start().
class ClockStandard final : public IOutput {
 public:
  ClockStandard();

  const OutputMeta& meta() const override { return metaStatic(); }
  static const OutputMeta& metaStatic();
  static void schemaStatic(JsonObject& out);

  bool configure(const ScreenSpec& screen, JsonObjectConst config) override;
  bool start(OutputContext& ctx) override;
  void stop() override;
  void render(PixelCanvas& target, const FrameContext& frame) override;

  void buildConfigSchema(JsonObject& out) const override { schemaStatic(out); }
  void writeConfig(JsonObject& out) const override;

 private:
  ScreenSpec screen_;
  CRGB color_;
  bool showDate_;
  bool colonBlink_;
  bool started_;
};

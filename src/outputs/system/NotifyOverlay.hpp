#pragma once

#include <ArduinoJson.h>
#include "core/IOutput.hpp"
#include "core/PixelCanvas.hpp"

// System-layer scrolling notification banner.
class NotifyOverlay final : public IOutput {
 public:
  NotifyOverlay();

  const OutputMeta& meta() const override { return metaStatic(); }
  static const OutputMeta& metaStatic();
  static void schemaStatic(JsonObject& out) { (void)out; }

  bool configure(const ScreenSpec& screen, JsonObjectConst config) override;
  bool start(OutputContext& ctx) override;
  void stop() override;
  void render(PixelCanvas& target, const FrameContext& frame) override;

  void buildConfigSchema(JsonObject& out) const override { schemaStatic(out); }
  void writeConfig(JsonObject& out) const override { (void)out; }

 private:
  ScreenSpec screen_;
  bool started_;
};

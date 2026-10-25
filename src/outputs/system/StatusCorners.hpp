#pragma once

#include <ArduinoJson.h>
#include "core/IOutput.hpp"
#include "core/PixelCanvas.hpp"

// System layer overlay: a small WiFi status mark in the top-right corner.
// It never owns the foreground and can be enabled or disabled at runtime.
class StatusCorners final : public IOutput {
 public:
  StatusCorners();

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
  OutputContext* context_;
  bool started_;
};

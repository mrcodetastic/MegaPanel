#pragma once

#include <ArduinoJson.h>

#include "core/IOutput.hpp"
#include "core/PixelCanvas.hpp"

// Reusable asset display: the asset name across the top, the price large in
// the middle, and the movement (absolute or percent) in red, green or grey at
// the bottom. It works for crypto, equities, FX or anything else the
// DataService can map to a price.
class AssetOutput final : public IOutput {
 public:
  AssetOutput();

  const OutputMeta& meta() const override { return metaStatic(); }
  static const OutputMeta& metaStatic();
  static void schemaStatic(JsonObject& out);

  bool configure(const ScreenSpec& screen, JsonObjectConst config) override;
  bool start(OutputContext& ctx) override;
  void stop() override;
  void render(PixelCanvas& target, const FrameContext& frame) override;
  void onConfigChanged(JsonObjectConst config) override;

  void buildConfigSchema(JsonObject& out) const override { schemaStatic(out); }
  void writeConfig(JsonObject& out) const override;

  uint8_t preferredFps() const override { return fps_; }

 private:
  void applyConfig(JsonObjectConst config);

  ScreenSpec screen_;
  class DataService* data_;
  uint8_t source_;
  char label_[20];
  char prefix_[8];
  int8_t decimals_;
  bool showPercent_;
  bool thousands_;
  CRGB upColor_;
  CRGB downColor_;
  CRGB flatColor_;
  CRGB nameColor_;
  uint8_t fps_;
  bool started_;
};

#pragma once

#include <array>
#include <memory>
#include "core/ClockLayout.hpp"
#include "core/IOutput.hpp"
#include "core/PixelCanvas.hpp"
#include "glyph/GlyphMorphEngine.hpp"
#include "glyph/IGlyphSource.hpp"

// Font-agnostic morphing digit clock. Glyphs are reduced to lit points; digit
// changes interpolate between the old and new point sets.
class ClockMorphDigits final : public IOutput {
 public:
  ClockMorphDigits();

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

 private:
  void applyLiveConfig(JsonObjectConst config);
  bool buildGlyphs();
  void drawColon(PixelCanvas& target, const FrameContext& frame) const;

  ScreenSpec screen_;
  ClockLayout layout_;
  CRGB color_;
  uint16_t morphMs_;
  bool reverse_;
  bool showColon_;
  bool started_;
  bool firstFrame_;
  char fontName_[20];

  std::unique_ptr<IGlyphSource> source_;
  std::array<GlyphPoints, 11> glyphs_;  // 0-9 and colon
  uint8_t current_[4];
  uint8_t target_[4];
  uint32_t morphStart_[4];
  bool morphing_[4];
  GlyphMorphEngine engine_;
};

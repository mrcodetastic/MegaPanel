#pragma once

#include <ArduinoJson.h>
#include "core/IOutput.hpp"
#include "core/PixelCanvas.hpp"

class DataService;
class NewsService;

// Scrolling text output. With a data source selected, the text is rebuilt from
// {name}, {price}, {change} and {percent} tokens; with `news` enabled, the
// NewsService headlines are joined instead; otherwise it shows the fixed text.
class TickerOutput final : public IOutput {
 public:
  TickerOutput();

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
  void applyConfig(JsonObjectConst config);
  void buildText(char* out, size_t capacity) const;
  void buildNewsText(char* out, size_t capacity) const;

  ScreenSpec screen_;
  CRGB color_;
  uint8_t speed_;
  bool started_;
  char text_[129];
  int8_t source_;
  char template_[129];
  bool newsMode_;
  class DataService* data_;
  class NewsService* news_;
  float scrollX_;
};

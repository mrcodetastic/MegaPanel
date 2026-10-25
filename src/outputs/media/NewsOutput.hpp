#pragma once

#include <ArduinoJson.h>

#include "core/IOutput.hpp"
#include "core/PixelCanvas.hpp"

struct NewsFeed;

// Vertical news ticker. Headlines from NewsService are word-wrapped to the
// panel width and scrolled upward pixel by pixel, so long titles stay read-
// able instead of being clipped. A small header row is optional.
class NewsOutput final : public IOutput {
 public:
  NewsOutput();
  ~NewsOutput() override;

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
  struct Line {
    char text[32];
    bool itemStart;
  };

  static constexpr uint8_t kMaxLines = 48;

  void applyConfig(JsonObjectConst config);
  void rebuildLines(const NewsFeed& feed);
  void appendWrapped(const char* text, uint8_t maxChars, bool firstLine);
  void drawHeader(PixelCanvas& target) const;

  ScreenSpec screen_;
  class NewsService* news_;
  CRGB color_;
  CRGB headerColor_;
  uint8_t size_;
  uint8_t speed_;
  uint8_t gap_;
  uint8_t fps_;
  char header_[20];
  bool bullet_;
  bool started_;

  Line* lines_;
  uint8_t lineCount_;
  uint8_t lineHeight_;
  uint16_t totalHeight_;
  uint32_t builtFor_;
  float offset_;
};

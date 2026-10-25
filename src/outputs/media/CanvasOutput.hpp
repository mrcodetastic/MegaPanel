#pragma once

#include <ArduinoJson.h>

#include "core/IOutput.hpp"
#include "core/PixelCanvas.hpp"

// JSON sprite-scene player. The scene lives in /canvas/<name>.json:
//
// {
//   "frames": [
//     { "ms": 500, "sprites": [
//         {"type":"rows","x":0,"y":0,"color":"#FF0000","rows":["..#..","..#.."]},
//         {"type":"text","x":0,"y":8,"text":"HI","size":2,"color":"#FFFFFF"},
//         {"type":"rect","x":2,"y":2,"w":4,"h":2,"color":"#00FF00"},
//         {"type":"circle","x":10,"y":10,"r":3,"color":"#0000FF"}
//     ]}
//   ]
// }
//
// This is a documented subset of the clockwise2 Canvas idea: frames of
// positioned primitives, loaded from LittleFS and scaled by the layout itself.
// Nothing is allocated per frame.
class CanvasOutput final : public IOutput {
 public:
  CanvasOutput();
  ~CanvasOutput() override;

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
  bool loadScene();
  void drawFrame(PixelCanvas& target, JsonObjectConst frame) const;

  ScreenSpec screen_;
  char fileName_[40];
  bool loop_;
  uint8_t fps_;
  bool started_;
  bool loaded_;
  JsonDocument* document_;
  uint8_t frameIndex_;
  uint32_t frameElapsedMs_;
};

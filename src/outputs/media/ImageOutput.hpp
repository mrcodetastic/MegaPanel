#pragma once

#include <ArduinoJson.h>
#include <FS.h>
#include <memory>
#include "core/IOutput.hpp"
#include "core/PixelCanvas.hpp"

// Simple 24-bit BMP image output. Images live in /images on LittleFS.
class ImageOutput final : public IOutput {
 public:
  ImageOutput();
  ~ImageOutput() override;

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
  bool loadBmp();
  void drawImage(PixelCanvas& target) const;

  ScreenSpec screen_;
  OutputContext* context_;
  char fileName_[48];
  bool stretch_;
  bool started_;
  bool loaded_;
  uint16_t imageWidth_;
  uint16_t imageHeight_;
  std::unique_ptr<uint8_t[]> pixels_;
};

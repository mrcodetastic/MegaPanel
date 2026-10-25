#pragma once

#include <ArduinoJson.h>
#include <FS.h>
#include <memory>
#include "core/IOutput.hpp"
#include "core/PixelCanvas.hpp"

// PCA1 animation player. Supports the legacy 128x64 4bpp format and scales it
// to the configured screen.
class PcaOutput : public IOutput {
 public:
  PcaOutput();
  ~PcaOutput() override;

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
  bool openFile();
  bool readFrame(uint16_t index);
  void drawFrame(PixelCanvas& target) const;

  ScreenSpec screen_;
  OutputContext* context_;
  char fileName_[40];
  bool started_;
  bool loaded_;
  File file_;
  uint16_t frameCount_;
  uint8_t paletteLen_;
  uint16_t palette_[16];
  std::unique_ptr<uint16_t[]> delays_;
  std::unique_ptr<uint8_t[]> frameBuffer_;
  uint16_t frameIndex_;
  uint32_t lastAdvanceMs_;
  uint32_t frameOffset_;
};

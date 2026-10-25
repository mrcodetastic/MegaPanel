#include "PcaOutput.hpp"

#include <LittleFS.h>
#include <cstring>
#include "core/FrameContext.hpp"
#include "core/OutputContext.hpp"
#include "util/Logger.hpp"

namespace {
constexpr uint16_t kFrameBytes = 4096;
constexpr uint16_t kSourceW = 128;
constexpr uint16_t kSourceH = 64;
}  // namespace

PcaOutput::PcaOutput()
    : screen_{0, 0},
      context_(nullptr),
      started_(false),
      loaded_(false),
      frameCount_(0),
      paletteLen_(0),
      frameIndex_(0),
      lastAdvanceMs_(0),
      frameOffset_(0) {
  fileName_[0] = '\0';
  memset(palette_, 0, sizeof(palette_));
}

PcaOutput::~PcaOutput() { stop(); }

const OutputMeta& PcaOutput::metaStatic() {
  static const OutputMeta meta = {
      "media.pca",
      "PCA animation",
      "1.0",
      "MegaPanel",
      OutputCategory::BackgroundPattern,
      LAYER_BG | LAYER_FG,
      LayerId::Background,
      ScreenSpec{128, 64},
      ScreenSpec{16, 16},
      0,
      0,
      30,
      20,
      true,
      false,
      false,
      false,
      true,
      false};
  return meta;
}

void PcaOutput::schemaStatic(JsonObject& out) {
  JsonObject file = out["file"].to<JsonObject>();
  file["type"] = "string";
  file["default"] = "";
  file["hint"] = "Basename in /anim, with or without .pca";
}

bool PcaOutput::configure(const ScreenSpec& screen, JsonObjectConst config) {
  screen_ = screen;
  const char* file = config["file"] | "";
  strncpy(fileName_, file, sizeof(fileName_) - 1);
  fileName_[sizeof(fileName_) - 1] = '\0';
  return screen_.isValid();
}

bool PcaOutput::start(OutputContext& ctx) {
  context_ = &ctx;
  stop();
  started_ = true;
  loaded_ = openFile();
  if (!loaded_) LOGW("pca", "no animation loaded for '%s'", fileName_);
  return true;
}

void PcaOutput::stop() {
  if (file_) file_.close();
  delays_.reset();
  frameBuffer_.reset();
  loaded_ = false;
  started_ = false;
  frameCount_ = 0;
  paletteLen_ = 0;
  frameIndex_ = 0;
}

void PcaOutput::onConfigChanged(JsonObjectConst config) {
  if (!context_) return;
  const char* file = config["file"] | "";
  if (strncmp(fileName_, file, sizeof(fileName_)) == 0) return;
  stop();
  configure(screen_, config);
  start(*context_);
}

bool PcaOutput::openFile() {
  if (!fileName_[0]) return false;

  String path = "/anim/";
  path += fileName_;
  if (!path.endsWith(".pca")) path += ".pca";

  file_ = LittleFS.open(path, "r");
  if (!file_) {
    LOGE("pca", "cannot open %s", path.c_str());
    return false;
  }

  uint8_t header[12];
  if (file_.read(header, sizeof(header)) != sizeof(header)) return false;
  if (memcmp(header, "PCA1", 4) != 0) {
    LOGE("pca", "bad magic in %s", path.c_str());
    return false;
  }

  frameCount_ = static_cast<uint16_t>(header[4] | (header[5] << 8));
  paletteLen_ = header[8];
  if (frameCount_ == 0 || frameCount_ > 360 || paletteLen_ == 0 || paletteLen_ > 16) {
    LOGE("pca", "invalid header in %s", path.c_str());
    return false;
  }

  if (file_.read(reinterpret_cast<uint8_t*>(palette_), paletteLen_ * 2) !=
      static_cast<size_t>(paletteLen_ * 2)) {
    return false;
  }

  delays_.reset(new uint16_t[frameCount_]);
  if (file_.read(reinterpret_cast<uint8_t*>(delays_.get()), frameCount_ * 2) !=
      static_cast<size_t>(frameCount_ * 2)) {
    return false;
  }
  for (uint16_t i = 0; i < frameCount_; ++i) {
    if (delays_[i] < 34) delays_[i] = 34;
    delays_[i] = static_cast<uint16_t>(delays_[i] | (delays_[i] << 8));  // no-op for clarity
  }

  frameOffset_ = 12 + static_cast<uint32_t>(paletteLen_) * 2 +
                 static_cast<uint32_t>(frameCount_) * 2;
  frameBuffer_.reset(new uint8_t[kFrameBytes]);
  if (!frameBuffer_) return false;

  frameIndex_ = 0;
  lastAdvanceMs_ = millis();
  return readFrame(0);
}

bool PcaOutput::readFrame(uint16_t index) {
  if (!file_ || !frameBuffer_ || index >= frameCount_) return false;
  if (!file_.seek(frameOffset_ + static_cast<uint32_t>(index) * kFrameBytes)) return false;
  return file_.read(frameBuffer_.get(), kFrameBytes) == kFrameBytes;
}

void PcaOutput::drawFrame(PixelCanvas& target) const {
  for (uint16_t y = 0; y < screen_.height; ++y) {
    const uint16_t srcY = static_cast<uint16_t>(static_cast<uint32_t>(y) * kSourceH / screen_.height);
    const uint32_t row = static_cast<uint32_t>(srcY) * (kSourceW / 2);
    for (uint16_t x = 0; x < screen_.width; ++x) {
      const uint16_t srcX = static_cast<uint16_t>(static_cast<uint32_t>(x) * kSourceW / screen_.width);
      const uint8_t packed = frameBuffer_[row + (srcX >> 1)];
      const uint8_t index = (srcX & 1) ? (packed & 0x0F) : (packed >> 4);
      const uint16_t color = palette_[index < paletteLen_ ? index : 0];
      target.drawPixel(x, y, target.color565_to_CRGB(color));
    }
  }
}

void PcaOutput::render(PixelCanvas& target, const FrameContext& frame) {
  if (!started_) return;
  target.clear(CRGB(0, 0, 0));

  if (!loaded_ || !frameBuffer_) {
    target.drawTextCentered("NO PCA", static_cast<int16_t>(screen_.height / 2 - 4), CRGB(180, 180, 180), 1);
    return;
  }

  const uint16_t delay = delays_[frameIndex_];
  if (frame.nowMs - lastAdvanceMs_ >= delay) {
    const uint16_t next = static_cast<uint16_t>((frameIndex_ + 1) % frameCount_);
    if (readFrame(next)) frameIndex_ = next;
    lastAdvanceMs_ = frame.nowMs;
  }

  drawFrame(target);
}

void PcaOutput::writeConfig(JsonObject& out) const {
  out["file"] = fileName_;
}

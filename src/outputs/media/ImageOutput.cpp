#include "ImageOutput.hpp"

#include <LittleFS.h>
#include <cstring>
#include "core/FrameContext.hpp"
#include "core/OutputContext.hpp"
#include "util/Logger.hpp"

ImageOutput::ImageOutput()
    : screen_{0, 0},
      context_(nullptr),
      stretch_(true),
      started_(false),
      loaded_(false),
      imageWidth_(0),
      imageHeight_(0) {
  fileName_[0] = '\0';
}

ImageOutput::~ImageOutput() { stop(); }

const OutputMeta& ImageOutput::metaStatic() {
  static const OutputMeta meta = {
      "media.image",
      "BMP image",
      "1.0",
      "MegaPanel",
      OutputCategory::DisplayOutput,
      LAYER_FG,
      LayerId::Foreground,
      ScreenSpec{0, 0},
      ScreenSpec{16, 16},
      0,
      0,
      2,
      20,
      false,
      false,
      false,
      false,
      true,
      false};
  return meta;
}

void ImageOutput::schemaStatic(JsonObject& out) {
  JsonObject file = out["file"].to<JsonObject>();
  file["type"] = "string";
  file["default"] = "";
  file["hint"] = "BMP basename in /images";

  JsonObject stretch = out["stretch"].to<JsonObject>();
  stretch["type"] = "bool";
  stretch["default"] = true;
}

bool ImageOutput::configure(const ScreenSpec& screen, JsonObjectConst config) {
  screen_ = screen;
  const char* file = config["file"] | "";
  strncpy(fileName_, file, sizeof(fileName_) - 1);
  fileName_[sizeof(fileName_) - 1] = '\0';
  stretch_ = config["stretch"] | true;
  return screen_.isValid();
}

bool ImageOutput::start(OutputContext& ctx) {
  context_ = &ctx;
  stop();
  started_ = true;
  loaded_ = loadBmp();
  if (!loaded_) LOGW("image", "no image loaded for '%s'", fileName_);
  return true;
}

void ImageOutput::stop() {
  pixels_.reset();
  loaded_ = false;
  started_ = false;
  imageWidth_ = 0;
  imageHeight_ = 0;
}

void ImageOutput::onConfigChanged(JsonObjectConst config) {
  if (!context_) return;
  const char* file = config["file"] | "";
  const bool stretch = config["stretch"] | true;
  if (stretch == stretch_ && strncmp(fileName_, file, sizeof(fileName_)) == 0) return;
  stop();
  configure(screen_, config);
  start(*context_);
}

bool ImageOutput::loadBmp() {
  if (!fileName_[0]) return false;

  String path = "/images/";
  path += fileName_;
  File file = LittleFS.open(path, "r");
  if (!file) {
    LOGE("image", "cannot open %s", path.c_str());
    return false;
  }

  uint8_t header[54];
  if (file.read(header, sizeof(header)) != sizeof(header)) return false;
  if (header[0] != 'B' || header[1] != 'M') return false;

  const uint32_t dataOffset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
  const int32_t width = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
  const int32_t rawHeight = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
  const uint16_t bpp = header[28] | (header[29] << 8);
  const uint32_t compression = header[30] | (header[31] << 8) | (header[32] << 16) | (header[33] << 24);

  if (bpp != 24 || compression != 0 || width <= 0 || rawHeight == 0 ||
      width > 256 || abs(rawHeight) > 256) {
    LOGE("image", "unsupported BMP %dx%d %ubpp", width, rawHeight, bpp);
    return false;
  }

  const int32_t height = rawHeight < 0 ? -rawHeight : rawHeight;
  const bool bottomUp = rawHeight > 0;
  const uint32_t rowSize = ((static_cast<uint32_t>(width) * 3 + 3) / 4) * 4;

  pixels_.reset(new (std::nothrow) uint8_t[static_cast<size_t>(width) * height * 3]);
  std::unique_ptr<uint8_t[]> row(new (std::nothrow) uint8_t[rowSize]);
  if (!pixels_ || !row) return false;

  if (!file.seek(dataOffset)) return false;
  for (int32_t y = 0; y < height; ++y) {
    if (file.read(row.get(), rowSize) != rowSize) return false;
    const int32_t dstY = bottomUp ? height - 1 - y : y;
    memcpy(pixels_.get() + static_cast<size_t>(dstY) * width * 3, row.get(), width * 3);
  }

  imageWidth_ = static_cast<uint16_t>(width);
  imageHeight_ = static_cast<uint16_t>(height);
  loaded_ = true;
  return true;
}

void ImageOutput::drawImage(PixelCanvas& target) const {
  if (!pixels_ || imageWidth_ == 0 || imageHeight_ == 0) return;

  int16_t offsetX = 0;
  int16_t offsetY = 0;
  int16_t drawW = screen_.width;
  int16_t drawH = screen_.height;
  float scale = 1.0f;

  if (!stretch_) {
    const float scaleX = static_cast<float>(screen_.width) / imageWidth_;
    const float scaleY = static_cast<float>(screen_.height) / imageHeight_;
    scale = scaleX < scaleY ? scaleX : scaleY;
    drawW = static_cast<int16_t>(imageWidth_ * scale);
    drawH = static_cast<int16_t>(imageHeight_ * scale);
    offsetX = static_cast<int16_t>((screen_.width - drawW) / 2);
    offsetY = static_cast<int16_t>((screen_.height - drawH) / 2);
  }

  for (int16_t y = 0; y < drawH; ++y) {
    const uint16_t srcY = static_cast<uint16_t>(
        stretch_ ? (static_cast<uint32_t>(y) * imageHeight_ / screen_.height)
                 : (static_cast<uint32_t>(y) / scale));
    if (srcY >= imageHeight_) continue;
    for (int16_t x = 0; x < drawW; ++x) {
      const uint16_t srcX = static_cast<uint16_t>(
          stretch_ ? (static_cast<uint32_t>(x) * imageWidth_ / screen_.width)
                   : (static_cast<uint32_t>(x) / scale));
      if (srcX >= imageWidth_) continue;
      const uint8_t* pixel = pixels_.get() +
                             (static_cast<size_t>(srcY) * imageWidth_ + srcX) * 3;
      target.drawPixel(static_cast<int16_t>(x + offsetX), static_cast<int16_t>(y + offsetY),
                       CRGB(pixel[2], pixel[1], pixel[0]));
    }
  }
}

void ImageOutput::render(PixelCanvas& target, const FrameContext& frame) {
  (void)frame;
  if (!started_) return;
  target.clear(CRGB(0, 0, 0));
  if (!loaded_) {
    target.drawTextCentered("NO IMG", static_cast<int16_t>(screen_.height / 2 - 4),
                            CRGB(180, 180, 180), 1);
    return;
  }
  drawImage(target);
}

void ImageOutput::writeConfig(JsonObject& out) const {
  out["file"] = fileName_;
  out["stretch"] = stretch_;
}

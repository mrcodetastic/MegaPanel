#include "CanvasOutput.hpp"

#include <LittleFS.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "core/FrameContext.hpp"
#include "util/Logger.hpp"

namespace {

constexpr size_t kMaxSceneBytes = 24576;

CRGB parseHexColor(const char* text, CRGB fallback) {
  if (!text) return fallback;
  if (*text == '#') ++text;
  if (strlen(text) != 6) return fallback;
  char* end = nullptr;
  const uint32_t value = static_cast<uint32_t>(strtoul(text, &end, 16));
  if (!end || *end != '\0') return fallback;
  return CRGB(static_cast<uint8_t>((value >> 16) & 0xFF),
              static_cast<uint8_t>((value >> 8) & 0xFF),
              static_cast<uint8_t>(value & 0xFF));
}

void copyText(char* destination, size_t capacity, const char* source) {
  if (!destination || capacity == 0) return;
  if (!source) {
    destination[0] = '\0';
    return;
  }
  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

void drawCircleFilled(PixelCanvas& target, int16_t cx, int16_t cy, int16_t radius, CRGB color) {
  if (radius <= 0) {
    target.drawPixel(cx, cy, color);
    return;
  }
  for (int16_t dy = -radius; dy <= radius; ++dy) {
    const int16_t span = static_cast<int16_t>(sqrtf(static_cast<float>(radius * radius - dy * dy)));
    target.fillRect(static_cast<int16_t>(cx - span), static_cast<int16_t>(cy + dy),
                    static_cast<int16_t>(span * 2 + 1), 1, color);
  }
}

}  // namespace

CanvasOutput::CanvasOutput()
    : screen_{0, 0},
      loop_(true),
      fps_(15),
      started_(false),
      loaded_(false),
      document_(nullptr),
      frameIndex_(0),
      frameElapsedMs_(0) {
  fileName_[0] = '\0';
}

CanvasOutput::~CanvasOutput() { stop(); }

const OutputMeta& CanvasOutput::metaStatic() {
  static const OutputMeta meta = {
      "media.canvas",
      "Canvas scene",
      "1.0",
      "JSON sprite scenes in /canvas, after clockwise2 Clockface7_Canvas",
      OutputCategory::DisplayOutput,
      LAYER_FG,
      LayerId::Foreground,
      ScreenSpec{64, 64},
      ScreenSpec{32, 32},
      0,
      0,
      15,
      20,
      true,
      false,
      false,
      false,
      true,
      false};
  return meta;
}

void CanvasOutput::schemaStatic(JsonObject& out) {
  JsonObject file = out["file"].to<JsonObject>();
  file["type"] = "string";
  file["default"] = "";
  file["hint"] = "Basename in /canvas, with or without .json";

  JsonObject loop = out["loop"].to<JsonObject>();
  loop["type"] = "bool";
  loop["default"] = true;

  JsonObject fps = out["fps"].to<JsonObject>();
  fps["type"] = "range";
  fps["min"] = 2;
  fps["max"] = 30;
  fps["default"] = 15;
}

bool CanvasOutput::configure(const ScreenSpec& screen, JsonObjectConst config) {
  screen_ = screen;
  copyText(fileName_, sizeof(fileName_), config["file"] | "");
  loop_ = config["loop"] | true;
  const uint8_t fps = static_cast<uint8_t>(config["fps"] | 15);
  fps_ = fps < 2 ? 2 : (fps > 30 ? 30 : fps);
  return screen_.isValid();
}

void CanvasOutput::onConfigChanged(JsonObjectConst config) {
  const char* file = config["file"] | "";
  loop_ = config["loop"] | loop_;
  const uint8_t fps = static_cast<uint8_t>(config["fps"] | fps_);
  fps_ = fps < 2 ? 2 : (fps > 30 ? 30 : fps);
  if (strncmp(fileName_, file, sizeof(fileName_)) != 0) {
    copyText(fileName_, sizeof(fileName_), file);
    if (started_) loadScene();
  }
}

bool CanvasOutput::start(OutputContext& ctx) {
  (void)ctx;
  started_ = true;
  frameIndex_ = 0;
  frameElapsedMs_ = 0;
  loaded_ = loadScene();
  if (!loaded_) LOGW("canvas", "no scene loaded for '%s'", fileName_);
  return true;
}

void CanvasOutput::stop() {
  delete document_;
  document_ = nullptr;
  loaded_ = false;
  started_ = false;
  frameIndex_ = 0;
  frameElapsedMs_ = 0;
}

bool CanvasOutput::loadScene() {
  delete document_;
  document_ = nullptr;
  loaded_ = false;
  if (!fileName_[0]) return false;

  String path = "/canvas/";
  path += fileName_;
  if (!path.endsWith(".json")) path += ".json";

  File file = LittleFS.open(path, "r");
  if (!file) {
    LOGE("canvas", "cannot open %s", path.c_str());
    return false;
  }
  const size_t size = file.size();
  if (size == 0 || size > kMaxSceneBytes) {
    LOGE("canvas", "%s has unsupported size %u", path.c_str(), static_cast<unsigned>(size));
    file.close();
    return false;
  }

  String payload;
  payload.reserve(size + 1);
  while (file.available()) {
    payload += static_cast<char>(file.read());
  }
  file.close();

  JsonDocument* document = new (std::nothrow) JsonDocument();
  if (!document) return false;

  const DeserializationError error =
      deserializeJson(*document, payload, DeserializationOption::NestingLimit(8));
  if (error) {
    LOGE("canvas", "json parse failed in %s: %s", path.c_str(), error.c_str());
    delete document;
    return false;
  }

  if (!(*document)["frames"].is<JsonArrayConst>() ||
      (*document)["frames"].as<JsonArrayConst>().size() == 0) {
    LOGE("canvas", "%s has no frames", path.c_str());
    delete document;
    return false;
  }

  document_ = document;
  loaded_ = true;
  LOGI("canvas", "loaded %s (%u frames)", path.c_str(),
       static_cast<unsigned>((*document)["frames"].as<JsonArrayConst>().size()));
  return true;
}

void CanvasOutput::drawFrame(PixelCanvas& target, JsonObjectConst frame) const {
  JsonArrayConst sprites = frame["sprites"];
  if (sprites.isNull()) return;

  for (JsonObjectConst sprite : sprites) {
    const char* type = sprite["type"] | "rows";
    const int16_t x = sprite["x"] | 0;
    const int16_t y = sprite["y"] | 0;

    if (strcmp(type, "rows") == 0) {
      const CRGB color = parseHexColor(sprite["color"] | "#FFFFFF", CRGB(255, 255, 255));
      JsonArrayConst rows = sprite["rows"];
      if (rows.isNull()) continue;
      int16_t row = 0;
      for (JsonVariantConst line : rows) {
        const char* text = line.as<const char*>();
        if (!text) {
          ++row;
          continue;
        }
        for (int16_t col = 0; text[col]; ++col) {
          if (text[col] != '.' && text[col] != ' ') {
            target.drawPixel(static_cast<int16_t>(x + col), static_cast<int16_t>(y + row), color);
          }
        }
        ++row;
      }
    } else if (strcmp(type, "text") == 0) {
      const CRGB color = parseHexColor(sprite["color"] | "#FFFFFF", CRGB(255, 255, 255));
      const char* text = sprite["text"] | "";
      const uint8_t size = static_cast<uint8_t>(sprite["size"] | 1);
      target.setTextSize(size < 1 ? 1 : (size > 5 ? 5 : size));
      target.setTextColor(color);
      target.setCursor(x, y);
      target.print(text);
    } else if (strcmp(type, "rect") == 0) {
      const CRGB color = parseHexColor(sprite["color"] | "#FFFFFF", CRGB(255, 255, 255));
      target.fillRect(x, y, static_cast<int16_t>(sprite["w"] | 1),
                      static_cast<int16_t>(sprite["h"] | 1), color);
    } else if (strcmp(type, "circle") == 0) {
      const CRGB color = parseHexColor(sprite["color"] | "#FFFFFF", CRGB(255, 255, 255));
      drawCircleFilled(target, x, y, static_cast<int16_t>(sprite["r"] | 1), color);
    }
  }
}

void CanvasOutput::render(PixelCanvas& target, const FrameContext& frame) {
  if (!started_) return;
  target.clear(CRGB(0, 0, 0));

  if (!loaded_ || !document_) {
    target.drawTextCentered("NO CANVAS", static_cast<int16_t>(screen_.height / 2 - 4),
                            CRGB(170, 170, 170), 1);
    return;
  }

  JsonArrayConst frames = (*document_)["frames"].as<JsonArrayConst>();
  const size_t frameCount = frames.size();
  if (frameCount == 0) return;
  if (frameIndex_ >= frameCount) frameIndex_ = 0;

  JsonObjectConst current = frames[frameIndex_].as<JsonObjectConst>();
  const uint32_t duration = current["ms"] | 250;

  frameElapsedMs_ += frame.deltaMs;
  if (frameElapsedMs_ >= duration) {
    if (loop_ || frameIndex_ + 1 < frameCount) {
      frameElapsedMs_ = 0;
      frameIndex_ = static_cast<uint8_t>((frameIndex_ + 1) % frameCount);
      current = frames[frameIndex_].as<JsonObjectConst>();
    } else {
      frameElapsedMs_ = duration;
    }
  }

  drawFrame(target, current);
}

void CanvasOutput::writeConfig(JsonObject& out) const {
  out["file"] = fileName_;
  out["loop"] = loop_;
  out["fps"] = fps_;
}

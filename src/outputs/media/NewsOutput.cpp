#include "NewsOutput.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "core/FrameContext.hpp"
#include "core/OutputContext.hpp"
#include "services/NewsService.hpp"

namespace {

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

void colorToHex(CRGB color, char* out, size_t capacity) {
  snprintf(out, capacity, "#%02X%02X%02X", color.r, color.g, color.b);
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

}  // namespace

NewsOutput::NewsOutput()
    : screen_{0, 0},
      news_(nullptr),
      color_(CRGB(255, 255, 255)),
      headerColor_(CRGB(110, 200, 255)),
      size_(1),
      speed_(12),
      gap_(4),
      fps_(20),
      bullet_(true),
      started_(false),
      lines_(nullptr),
      lineCount_(0),
      lineHeight_(12),
      totalHeight_(0),
      builtFor_(0),
      offset_(0.0f) {
  copyText(header_, sizeof(header_), "NEWS");
}

NewsOutput::~NewsOutput() { stop(); }

const OutputMeta& NewsOutput::metaStatic() {
  static const OutputMeta meta = {
      "media.news",
      "News ticker",
      "1.0",
      "Vertical news ticker over NewsService, after RetroPanel's ticker feed",
      OutputCategory::DisplayOutput,
      LAYER_FG,
      LayerId::Foreground,
      ScreenSpec{0, 0},
      ScreenSpec{32, 32},
      0,
      0,
      20,
      20,
      true,
      false,
      true,
      false,
      false,
      false};
  return meta;
}

void NewsOutput::schemaStatic(JsonObject& out) {
  JsonObject header = out["header"].to<JsonObject>();
  header["type"] = "string";
  header["default"] = "NEWS";
  header["hint"] = "Empty hides the header row";

  JsonObject color = out["color"].to<JsonObject>();
  color["type"] = "color";
  color["default"] = "#FFFFFF";

  JsonObject headerColor = out["headerColor"].to<JsonObject>();
  headerColor["type"] = "color";
  headerColor["default"] = "#6EC8FF";

  JsonObject speed = out["speed"].to<JsonObject>();
  speed["type"] = "range";
  speed["min"] = 2;
  speed["max"] = 60;
  speed["step"] = 1;
  speed["default"] = 12;
  speed["hint"] = "Scroll speed in pixels per second";

  JsonObject size = out["size"].to<JsonObject>();
  size["type"] = "range";
  size["min"] = 1;
  size["max"] = 2;
  size["default"] = 1;

  JsonObject gap = out["gap"].to<JsonObject>();
  gap["type"] = "range";
  gap["min"] = 2;
  gap["max"] = 12;
  gap["default"] = 4;

  JsonObject bullet = out["bullet"].to<JsonObject>();
  bullet["type"] = "bool";
  bullet["default"] = true;

  JsonObject fps = out["fps"].to<JsonObject>();
  fps["type"] = "range";
  fps["min"] = 5;
  fps["max"] = 30;
  fps["default"] = 20;
}

void NewsOutput::applyConfig(JsonObjectConst config) {
  copyText(header_, sizeof(header_), config["header"] | "NEWS");
  color_ = parseHexColor(config["color"] | "#FFFFFF", color_);
  headerColor_ = parseHexColor(config["headerColor"] | "#6EC8FF", headerColor_);

  const uint8_t speed = static_cast<uint8_t>(config["speed"] | 12);
  speed_ = speed < 2 ? 2 : (speed > 60 ? 60 : speed);

  const uint8_t size = static_cast<uint8_t>(config["size"] | 1);
  size_ = size < 1 ? 1 : (size > 2 ? 2 : size);

  const uint8_t gap = static_cast<uint8_t>(config["gap"] | 4);
  gap_ = gap < 2 ? 2 : (gap > 12 ? 12 : gap);

  const uint8_t fps = static_cast<uint8_t>(config["fps"] | 20);
  fps_ = fps < 5 ? 5 : (fps > 30 ? 30 : fps);

  bullet_ = config["bullet"] | true;
  builtFor_ = 0;
}

bool NewsOutput::configure(const ScreenSpec& screen, JsonObjectConst config) {
  screen_ = screen;
  applyConfig(config);
  return screen_.isValid();
}

void NewsOutput::onConfigChanged(JsonObjectConst config) {
  applyConfig(config);
  lineCount_ = 0;
  offset_ = 0.0f;
}

bool NewsOutput::start(OutputContext& ctx) {
  news_ = ctx.news;
  started_ = true;
  offset_ = 0.0f;
  lineCount_ = 0;
  builtFor_ = 0;
  if (!lines_) lines_ = new (std::nothrow) Line[kMaxLines];
  return lines_ != nullptr;
}

void NewsOutput::stop() {
  delete[] lines_;
  lines_ = nullptr;
  started_ = false;
  news_ = nullptr;
  lineCount_ = 0;
  builtFor_ = 0;
}

void NewsOutput::appendWrapped(const char* text, uint8_t maxChars, bool firstLine) {
  if (!text || maxChars < 4) return;

  bool first = firstLine;
  while (*text && lineCount_ < kMaxLines) {
    while (*text == ' ') ++text;
    if (!*text) break;

    size_t length = 0;
    size_t lastSpace = 0;
    while (text[length] && length < maxChars) {
      if (text[length] == ' ') lastSpace = length;
      ++length;
    }
    if (text[length] && lastSpace > 0) length = lastSpace;

    Line& line = lines_[lineCount_];
    if (length >= sizeof(line.text)) length = sizeof(line.text) - 1;
    memcpy(line.text, text, length);
    line.text[length] = '\0';
    // Trim any trailing spaces the break left behind.
    while (length > 0 && line.text[length - 1] == ' ') line.text[--length] = '\0';
    line.itemStart = first;
    first = false;
    ++lineCount_;

    text += length;
    while (*text == ' ') ++text;
  }
}

void NewsOutput::rebuildLines(const NewsFeed& feed) {
  lineCount_ = 0;
  if (!lines_) return;

  lineHeight_ = static_cast<uint8_t>(8 * size_ + gap_);
  const uint8_t indent = bullet_ ? 5 : 1;
  uint8_t maxChars = static_cast<uint8_t>((screen_.width - indent - 2) / (6 * size_));
  if (maxChars < 6) maxChars = 6;

  for (uint8_t i = 0; i < feed.count && lineCount_ < kMaxLines; ++i) {
    appendWrapped(feed.items[i], maxChars, true);
    // A blank spacer line between headlines reads better than a hard join.
    if (lineCount_ < kMaxLines) {
      Line& spacer = lines_[lineCount_++];
      spacer.text[0] = '\0';
      spacer.itemStart = false;
    }
  }

  totalHeight_ = static_cast<uint16_t>(lineCount_) * lineHeight_ + gap_;
  if (totalHeight_ == 0) totalHeight_ = lineHeight_;
  offset_ = 0.0f;
  builtFor_ = feed.updatedMs;
}

void NewsOutput::drawHeader(PixelCanvas& target) const {
  if (header_[0] == '\0') return;
  target.setTextSize(1);
  target.setTextColor(headerColor_);
  const int16_t width = target.textWidth(header_);
  target.setCursor(static_cast<int16_t>((screen_.width - width) / 2), 0);
  target.print(header_);
  target.fillRect(0, 9, static_cast<int16_t>(screen_.width), 1, headerColor_);
}

void NewsOutput::render(PixelCanvas& target, const FrameContext& frame) {
  if (!started_ || !lines_) return;
  target.clear(CRGB(0, 0, 0));

  const int16_t height = static_cast<int16_t>(screen_.height);
  const bool hasHeader = header_[0] != '\0';
  const int16_t contentTop = hasHeader ? 11 : 0;

  const NewsFeed feed = news_ ? news_->snapshot() : NewsFeed();
  if (!feed.valid) {
    if (hasHeader) drawHeader(target);
    char message[32];
    if (feed.error[0]) {
      copyText(message, sizeof(message), feed.error);
    } else {
      copyText(message, sizeof(message), news_ && news_->fetching() ? "FETCHING" : "NO DATA");
    }
    target.setTextSize(1);
    target.setTextColor(CRGB(150, 160, 170));
    const int16_t messageWidth = target.textWidth(message);
    target.setCursor(static_cast<int16_t>((screen_.width - messageWidth) / 2),
                     static_cast<int16_t>(contentTop + (height - contentTop - 8) / 2));
    target.print(message);
    return;
  }

  if (feed.updatedMs != builtFor_) rebuildLines(feed);
  if (lineCount_ == 0) return;

  offset_ += static_cast<float>(speed_) * frame.deltaSec;
  while (offset_ >= static_cast<float>(totalHeight_)) offset_ -= static_cast<float>(totalHeight_);

  target.setTextSize(size_);
  target.setTextColor(color_);
  const int16_t lineHeight = static_cast<int16_t>(8 * size_ + gap_);

  for (uint8_t i = 0; i < lineCount_; ++i) {
    const int16_t y = static_cast<int16_t>(contentTop + i * lineHeight - static_cast<int16_t>(offset_));
    if (y <= contentTop - lineHeight || y >= height) continue;
    if (lines_[i].text[0] == '\0') continue;

    int16_t x = 1;
    if (lines_[i].itemStart && bullet_) {
      target.fillRect(1, static_cast<int16_t>(y + 3), 3, 3, color_);
      x = 6;
    }
    target.setCursor(x, y);
    target.print(lines_[i].text);
  }

  // The header is drawn last so scrolled lines pass underneath it cleanly.
  if (hasHeader) {
    target.fillRect(0, 0, static_cast<int16_t>(screen_.width), 10, CRGB(0, 0, 0));
    drawHeader(target);
  }
}

void NewsOutput::writeConfig(JsonObject& out) const {
  out["header"] = header_;
  char hex[8];
  colorToHex(color_, hex, sizeof(hex));
  out["color"] = hex;
  colorToHex(headerColor_, hex, sizeof(hex));
  out["headerColor"] = hex;
  out["speed"] = speed_;
  out["size"] = size_;
  out["gap"] = gap_;
  out["bullet"] = bullet_;
  out["fps"] = fps_;
}

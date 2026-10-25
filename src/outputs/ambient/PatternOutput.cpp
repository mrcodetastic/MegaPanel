#include "PatternOutput.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "util/Logger.hpp"

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

}  // namespace

PatternOutput::PatternOutput()
    : screen_{0, 0}, color_(CRGB::White), speed_(64), started_(false), firstFrame_(true) {}

bool PatternOutput::configure(const ScreenSpec& screen, JsonObjectConst config) {
  screen_ = screen;
  color_ = parseHexColor(config["color"] | "#FFFFFF", CRGB::White);
  speed_ = static_cast<uint8_t>(config["speed"] | 64);
  if (speed_ < 1) speed_ = 1;
  return screen_.isValid();
}

void PatternOutput::onConfigChanged(JsonObjectConst config) {
  color_ = parseHexColor(config["color"] | "#FFFFFF", CRGB::White);
  speed_ = static_cast<uint8_t>(config["speed"] | 64);
  if (speed_ < 1) speed_ = 1;
}

bool PatternOutput::start(OutputContext& ctx) {
  (void)ctx;
  resetPattern();
  started_ = true;
  firstFrame_ = true;
  return true;
}

void PatternOutput::stop() {
  releasePattern();
  started_ = false;
}

void PatternOutput::render(PixelCanvas& target, const FrameContext& frame) {
  if (!started_) return;
  if (firstFrame_) {
    target.clear(CRGB::Black);
    firstFrame_ = false;
  }
  drawPattern(target, frame);
}

void PatternOutput::writeConfig(JsonObject& out) const {
  char hex[8];
  colorToHex(color_, hex, sizeof(hex));
  out["color"] = hex;
  out["speed"] = speed_;
}

void PatternOutput::commonSchema(JsonObject& out) {
  JsonObject color = out["color"].to<JsonObject>();
  color["type"] = "color";
  color["default"] = "#FFFFFF";

  JsonObject speed = out["speed"].to<JsonObject>();
  speed["type"] = "range";
  speed["min"] = 1;
  speed["max"] = 255;
  speed["step"] = 1;
  speed["default"] = 64;
}

#include "SimpleClock.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

SimpleClock::SimpleClock()
    : screen_{0, 0}, color_(CRGB::White), showDate_(true), started_(false) {}

bool SimpleClock::configure(const ScreenSpec& screen, JsonObjectConst config) {
  screen_ = screen;
  color_ = parseHexColor(config["color"] | "#FFFFFF", CRGB::White);
  showDate_ = config["showDate"] | true;
  return screen_.isValid();
}

void SimpleClock::onConfigChanged(JsonObjectConst config) {
  color_ = parseHexColor(config["color"] | "#FFFFFF", CRGB::White);
  showDate_ = config["showDate"] | true;
}

bool SimpleClock::start(OutputContext& ctx) {
  (void)ctx;
  started_ = true;
  return true;
}

void SimpleClock::stop() { started_ = false; }

void SimpleClock::render(PixelCanvas& target, const FrameContext& frame) {
  if (!started_) return;
  target.clear(CRGB::Black);
  drawClock(target, frame);
}

void SimpleClock::writeConfig(JsonObject& out) const {
  char hex[8];
  colorToHex(color_, hex, sizeof(hex));
  out["color"] = hex;
  out["showDate"] = showDate_;
}

void SimpleClock::commonSchema(JsonObject& out) {
  JsonObject color = out["color"].to<JsonObject>();
  color["type"] = "color";
  color["default"] = "#FFFFFF";

  JsonObject showDate = out["showDate"].to<JsonObject>();
  showDate["type"] = "bool";
  showDate["default"] = true;
}

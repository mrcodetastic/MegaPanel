#include "ClockStandard.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "core/FrameContext.hpp"
#include "util/Logger.hpp"

namespace {

CRGB parseHexColor(const char* text, CRGB fallback) {
  if (!text) return fallback;
  if (*text == '#') ++text;
  const size_t length = strlen(text);
  if (length == 6) {
    char* end = nullptr;
    const uint32_t value = static_cast<uint32_t>(strtoul(text, &end, 16));
    if (end && *end == '\0') {
      return CRGB(static_cast<uint8_t>((value >> 16) & 0xFF),
                  static_cast<uint8_t>((value >> 8) & 0xFF),
                  static_cast<uint8_t>(value & 0xFF));
    }
  }
  return fallback;
}

void colorToHex(CRGB color, char* out, size_t capacity) {
  snprintf(out, capacity, "#%02X%02X%02X", color.r, color.g, color.b);
}

void formatTimeText(const FrameContext& frame, bool showColon, char* out, size_t capacity) {
  if (!frame.timeValid) {
    snprintf(out, capacity, "--%c--", showColon ? ':' : ' ');
    return;
  }
  int hour = frame.localTime.tm_hour;
  if (!frame.use24Hour) {
    hour %= 12;
    if (hour == 0) hour = 12;
  }
  snprintf(out, capacity, "%02d%c%02d", hour, showColon ? ':' : ' ', frame.localTime.tm_min);
}

void formatDateText(const FrameContext& frame, char* out, size_t capacity) {
  if (!frame.timeValid) {
    snprintf(out, capacity, "--/--/----");
    return;
  }
  const int day = frame.localTime.tm_mday;
  const int month = frame.localTime.tm_mon + 1;
  const int year = frame.localTime.tm_year + 1900;
  switch (frame.dateFormat) {
    case 1: snprintf(out, capacity, "%02d/%02d/%04d", month, day, year); break;
    case 2: snprintf(out, capacity, "%04d-%02d-%02d", year, month, day); break;
    case 3: snprintf(out, capacity, "%02d.%02d.%04d", day, month, year); break;
    default: snprintf(out, capacity, "%02d/%02d/%04d", day, month, year); break;
  }
}

}  // namespace

ClockStandard::ClockStandard()
    : screen_{0, 0}, color_(CRGB::White), showDate_(true), colonBlink_(true), started_(false) {}

const OutputMeta& ClockStandard::metaStatic() {
  static const OutputMeta meta = {
      "clock.standard",
      "Standard clock",
      "1.0",
      "Ported from animatedpixelclock clock_common.cpp",
      OutputCategory::DisplayOutput,
      LAYER_FG,
      LayerId::Foreground,
      ScreenSpec{0, 0},
      ScreenSpec{32, 24},
      0,
      0,
      5,
      25,
      true,
      true,
      false,
      false,
      false,
      false};
  return meta;
}

void ClockStandard::schemaStatic(JsonObject& out) {
  JsonObject color = out["color"].to<JsonObject>();
  color["type"] = "color";
  color["default"] = "#FFFFFF";

  JsonObject showDate = out["showDate"].to<JsonObject>();
  showDate["type"] = "bool";
  showDate["default"] = true;

  JsonObject colonBlink = out["colonBlink"].to<JsonObject>();
  colonBlink["type"] = "bool";
  colonBlink["default"] = true;
}

bool ClockStandard::configure(const ScreenSpec& screen, JsonObjectConst config) {
  screen_ = screen;
  color_ = parseHexColor(config["color"] | "#FFFFFF", CRGB::White);
  showDate_ = config["showDate"] | true;
  colonBlink_ = config["colonBlink"] | true;
  return screen_.isValid();
}

bool ClockStandard::start(OutputContext& ctx) {
  (void)ctx;
  started_ = true;
  return true;
}

void ClockStandard::stop() { started_ = false; }

void ClockStandard::render(PixelCanvas& target, const FrameContext& frame) {
  if (!started_) return;

  target.clear(CRGB::Black);

  const bool showColon = colonBlink_ ? ((frame.nowMs / 500u) % 2u == 0u) : true;
  char timeText[8];
  formatTimeText(frame, showColon, timeText, sizeof(timeText));

  const int16_t dateHeight = showDate_ ? 10 : 0;
  int16_t availableHeight = static_cast<int16_t>(screen_.height - dateHeight - 4);
  if (availableHeight < 8) availableHeight = static_cast<int16_t>(screen_.height);

  int textSize = (screen_.width - 4) / (5 * 6);
  const int heightSize = availableHeight / 8;
  if (heightSize < textSize) textSize = heightSize;
  if (textSize < 1) textSize = 1;
  if (textSize > 6) textSize = 6;

  const int16_t textWidth = static_cast<int16_t>(5 * 6 * textSize);
  const int16_t x = static_cast<int16_t>((screen_.width - textWidth) / 2);
  const int16_t y = static_cast<int16_t>((availableHeight - 8 * textSize) / 2 + 2);

  target.setTextSize(static_cast<uint8_t>(textSize));
  target.setTextColor(color_);
  target.setCursor(x, y);
  target.print(timeText);

  if (showDate_) {
    char dateText[16];
    formatDateText(frame, dateText, sizeof(dateText));
    target.setTextSize(1);
    target.setTextColor(CRGB(150, 150, 150));
    const int16_t dateWidth = target.textWidth(dateText);
    target.setCursor(static_cast<int16_t>((screen_.width - dateWidth) / 2),
                     static_cast<int16_t>(screen_.height - 9));
    target.print(dateText);
  }

  if (!frame.use24Hour && screen_.width >= 96) {
    target.setTextSize(1);
    target.setTextColor(CRGB(120, 120, 120));
    target.setCursor(static_cast<int16_t>(screen_.width - 14), 1);
    target.print(frame.localTime.tm_hour >= 12 ? "PM" : "AM");
  }

  target.setTextColor(CRGB(255, 255, 255));
}

void ClockStandard::writeConfig(JsonObject& out) const {
  char hex[8];
  colorToHex(color_, hex, sizeof(hex));
  out["color"] = hex;
  out["showDate"] = showDate_;
  out["colonBlink"] = colonBlink_;
}

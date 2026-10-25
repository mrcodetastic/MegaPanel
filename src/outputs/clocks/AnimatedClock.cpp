#include "AnimatedClock.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/FrameContext.hpp"
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

AnimatedClock::AnimatedClock()
    : screen_{0, 0},
      color_(CRGB(255, 255, 255)),
      speed_(5),
      showDate_(false),
      plates_(true),
      colonBlink_(true),
      fps_(30),
      digitSize_(2),
      digitX_{0, 0, 0, 0, 0},
      digitY_(0),
      hour_(0),
      minute_(0),
      haveTime_(false),
      minuteChanged_(false),
      colonOn_(true),
      changedIndex_(255),
      secondsSinceChange_(100.0f),
      started_(false) {}

bool AnimatedClock::configure(const ScreenSpec& screen, JsonObjectConst config) {
  screen_ = screen;
  if (!screen_.isValid()) return false;

  color_ = parseHexColor(config["color"] | "#FFFFFF", CRGB(255, 255, 255));
  speed_ = static_cast<uint8_t>(config["speed"] | 5);
  if (speed_ < 1) speed_ = 1;
  if (speed_ > 10) speed_ = 10;
  showDate_ = config["showDate"] | false;
  plates_ = config["plates"] | true;
  colonBlink_ = config["colonBlink"] | true;
  fps_ = static_cast<uint8_t>(config["fps"] | 30);
  if (fps_ < 5) fps_ = 5;
  if (fps_ > 60) fps_ = 60;

  // Digits occupy the upper half; the scene gets the rest. The size follows
  // the real panel rather than a 128x64 assumption.
  int size = (static_cast<int>(screen_.width) - 2) / 30;
  const int heightBound = (static_cast<int>(screen_.height) * 45 / 100) / 8;
  if (heightBound < size) size = heightBound;
  if (size < 1) size = 1;
  if (size > 5) size = 5;
  digitSize_ = static_cast<uint8_t>(size);

  digitY_ = static_cast<int16_t>((static_cast<int>(screen_.height) - digitHeight()) / 2 -
                                 static_cast<int>(screen_.height) / 8);
  if (digitY_ < 1) digitY_ = 1;

  const int16_t start = static_cast<int16_t>((static_cast<int>(screen_.width) - digitsWidth()) / 2);
  for (uint8_t i = 0; i < 5; ++i) {
    digitX_[i] = static_cast<int16_t>(start + i * 6 * digitSize_);
  }
  return true;
}

void AnimatedClock::onConfigChanged(JsonObjectConst config) {
  color_ = parseHexColor(config["color"] | "#FFFFFF", CRGB(255, 255, 255));
  speed_ = static_cast<uint8_t>(config["speed"] | speed_);
  if (speed_ < 1) speed_ = 1;
  if (speed_ > 10) speed_ = 10;
  showDate_ = config["showDate"] | showDate_;
  plates_ = config["plates"] | plates_;
  colonBlink_ = config["colonBlink"] | colonBlink_;
  const uint8_t fps = static_cast<uint8_t>(config["fps"] | fps_);
  fps_ = fps < 5 ? 5 : (fps > 60 ? 60 : fps);
}

bool AnimatedClock::start(OutputContext& ctx) {
  (void)ctx;
  resetClock();
  started_ = true;
  haveTime_ = false;
  minuteChanged_ = false;
  changedIndex_ = 255;
  secondsSinceChange_ = 100.0f;
  return true;
}

void AnimatedClock::stop() {
  releaseClock();
  started_ = false;
  haveTime_ = false;
}

bool AnimatedClock::colonVisible(const FrameContext& frame) const {
  if (!colonBlink_) return true;
  return ((frame.nowMs / 500u) % 2u) == 0u;
}

float AnimatedClock::changeProgress(float durationSec) const {
  if (durationSec <= 0.0f) return 1.0f;
  const float value = secondsSinceChange_ / durationSec;
  if (value < 0.0f) return 0.0f;
  if (value > 1.0f) return 1.0f;
  return value;
}

char AnimatedClock::digitChar(uint8_t index) const {
  if (index == 2) return colonOn_ ? ':' : ' ';
  const uint8_t value = index < 2 ? hour_ : minute_;
  if (index == 0 || index == 3) return static_cast<char>('0' + value / 10);
  return static_cast<char>('0' + value % 10);
}

void AnimatedClock::drawDigits(PixelCanvas& target, bool plates, const int8_t* offsets) const {
  target.setTextSize(digitSize_);
  target.setTextColor(color_);
  char text[2] = {0, 0};
  for (uint8_t i = 0; i < 5; ++i) {
    const int16_t y = static_cast<int16_t>(digitY_ + (offsets ? offsets[i] : 0));
    if (plates) {
      target.fillRect(static_cast<int16_t>(digitX_[i] - 1), static_cast<int16_t>(y - 1),
                      static_cast<int16_t>(digitWidth() + 2), static_cast<int16_t>(digitHeight() + 2),
                      CRGB(0, 0, 0));
    }
    text[0] = digitChar(i);
    target.setCursor(digitX_[i], y);
    target.print(text);
  }
}

void AnimatedClock::drawDateRow(PixelCanvas& target, const FrameContext& frame, CRGB color) const {
  char dateText[16];
  formatDateText(frame, dateText, sizeof(dateText));
  target.setTextSize(1);
  target.setTextColor(color);
  const int16_t width = target.textWidth(dateText);
  target.setCursor(static_cast<int16_t>((static_cast<int>(screen_.width) - width) / 2),
                   static_cast<int16_t>(screen_.height - 9));
  target.print(dateText);
}

void AnimatedClock::drawSyncNotice(PixelCanvas& target, const FrameContext& frame) const {
  target.setTextSize(1);
  target.setTextColor(color_);
  const char* text = frame.ntpSynced ? "TIME ERROR" : "SYNCING";
  const int16_t width = target.textWidth(text);
  target.setCursor(static_cast<int16_t>((static_cast<int>(screen_.width) - width) / 2),
                   static_cast<int16_t>(screen_.height / 2 - 3));
  target.print(text);
}

void AnimatedClock::render(PixelCanvas& target, const FrameContext& frame) {
  if (!started_) return;

  target.clear(CRGB(0, 0, 0));

  minuteChanged_ = false;
  changedIndex_ = 255;
  colonOn_ = colonVisible(frame);

  if (frame.timeValid) {
    int hour = frame.localTime.tm_hour;
    if (!frame.use24Hour) {
      hour %= 12;
      if (hour == 0) hour = 12;
    }
    const uint8_t newHour = static_cast<uint8_t>(hour);
    const uint8_t newMinute = static_cast<uint8_t>(frame.localTime.tm_min);

    if (!haveTime_) {
      hour_ = newHour;
      minute_ = newMinute;
      haveTime_ = true;
      secondsSinceChange_ = 100.0f;
    } else if (newHour != hour_ || newMinute != minute_) {
      const char oldText[5] = {static_cast<char>('0' + hour_ / 10),
                               static_cast<char>('0' + hour_ % 10), ':',
                               static_cast<char>('0' + minute_ / 10),
                               static_cast<char>('0' + minute_ % 10)};
      const char newText[5] = {static_cast<char>('0' + newHour / 10),
                               static_cast<char>('0' + newHour % 10), ':',
                               static_cast<char>('0' + newMinute / 10),
                               static_cast<char>('0' + newMinute % 10)};
      for (uint8_t i = 0; i < 5; ++i) {
        if (oldText[i] != newText[i] && i != 2) {
          changedIndex_ = i;
          break;
        }
      }
      hour_ = newHour;
      minute_ = newMinute;
      minuteChanged_ = true;
      secondsSinceChange_ = 0.0f;
    }
  } else {
    haveTime_ = false;
  }

  if (!minuteChanged_) secondsSinceChange_ += frame.deltaSec;

  updateClock(frame, frame.deltaSec);
  drawClock(target, frame);

  target.setTextColor(CRGB(255, 255, 255));
  target.setTextSize(1);
}

void AnimatedClock::drawSolidSprite(PixelCanvas& target, int16_t x, int16_t y,
                                    const char* const* rows, uint8_t rowCount, CRGB color) {
  for (uint8_t row = 0; row < rowCount; ++row) {
    const char* line = rows[row];
    if (!line) continue;
    for (int16_t col = 0; line[col]; ++col) {
      if (line[col] == '#') {
        target.drawPixel(static_cast<int16_t>(x + col), static_cast<int16_t>(y + row), color);
      }
    }
  }
}

void AnimatedClock::drawSprite(PixelCanvas& target, int16_t x, int16_t y,
                               const char* const* rows, uint8_t rowCount, const char* keys,
                               const CRGB* colors, uint8_t colorCount) {
  for (uint8_t row = 0; row < rowCount; ++row) {
    const char* line = rows[row];
    if (!line) continue;
    for (int16_t col = 0; line[col]; ++col) {
      const char c = line[col];
      if (c == '.' || c == ' ') continue;
      for (uint8_t k = 0; k < colorCount; ++k) {
        if (keys[k] == c) {
          target.drawPixel(static_cast<int16_t>(x + col), static_cast<int16_t>(y + row), colors[k]);
          break;
        }
      }
    }
  }
}

void AnimatedClock::writeConfig(JsonObject& out) const {
  char hex[8];
  colorToHex(color_, hex, sizeof(hex));
  out["color"] = hex;
  out["speed"] = speed_;
  out["showDate"] = showDate_;
  out["plates"] = plates_;
  out["colonBlink"] = colonBlink_;
  out["fps"] = fps_;
}

void AnimatedClock::commonSchema(JsonObject& out) {
  JsonObject color = out["color"].to<JsonObject>();
  color["type"] = "color";
  color["default"] = "#FFFFFF";

  JsonObject speed = out["speed"].to<JsonObject>();
  speed["type"] = "range";
  speed["min"] = 1;
  speed["max"] = 10;
  speed["default"] = 5;

  JsonObject showDate = out["showDate"].to<JsonObject>();
  showDate["type"] = "bool";
  showDate["default"] = false;

  JsonObject plates = out["plates"].to<JsonObject>();
  plates["type"] = "bool";
  plates["default"] = true;

  JsonObject colonBlink = out["colonBlink"].to<JsonObject>();
  colonBlink["type"] = "bool";
  colonBlink["default"] = true;

  JsonObject fps = out["fps"].to<JsonObject>();
  fps["type"] = "range";
  fps["min"] = 5;
  fps["max"] = 60;
  fps["default"] = 30;
}

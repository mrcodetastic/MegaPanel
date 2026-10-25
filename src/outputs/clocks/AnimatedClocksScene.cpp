#include "AnimatedClocks.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "WorldTime.hpp"
#include "core/FrameContext.hpp"
#include "outputs/weather/WeatherGlyph.hpp"
#include "worldmap.h"

namespace {

constexpr uint8_t kMatrixMaxColumns = 40;
const char kMatrixChars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-<>*=$#%&";

struct WorldCity {
  const char* code;
  float latitude;
  float longitude;
  const char* tz;
};

const WorldCity kWorldCities[] = {
    {"LDN", 51.5f, -0.12f, "GMT0BST,M3.5.0/1,M10.5.0"},
    {"NYC", 40.7f, -74.0f, "EST5EDT,M3.2.0,M11.1.0"},
    {"LAX", 34.0f, -118.2f, "PST8PDT,M3.2.0,M11.1.0"},
    {"SAO", -23.5f, -46.6f, "<-03>3"},
    {"BER", 52.5f, 13.4f, "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"DXB", 25.2f, 55.3f, "<-04>-4"},
    {"DEL", 28.6f, 77.2f, "IST-5:30"},
    {"SIN", 1.35f, 103.8f, "<-08>-8"},
    {"TYO", 35.7f, 139.7f, "JST-9"},
    {"SYD", -33.9f, 151.2f, "AEST-10AEDT,M10.1.0,M4.1.0/3"},
};

constexpr uint8_t kWorldCityCount = sizeof(kWorldCities) / sizeof(kWorldCities[0]);
constexpr int16_t kWorldMapW = 120;
constexpr int16_t kWorldMapH = 56;

float clampf(float value, float low, float high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

float wrapf(float value, float low, float high) {
  const float span = high - low;
  if (span <= 0.0f) return low;
  while (value < low) value += span;
  while (value >= high) value -= span;
  return value;
}

uint16_t nextRandom(uint16_t& seed) {
  seed = static_cast<uint16_t>(seed * 2053u + 13849u);
  return static_cast<uint16_t>(seed >> 4);
}

void localClock(int64_t localSeconds, uint8_t* hour, uint8_t* minute) {
  int64_t minutes = localSeconds / 60;
  int64_t dayMinutes = minutes % 1440;
  if (dayMinutes < 0) dayMinutes += 1440;
  *hour = static_cast<uint8_t>(dayMinutes / 60);
  *minute = static_cast<uint8_t>(dayMinutes % 60);
}

}  // namespace

void noExtraSchema(JsonObject& out) { (void)out; }

void clockWorldSchema(JsonObject& out) {
  JsonObject rotate = out["rotate"].to<JsonObject>();
  rotate["type"] = "bool";
  rotate["default"] = true;

  JsonObject city = out["city"].to<JsonObject>();
  city["type"] = "number";
  city["min"] = 0;
  city["max"] = 9;
  city["default"] = 0;

  JsonObject showMap = out["showMap"].to<JsonObject>();
  showMap["type"] = "bool";
  showMap["default"] = true;
}

// ---------------------------------------------------------------------------
// Matrix rain
// ---------------------------------------------------------------------------
struct ClockMatrix::State {
  struct Column {
    float head;
    float speed;
    uint8_t length;
    bool active;
  } columns[kMatrixMaxColumns];
  uint8_t count;
  uint8_t rows;
  uint16_t seed;
};

ClockMatrix::ClockMatrix() : state_(nullptr) {}

void ClockMatrix::resetClock() {
  if (!state_) state_ = new (std::nothrow) State();
  if (!state_) return;
  State& s = *state_;
  s.seed = 0x7A11u;
  s.rows = static_cast<uint8_t>(screen_.height / 8) + 1;
  s.count = static_cast<uint8_t>(screen_.width / 6);
  if (s.count > kMatrixMaxColumns) s.count = kMatrixMaxColumns;
  for (uint8_t i = 0; i < kMatrixMaxColumns; ++i) {
    State::Column& c = s.columns[i];
    c.head = static_cast<float>(nextRandom(s.seed) % (s.rows * 8)) / 8.0f - s.rows;
    c.speed = 3.0f + static_cast<float>(nextRandom(s.seed) % 70u) / 10.0f;
    c.length = static_cast<uint8_t>(3 + nextRandom(s.seed) % 6u);
    c.active = i < s.count;
  }
}

void ClockMatrix::releaseClock() {
  delete state_;
  state_ = nullptr;
}

void ClockMatrix::updateClock(const FrameContext& frame, float dt) {
  (void)frame;
  if (!state_) return;
  State& s = *state_;
  dt = clampf(dt, 0.0f, 0.1f);
  const float scale = 0.4f + static_cast<float>(speed_) * 0.18f;

  for (uint8_t i = 0; i < s.count; ++i) {
    State::Column& c = s.columns[i];
    c.head += c.speed * scale * dt;
    if (c.head - c.length > static_cast<float>(s.rows)) {
      c.head = -static_cast<float>(nextRandom(s.seed) % 6u);
      c.speed = 3.0f + static_cast<float>(nextRandom(s.seed) % 70u) / 10.0f;
      c.length = static_cast<uint8_t>(3 + nextRandom(s.seed) % 6u);
    }
  }
}

void ClockMatrix::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!state_) return;
  if (!haveTime()) {
    drawSyncNotice(target, frame);
    return;
  }
  State& s = *state_;
  const uint8_t charsetSize = static_cast<uint8_t>(sizeof(kMatrixChars) - 1);
  const int16_t height = static_cast<int16_t>(screen_.height);

  target.setTextSize(1);
  for (uint8_t i = 0; i < s.count; ++i) {
    const State::Column& c = s.columns[i];
    const int16_t x = static_cast<int16_t>(i * 6);
    const int16_t headRow = static_cast<int16_t>(c.head);
    for (uint8_t k = 0; k <= c.length; ++k) {
      const int16_t row = static_cast<int16_t>(headRow - k);
      if (row < 0 || row * 8 >= height) continue;
      const float t = 1.0f - (c.head - static_cast<float>(row)) / (c.length + 1.0f);
      const uint8_t level = static_cast<uint8_t>(clampf(t, 0.0f, 1.0f) * 255.0f);
      const CRGB shade(static_cast<uint8_t>(color_.r * level / 255),
                       static_cast<uint8_t>(color_.g * level / 255),
                       static_cast<uint8_t>(color_.b * level / 255));
      const uint16_t index = static_cast<uint16_t>(nextRandom(s.seed) % charsetSize);
      char glyph[2] = {kMatrixChars[index], 0};
      target.setTextColor(k == 0 ? color_ : shade);
      target.setCursor(x, static_cast<int16_t>(row * 8));
      target.print(glyph);
    }
  }

  drawDigits(target, plates_, nullptr);

  if (changedIndex() < 5 && secondsSinceChange() < 0.28f) {
    const uint8_t slot = changedIndex();
    target.fillRect(static_cast<int16_t>(digitX(slot) - 1), static_cast<int16_t>(digitY() - 1),
                    static_cast<int16_t>(digitWidth() + 2), static_cast<int16_t>(digitHeight() + 2),
                    CRGB(0, 0, 0));
    const uint16_t index = static_cast<uint16_t>(nextRandom(s.seed) % charsetSize);
    char glyph[2] = {kMatrixChars[index], 0};
    target.setTextSize(digitSize());
    target.setTextColor(color_);
    target.setCursor(digitX(slot), digitY());
    target.print(glyph);
  }

  if (showDate_) drawDateRow(target, frame, CRGB(0, 200, 90));
}

// ---------------------------------------------------------------------------
// Weather
// ---------------------------------------------------------------------------
struct ClockWeather::State {
  float phase;
};

ClockWeather::ClockWeather() : state_(nullptr) {}

void ClockWeather::resetClock() {
  if (!state_) state_ = new (std::nothrow) State();
  if (!state_) return;
  state_->phase = 0.0f;
}

void ClockWeather::releaseClock() {
  delete state_;
  state_ = nullptr;
}

void ClockWeather::updateClock(const FrameContext& frame, float dt) {
  (void)frame;
  if (!state_) return;
  state_->phase += dt;
}

void ClockWeather::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!state_) return;

  const int16_t width = static_cast<int16_t>(screen_.width);
  const int16_t height = static_cast<int16_t>(screen_.height);
  const uint8_t timeSize = height >= 96 ? 3 : (height >= 48 ? 2 : 1);
  const uint8_t iconSize = height >= 96 ? 32 : (height >= 56 ? 20 : 16);
  const uint8_t tempSize = height >= 96 ? 4 : 2;

  char text[24];
  if (haveTime()) {
    snprintf(text, sizeof(text), "%02u%c%02u", static_cast<unsigned>(hour()),
             colonVisible(frame) ? ':' : ' ', static_cast<unsigned>(minute()));
  } else {
    snprintf(text, sizeof(text), "--:--");
  }
  target.setTextSize(timeSize);
  target.setTextColor(color_);
  const int16_t timeWidth = target.textWidth(text);
  target.setCursor(static_cast<int16_t>((width - timeWidth) / 2), 0);
  target.print(text);

  const WeatherData* data = frame.weather;
  const int16_t iconY = static_cast<int16_t>(8 * timeSize + 1);

  if (!data || !data->configured) {
    target.setTextSize(1);
    target.setTextColor(CRGB(140, 140, 140));
    const char* line1 = "WEATHER";
    const char* line2 = "SET LOCATION";
    target.setCursor(static_cast<int16_t>((width - target.textWidth(line1)) / 2),
                     static_cast<int16_t>(height / 2 + 2));
    target.print(line1);
    target.setCursor(static_cast<int16_t>((width - target.textWidth(line2)) / 2),
                     static_cast<int16_t>(height / 2 + 12));
    target.print(line2);
    return;
  }

  const WeatherIcon icon = data->valid ? data->icon : WeatherIcon::Unknown;
  const CRGB iconColor = data->stale ? CRGB(120, 140, 150) : color_;
  drawWeatherIcon(target, 2, iconY, iconSize, icon, iconColor, frame.nowMs);

  if (!data->valid) {
    target.setTextSize(1);
    target.setTextColor(CRGB(140, 140, 140));
    const char* line = "NO DATA";
    target.setCursor(static_cast<int16_t>((width - target.textWidth(line)) / 2),
                     static_cast<int16_t>(height - 11));
    target.print(line);
    return;
  }

  const int16_t temp = data->displayTemp(data->fahrenheit);
  snprintf(text, sizeof(text), "%d", static_cast<int>(temp));
  target.setTextSize(tempSize);
  target.setTextColor(data->stale ? CRGB(150, 160, 170) : color_);
  const int16_t tempWidth = target.textWidth(text);
  const int16_t tempX = static_cast<int16_t>(width - tempWidth - 5);
  const int16_t tempY = static_cast<int16_t>(iconY + (iconSize - 8 * tempSize) / 2);
  target.setCursor(tempX, tempY);
  target.print(text);
  target.fillRect(static_cast<int16_t>(tempX + tempWidth), static_cast<int16_t>(tempY - 1), 2, 2,
                  color_);

  const int16_t detailY = static_cast<int16_t>(height - 9);
  target.setTextSize(1);
  target.setTextColor(CRGB(140, 150, 160));
  if (data->stale) {
    snprintf(text, sizeof(text), "~%d/%d %u%%", static_cast<int>(data->displayTempMax(data->fahrenheit)),
             static_cast<int>(data->displayTempMin(data->fahrenheit)),
             static_cast<unsigned>(data->humidity));
  } else if (data->sunrise[0] && data->sunset[0] &&
             fmodf(state_->phase, 10.0f) > 5.0f) {
    snprintf(text, sizeof(text), "%s-%s", data->sunrise, data->sunset);
  } else {
    snprintf(text, sizeof(text), "%d/%d %u%%", static_cast<int>(data->displayTempMax(data->fahrenheit)),
             static_cast<int>(data->displayTempMin(data->fahrenheit)),
             static_cast<unsigned>(data->humidity));
  }
  const int16_t detailWidth = target.textWidth(text);
  target.setCursor(static_cast<int16_t>((width - detailWidth) / 2), detailY);
  target.print(text);
}

// ---------------------------------------------------------------------------
// World clock
// ---------------------------------------------------------------------------
struct ClockWorld::State {
  PosixTz zones[kWorldCityCount];
  bool zoneValid[kWorldCityCount];
  uint8_t active;
  float rotateTimer;
};

ClockWorld::ClockWorld() : state_(nullptr), rotate_(true), showMap_(true) {}

bool ClockWorld::configure(const ScreenSpec& screen, JsonObjectConst config) {
  if (!AnimatedClock::configure(screen, config)) return false;
  rotate_ = config["rotate"] | true;
  showMap_ = config["showMap"] | true;
  return true;
}

void ClockWorld::onConfigChanged(JsonObjectConst config) {
  AnimatedClock::onConfigChanged(config);
  rotate_ = config["rotate"] | rotate_;
  showMap_ = config["showMap"] | showMap_;
}

void ClockWorld::writeConfig(JsonObject& out) const {
  AnimatedClock::writeConfig(out);
  out["rotate"] = rotate_;
  out["showMap"] = showMap_;
  out["city"] = state_ ? state_->active : 0;
}

void ClockWorld::resetClock() {
  if (!state_) state_ = new (std::nothrow) State();
  if (!state_) return;
  State& s = *state_;
  for (uint8_t i = 0; i < kWorldCityCount; ++i) {
    s.zoneValid[i] = posixTzParse(kWorldCities[i].tz, &s.zones[i]);
  }
  s.active = 0;
  s.rotateTimer = 0.0f;
}

void ClockWorld::releaseClock() {
  delete state_;
  state_ = nullptr;
}

void ClockWorld::updateClock(const FrameContext& frame, float dt) {
  (void)frame;
  if (!state_) return;
  State& s = *state_;
  if (!rotate_) return;
  s.rotateTimer += clampf(dt, 0.0f, 0.1f);
  if (s.rotateTimer >= 6.0f) {
    s.rotateTimer = 0.0f;
    s.active = static_cast<uint8_t>((s.active + 1) % kWorldCityCount);
  }
}

void ClockWorld::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!state_) return;
  if (!frame.timeValid) {
    drawSyncNotice(target, frame);
    return;
  }
  State& s = *state_;
  const int16_t width = static_cast<int16_t>(screen_.width);
  const int16_t height = static_cast<int16_t>(screen_.height);
  const WorldCity& city = kWorldCities[s.active];

  uint8_t cityHour = 0;
  uint8_t cityMinute = 0;
  if (s.zoneValid[s.active]) {
    localClock(frame.epoch + posixTzOffset(s.zones[s.active], frame.epoch), &cityHour, &cityMinute);
  }

  const uint8_t textSize = width >= 96 ? 2 : 1;
  const int16_t textHeight = static_cast<int16_t>(8 * textSize);

  if (!showMap_) {
    char big[16];
    snprintf(big, sizeof(big), "%02u:%02u", static_cast<unsigned>(cityHour),
             static_cast<unsigned>(cityMinute));
    target.setTextSize(static_cast<uint8_t>(textSize + 1));
    target.setTextColor(color_);
    const int16_t bigWidth = target.textWidth(big);
    target.setCursor(static_cast<int16_t>((width - bigWidth) / 2),
                     static_cast<int16_t>(height / 2 - 8 * textSize));
    target.print(big);
    target.setTextSize(textSize);
    target.setTextColor(CRGB(150, 160, 170));
    const int16_t codeWidth = target.textWidth(city.code);
    target.setCursor(static_cast<int16_t>((width - codeWidth) / 2),
                     static_cast<int16_t>(height / 2 + 8 * textSize));
    target.print(city.code);
    return;
  }

  // Fit the 120x56 map above the city line.
  const int16_t availableW = static_cast<int16_t>(width - 4);
  const int16_t availableH = static_cast<int16_t>(height - textHeight - 4);
  int16_t mapW = availableW;
  int16_t mapH = static_cast<int16_t>(mapW * kWorldMapH / kWorldMapW);
  if (mapH > availableH) {
    mapH = availableH;
    mapW = static_cast<int16_t>(mapH * kWorldMapW / kWorldMapH);
  }
  if (mapW < 8 || mapH < 4) return;
  const int16_t originX = static_cast<int16_t>((width - mapW) / 2);
  const int16_t originY = 1;

  const CRGB land(40, 130, 90);
  for (int16_t sy = 0; sy < mapH; ++sy) {
    const int16_t my = static_cast<int16_t>(sy * kWorldMapH / mapH);
    for (int16_t sx = 0; sx < mapW; ++sx) {
      if (((sx + sy) & 1) != 0) continue;
      const int16_t mx = static_cast<int16_t>(sx * kWorldMapW / mapW);
      if (pgm_read_word(&_WORLD_MAP[my * kWorldMapW + mx]) == 0) continue;
      target.drawPixel(static_cast<int16_t>(originX + sx), static_cast<int16_t>(originY + sy), land);
    }
  }

  for (uint8_t i = 0; i < kWorldCityCount; ++i) {
    const int16_t mx = static_cast<int16_t>((kWorldCities[i].longitude + 180.0f) / 360.0f *
                                            static_cast<float>(mapW));
    const int16_t my = static_cast<int16_t>((90.0f - kWorldCities[i].latitude) / 180.0f *
                                            static_cast<float>(mapH));
    const int16_t x = static_cast<int16_t>(originX + mx);
    const int16_t y = static_cast<int16_t>(originY + my);
    if (i == s.active) {
      if ((frame.nowMs / 400u) % 2u == 0u) target.fillRect(x, y, 2, 2, color_);
    } else {
      target.drawPixel(x, y, CRGB(90, 110, 130));
    }
  }

  char line[16];
  snprintf(line, sizeof(line), "%s %02u:%02u", city.code, static_cast<unsigned>(cityHour),
           static_cast<unsigned>(cityMinute));
  target.setTextSize(textSize);
  target.setTextColor(color_);
  const int16_t lineWidth = target.textWidth(line);
  target.setCursor(static_cast<int16_t>((width - lineWidth) / 2),
                   static_cast<int16_t>(height - textHeight - 1));
  target.print(line);
}

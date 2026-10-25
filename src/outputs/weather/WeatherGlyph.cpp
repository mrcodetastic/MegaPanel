#include "WeatherGlyph.hpp"

#include <cmath>
#include <stdint.h>

namespace {

struct Scaler {
  float s;
  int16_t x;
  int16_t y;

  int16_t px(float value) const { return static_cast<int16_t>(x + value * s + 0.5f); }
  int16_t py(float value) const { return static_cast<int16_t>(y + value * s + 0.5f); }
  int16_t len(float value) const {
    const int16_t scaled = static_cast<int16_t>(value * s + 0.5f);
    return scaled < 1 ? 1 : scaled;
  }
};

void fillDisc(PixelCanvas& canvas, const Scaler& k, float cx, float cy, float radius, CRGB color) {
  const int16_t x0 = k.px(cx - radius);
  const int16_t x1 = k.px(cx + radius);
  const int16_t y0 = k.py(cy - radius);
  const int16_t y1 = k.py(cy + radius);
  const float r2 = radius * radius;
  for (int16_t yy = y0; yy <= y1; ++yy) {
    const float dy = (yy - k.py(cy)) / k.s;
    const float span = r2 - dy * dy;
    if (span <= 0.0f) continue;
    const float dx = sqrtf(span);
    const int16_t sx = static_cast<int16_t>(k.px(cx - dx));
    const int16_t ex = static_cast<int16_t>(k.px(cx + dx));
    if (ex >= sx) canvas.fillRect(sx, yy, static_cast<int16_t>(ex - sx + 1), 1, color);
  }
}

void line(PixelCanvas& canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, CRGB color) {
  int16_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
  int16_t dy = y1 > y0 ? y1 - y0 : y0 - y1;
  const int16_t sx = x0 < x1 ? 1 : -1;
  const int16_t sy = y0 < y1 ? 1 : -1;
  int16_t err = static_cast<int16_t>(dx - dy);
  while (true) {
    canvas.drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    const int16_t e2 = static_cast<int16_t>(err * 2);
    if (e2 > -dy) {
      err = static_cast<int16_t>(err - dy);
      x0 = static_cast<int16_t>(x0 + sx);
    }
    if (e2 < dx) {
      err = static_cast<int16_t>(err + dx);
      y0 = static_cast<int16_t>(y0 + sy);
    }
  }
}

void drawSun(PixelCanvas& canvas, const Scaler& k, float cx, float cy, float radius,
             uint32_t nowMs, CRGB color, bool rays) {
  fillDisc(canvas, k, cx, cy, radius, color);
  if (!rays) return;
  const float phase = static_cast<float>(nowMs % 8000u) / 8000.0f;
  for (uint8_t i = 0; i < 8; ++i) {
    const float angle = phase * 6.2831853f + static_cast<float>(i) * 0.7853982f;
    const float c = cosf(angle);
    const float s = sinf(angle);
    line(canvas, k.px(cx + c * (radius + 1.0f)), k.py(cy + s * (radius + 1.0f)),
         k.px(cx + c * (radius + 2.4f)), k.py(cy + s * (radius + 2.4f)), color);
  }
}

void drawCloud(PixelCanvas& canvas, const Scaler& k, float cx, float cy, float scale, CRGB color) {
  fillDisc(canvas, k, cx - 3.0f * scale, cy, 2.4f * scale, color);
  fillDisc(canvas, k, cx, cy - 1.4f * scale, 3.2f * scale, color);
  fillDisc(canvas, k, cx + 3.2f * scale, cy, 2.2f * scale, color);
  canvas.fillRect(k.px(cx - 4.6f * scale), k.py(cy),
                  k.len(9.0f * scale), k.len(2.0f * scale), color);
}

void drawDrops(PixelCanvas& canvas, const Scaler& k, uint32_t nowMs, CRGB color, bool snow,
               bool drizzle) {
  const uint8_t count = drizzle ? 2 : 3;
  const float speed = drizzle ? 0.02f : 0.05f;
  for (uint8_t i = 0; i < count; ++i) {
    const float phase = static_cast<float>(nowMs) * speed + static_cast<float>(i) * 0.37f;
    const float travel = 3.0f;
    const float yy = 10.5f + fmodf(phase, 1.0f) * travel;
    const float xx = 5.0f + static_cast<float>(i) * 3.0f;
    if (snow) {
      const int16_t sx = k.px(xx);
      const int16_t sy = k.py(yy);
      canvas.drawPixel(sx, sy, color);
      canvas.drawPixel(static_cast<int16_t>(sx - 1), sy, color);
      canvas.drawPixel(static_cast<int16_t>(sx + 1), sy, color);
      canvas.drawPixel(sx, static_cast<int16_t>(sy - 1), color);
      canvas.drawPixel(sx, static_cast<int16_t>(sy + 1), color);
    } else {
      const int16_t sy = k.py(yy);
      canvas.fillRect(k.px(xx), sy, k.len(0.9f), drizzle ? k.len(1.0f) : k.len(1.8f), color);
    }
  }
}

void drawBolt(PixelCanvas& canvas, const Scaler& k, CRGB color) {
  static const int8_t points[][2] = {{9, 9}, {7, 13}, {9, 13}, {8, 16}, {12, 11}, {10, 11}, {11, 9}};
  const uint8_t count = sizeof(points) / sizeof(points[0]);
  for (uint8_t i = 0; i + 1 < count; ++i) {
    line(canvas, k.px(points[i][0]), k.py(points[i][1]), k.px(points[i + 1][0]),
         k.py(points[i + 1][1]), color);
  }
}

}  // namespace

void drawWeatherIcon(PixelCanvas& canvas,
                     int16_t x,
                     int16_t y,
                     uint8_t size,
                     WeatherIcon icon,
                     CRGB color,
                     uint32_t nowMs) {
  if (size < 8) size = 8;
  Scaler k{static_cast<float>(size) / 16.0f, x, y};

  switch (icon) {
    case WeatherIcon::Clear:
      drawSun(canvas, k, 8.0f, 8.0f, 3.4f, nowMs, color, true);
      break;
    case WeatherIcon::PartlyCloudy:
      drawSun(canvas, k, 5.5f, 6.0f, 2.8f, nowMs, color, true);
      drawCloud(canvas, k, 9.0f, 10.0f, 0.85f, color);
      break;
    case WeatherIcon::Cloudy:
      drawCloud(canvas, k, 7.0f, 7.0f, 0.7f, color);
      drawCloud(canvas, k, 9.0f, 10.5f, 0.9f, color);
      break;
    case WeatherIcon::Fog: {
      const float drift = fmodf(static_cast<float>(nowMs) * 0.004f, 1.0f);
      canvas.fillRect(k.px(2.0f + drift * 2.0f), k.py(6.0f), k.len(10.0f - drift * 3.0f),
                      k.len(1.4f), color);
      canvas.fillRect(k.px(4.0f - drift * 2.0f), k.py(9.0f), k.len(9.0f), k.len(1.4f), color);
      canvas.fillRect(k.px(2.0f + drift * 3.0f), k.py(12.0f), k.len(8.0f), k.len(1.4f), color);
      break;
    }
    case WeatherIcon::Drizzle:
      drawCloud(canvas, k, 8.0f, 7.0f, 0.95f, color);
      drawDrops(canvas, k, nowMs, color, false, true);
      break;
    case WeatherIcon::Rain:
      drawCloud(canvas, k, 8.0f, 7.0f, 0.95f, color);
      drawDrops(canvas, k, nowMs, color, false, false);
      break;
    case WeatherIcon::Snow:
      drawCloud(canvas, k, 8.0f, 7.0f, 0.95f, color);
      drawDrops(canvas, k, nowMs, color, true, false);
      break;
    case WeatherIcon::Thunder:
      drawCloud(canvas, k, 8.0f, 6.5f, 0.95f, color);
      drawBolt(canvas, k, color);
      break;
    case WeatherIcon::Wind: {
      const float drift = fmodf(static_cast<float>(nowMs) * 0.01f, 4.0f);
      canvas.fillRect(k.px(2.0f), k.py(5.0f + drift * 0.5f), k.len(9.0f + drift), k.len(1.2f), color);
      canvas.fillRect(k.px(4.0f), k.py(8.5f), k.len(8.0f - drift), k.len(1.2f), color);
      canvas.fillRect(k.px(2.0f + drift * 0.5f), k.py(12.0f), k.len(7.0f + drift), k.len(1.2f), color);
      break;
    }
    case WeatherIcon::Unknown:
    default: {
      canvas.fillRect(k.px(5.0f), k.py(4.0f), k.len(6.0f), k.len(1.4f), color);
      canvas.fillRect(k.px(9.6f), k.py(5.4f), k.len(1.4f), k.len(3.0f), color);
      canvas.fillRect(k.px(7.0f), k.py(8.4f), k.len(4.0f), k.len(1.4f), color);
      canvas.fillRect(k.px(7.6f), k.py(9.8f), k.len(1.4f), k.len(2.2f), color);
      canvas.fillRect(k.px(7.6f), k.py(13.4f), k.len(1.4f), k.len(1.4f), color);
      break;
    }
  }
}

const char* weatherShortLabel(WeatherIcon icon) {
  switch (icon) {
    case WeatherIcon::Clear: return "CLEAR";
    case WeatherIcon::PartlyCloudy: return "PARTLY";
    case WeatherIcon::Cloudy: return "CLOUDY";
    case WeatherIcon::Fog: return "FOG";
    case WeatherIcon::Drizzle: return "DRIZZLE";
    case WeatherIcon::Rain: return "RAIN";
    case WeatherIcon::Snow: return "SNOW";
    case WeatherIcon::Thunder: return "STORM";
    case WeatherIcon::Wind: return "WIND";
    default: return "N/A";
  }
}

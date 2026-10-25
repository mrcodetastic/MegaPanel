#include "Layout.hpp"

Layout::Layout(const ScreenSpec& spec, uint8_t margin)
    : screen(spec),
      safe{margin,
           margin,
           static_cast<int16_t>(spec.width > 2u * margin ? spec.width - 2u * margin : 0),
           static_cast<int16_t>(spec.height > 2u * margin ? spec.height - 2u * margin : 0)} {}

int16_t Layout::unit(int16_t value) const {
  const int16_t base = screen.width < screen.height ? screen.width : screen.height;
  int32_t scaled = static_cast<int32_t>(value) * base / 64;
  if (scaled == 0 && value > 0) scaled = 1;
  return static_cast<int16_t>(scaled);
}

Rect Layout::centered(int16_t width, int16_t height) const {
  int16_t x = static_cast<int16_t>((screen.width - width) / 2);
  int16_t y = static_cast<int16_t>((screen.height - height) / 2);
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  return Rect{x, y, width, height};
}

Rect Layout::inset(int16_t px) const {
  int16_t x = static_cast<int16_t>(safe.x + px);
  int16_t y = static_cast<int16_t>(safe.y + px);
  int16_t w = static_cast<int16_t>(safe.w - 2 * px);
  int16_t h = static_cast<int16_t>(safe.h - 2 * px);
  if (w < 0) w = 0;
  if (h < 0) h = 0;
  return Rect{x, y, w, h};
}

int16_t Layout::alignX(int16_t contentWidth) const {
  const int16_t x = static_cast<int16_t>((screen.width - contentWidth) / 2);
  return x < 0 ? 0 : x;
}

int16_t Layout::alignY(int16_t contentHeight) const {
  const int16_t y = static_cast<int16_t>((screen.height - contentHeight) / 2);
  return y < 0 ? 0 : y;
}

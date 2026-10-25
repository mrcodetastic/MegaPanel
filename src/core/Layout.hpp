#pragma once

#include "Types.hpp"

// Basic screen geometry and scaling helpers. All values are in logical pixels.
class Layout {
 public:
  ScreenSpec screen;
  Rect safe;

  explicit Layout(const ScreenSpec& spec = ScreenSpec{0, 0}, uint8_t margin = 1);

  // Scale a value from a 64 px reference to the smaller screen dimension.
  int16_t unit(int16_t value) const;

  Rect centered(int16_t width, int16_t height) const;
  Rect inset(int16_t px) const;
  int16_t alignX(int16_t contentWidth) const;
  int16_t alignY(int16_t contentHeight) const;
};

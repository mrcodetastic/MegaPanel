#pragma once

#include "Layout.hpp"

enum class ClockArrangement : uint8_t {
  FiveAcross,
  TwoRows,
  SingleRow,
  Custom
};

// Geometry shared by digit-based clocks and animated clocks. It is calculated
// from the logical screen and the requested digit size, never from a fixed
// 128x64 assumption.
struct ClockLayout {
  ClockArrangement arrangement;
  Rect time;
  Rect digits[5];   // 0,1 = hour; 2 = colon; 3,4 = minute
  Rect date;
  Rect playArea;
  int16_t groundY;
  int16_t patrolY;
  uint8_t digitW;
  uint8_t digitH;
  uint8_t gap;
  uint8_t textSize;

  ClockLayout();

  static ClockLayout choose(const ScreenSpec& screen,
                            uint8_t digitW,
                            uint8_t digitH,
                            uint8_t gap,
                            bool withDate);
};

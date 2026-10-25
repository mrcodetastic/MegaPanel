#pragma once

#include "core/Types.hpp"

// Default pin map is the Waveshare ESP32-S3 RGB Matrix driver board layout.
// Change these values for a hand-wired panel or another board.
struct PanelPinMap {
  int8_t r1, g1, b1;
  int8_t r2, g2, b2;
  int8_t a, b, c, d, e;
  int8_t lat, oe, clk;

  PanelPinMap()
      : r1(4), g1(5), b1(6),
        r2(7), g2(15), b2(16),
        a(18), b(8), c(3), d(42), e(9),
        lat(40), oe(2), clk(41) {}
};

struct PanelConfig {
  ScreenSpec logical;
  uint16_t panelWidth;
  uint16_t panelHeight;
  uint8_t rows;
  uint8_t cols;
  uint8_t chainType;
  uint8_t rotation;
  uint8_t driver;  // 0 = FM6126A, 1 = ICN2038S
  PanelPinMap pins;
  bool clkPhase;
  bool doubleBuffer;
  bool swapBlueGreen;
  uint8_t brightness;

  PanelConfig()
      : logical{64, 64},
        panelWidth(64),
        panelHeight(64),
        rows(1),
        cols(1),
        chainType(0),
        rotation(0),
        driver(0),
        pins(),
        clkPhase(false),
        doubleBuffer(true),
        swapBlueGreen(false),
        brightness(128) {}
};

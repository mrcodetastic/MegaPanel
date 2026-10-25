#pragma once

#include <GFX_Layer.hpp>
#include "Types.hpp"

// GFX_Layer with the small drawing helpers the ported outputs need. It remains
// a normal GFX_Layer, so it is still a single framebuffer with layer alpha and
// colour-key transparency.
class PixelCanvas : public GFX_Layer {
 public:
  PixelCanvas(uint16_t width, uint16_t height);

  void drawFastHLine(int16_t x, int16_t y, int16_t w, CRGB color) {
    fillRect(x, y, w, 1, color);
  }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    fillRect(x, y, w, 1, color);
  }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, CRGB color) {
    fillRect(x, y, 1, h, color);
  }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    fillRect(x, y, 1, h, color);
  }

  int16_t textWidth(const char* text);

  void applyStyle(const LayerStyle& style);
  void clearForStyle(const LayerStyle& style);

  void drawTextCentered(const char* text, int16_t y, CRGB color, uint8_t size = 1);
  void drawTextCentered(const char* text, int16_t y, uint16_t color, uint8_t size = 1);
};

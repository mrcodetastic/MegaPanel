#include "PixelCanvas.hpp"

PixelCanvas::PixelCanvas(uint16_t width, uint16_t height) : GFX_Layer(width, height) {}

int16_t PixelCanvas::textWidth(const char* text) {
  if (!text) return 0;
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return static_cast<int16_t>(w);
}

void PixelCanvas::applyStyle(const LayerStyle& style) {
  setAlpha(style.alpha);
  if (style.transparency == LayerTransparency::ColorKey) {
    setColorKey(style.colorKey);
  } else {
    disableTransparency();
  }
}

void PixelCanvas::clearForStyle(const LayerStyle& style) {
  if (style.transparency == LayerTransparency::ColorKey) {
    clear(style.colorKey);
  } else {
    clear(CRGB::Black);
  }
}

void PixelCanvas::drawTextCentered(const char* text, int16_t y, CRGB color, uint8_t size) {
  if (!text) return;
  setTextSize(size);
  setTextColor(color);
  const int16_t w = textWidth(text);
  setCursor(static_cast<int16_t>((width() - w) / 2), y);
  print(text);
  setTextColor(CRGB(255, 255, 255));
}

void PixelCanvas::drawTextCentered(const char* text, int16_t y, uint16_t color, uint8_t size) {
  if (!text) return;
  setTextSize(size);
  setTextColor(color);
  const int16_t w = textWidth(text);
  setCursor(static_cast<int16_t>((width() - w) / 2), y);
  print(text);
  setTextColor(static_cast<uint16_t>(0xFFFF));
}

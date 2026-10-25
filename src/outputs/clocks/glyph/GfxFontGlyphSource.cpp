#include "GfxFontGlyphSource.hpp"

#include <Arduino.h>

GfxFontGlyphSource::GfxFontGlyphSource(const GFXfont* font) : font_(font) {}

bool GfxFontGlyphSource::glyph(uint8_t character, GlyphPoints& out) {
  out.points.clear();
  if (!font_ || character < font_->first || character > font_->last) return false;

  const GFXglyph* glyph = &font_->glyph[character - font_->first];
  const uint16_t bitmapOffset = pgm_read_word(&glyph->bitmapOffset);
  const uint8_t width = static_cast<uint8_t>(pgm_read_byte(&glyph->width));
  const uint8_t height = static_cast<uint8_t>(pgm_read_byte(&glyph->height));
  const int8_t xOffset = static_cast<int8_t>(pgm_read_byte(&glyph->xOffset));
  const int8_t yOffset = static_cast<int8_t>(pgm_read_byte(&glyph->yOffset));

  if (width == 0 || height == 0) return false;

  out.points.reserve(static_cast<size_t>(width) * height / 3 + 1);
  uint16_t offset = bitmapOffset;
  uint8_t bits = 0;
  uint8_t bit = 0;

  for (uint8_t y = 0; y < height; ++y) {
    for (uint8_t x = 0; x < width; ++x) {
      if ((bit++ & 7) == 0) {
        bits = pgm_read_byte(&font_->bitmap[offset++]);
      }
      if (bits & 0x80) {
        out.points.push_back(Point{static_cast<int16_t>(xOffset + x),
                                   static_cast<int16_t>(yOffset + y)});
      }
      bits <<= 1;
    }
  }

  out.recomputeBounds();
  return !out.points.empty();
}

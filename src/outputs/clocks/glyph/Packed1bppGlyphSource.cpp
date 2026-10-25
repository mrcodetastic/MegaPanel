#include "Packed1bppGlyphSource.hpp"

#include <Arduino.h>

Packed1bppGlyphSource::Packed1bppGlyphSource(const uint8_t* font)
    : font_(font),
      height_(font ? pgm_read_byte(font + 1) : 0),
      heightBytes_(font ? static_cast<uint8_t>((pgm_read_byte(font + 1) + 7) / 8) : 0),
      first_(font ? pgm_read_byte(font + 2) : 0),
      last_(font ? pgm_read_byte(font + 3) : 0) {}

uint16_t Packed1bppGlyphSource::recordOffset(uint8_t character) const {
  uint16_t offset = 4;
  for (uint8_t c = first_; c < character; ++c) {
    const uint8_t width = pgm_read_byte(font_ + offset++);
    offset += static_cast<uint16_t>(width) * heightBytes_;
  }
  return offset;
}

bool Packed1bppGlyphSource::glyph(uint8_t character, GlyphPoints& out) {
  out.points.clear();
  if (!font_ || height_ == 0 || heightBytes_ == 0 ||
      character < first_ || character > last_) {
    return false;
  }

  uint16_t offset = recordOffset(character);
  const uint8_t width = pgm_read_byte(font_ + offset++);
  if (width == 0) return false;

  out.points.reserve(static_cast<size_t>(width) * height_ / 3 + 1);
  for (uint8_t x = 0; x < width; ++x) {
    for (uint8_t byte = 0; byte < heightBytes_; ++byte) {
      const uint8_t bits = pgm_read_byte(font_ + offset + x * heightBytes_ + byte);
      for (uint8_t bit = 0; bit < 8; ++bit) {
        if (bits & (1u << bit)) {
          const uint8_t y = static_cast<uint8_t>(byte * 8 + bit);
          if (y < height_) out.points.push_back(Point{x, y});
        }
      }
    }
  }

  out.recomputeBounds();
  return !out.points.empty();
}

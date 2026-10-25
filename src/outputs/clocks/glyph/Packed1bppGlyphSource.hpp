#pragma once

#include <stdint.h>
#include "IGlyphSource.hpp"

// Parser for the packed 1bpp font format used by oled_morphing_clock.
//
// Header: int8_t defaultWidth, uint8_t height, uint8_t firstChar, uint8_t lastChar.
// Each glyph: uint8_t width, then width * ceil(height / 8) column-major bytes.
// Each column byte stores eight vertical pixels, bit 0 at the top.
class Packed1bppGlyphSource : public IGlyphSource {
 public:
  explicit Packed1bppGlyphSource(const uint8_t* font);

  bool glyph(uint8_t character, GlyphPoints& out) override;

 private:
  uint16_t recordOffset(uint8_t character) const;

  const uint8_t* font_;
  uint8_t height_;
  uint8_t heightBytes_;
  uint8_t first_;
  uint8_t last_;
};

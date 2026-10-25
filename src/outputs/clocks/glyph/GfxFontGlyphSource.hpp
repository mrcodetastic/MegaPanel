#pragma once

#include <Arduino.h>
#include <gfxfont.h>
#include "IGlyphSource.hpp"

// Extracts lit pixels from any Adafruit-GFX / GFX_Lite GFXfont.
class GfxFontGlyphSource : public IGlyphSource {
 public:
  explicit GfxFontGlyphSource(const GFXfont* font);

  bool glyph(uint8_t character, GlyphPoints& out) override;

 private:
  const GFXfont* font_;
};

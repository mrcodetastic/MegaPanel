#pragma once

#include "core/PixelCanvas.hpp"
#include "IGlyphSource.hpp"

// Draws a glyph or interpolates between two glyphs inside a target rectangle.
class GlyphMorphEngine {
 public:
  void drawGlyph(PixelCanvas& target,
                 const GlyphPoints& glyph,
                 const Rect& box,
                 CRGB color) const;

  void drawMorph(PixelCanvas& target,
                 const GlyphPoints& from,
                 const GlyphPoints& to,
                 const Rect& box,
                 CRGB color,
                 float progress,
                 bool reverse) const;

 private:
  static Point mapPoint(const Point& point, const GlyphPoints& glyph, const Rect& box);
};

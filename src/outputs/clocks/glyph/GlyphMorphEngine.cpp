#include "GlyphMorphEngine.hpp"

Point GlyphMorphEngine::mapPoint(const Point& point, const GlyphPoints& glyph, const Rect& box) {
  const int16_t glyphW = static_cast<int16_t>(glyph.maxX - glyph.minX);
  const int16_t glyphH = static_cast<int16_t>(glyph.maxY - glyph.minY);
  if (glyphW <= 0) {
    return Point{static_cast<int16_t>(box.centerX()), point.y};
  }
  if (glyphH <= 0) {
    return Point{point.x, static_cast<int16_t>(box.centerY())};
  }

  int32_t x = box.x + (static_cast<int32_t>(point.x - glyph.minX) * (box.w - 1)) / glyphW;
  int32_t y = box.y + (static_cast<int32_t>(point.y - glyph.minY) * (box.h - 1)) / glyphH;
  return Point{static_cast<int16_t>(x), static_cast<int16_t>(y)};
}

void GlyphMorphEngine::drawGlyph(PixelCanvas& target,
                                 const GlyphPoints& glyph,
                                 const Rect& box,
                                 CRGB color) const {
  if (glyph.empty() || box.empty()) return;
  for (const Point& point : glyph.points) {
    const Point mapped = mapPoint(point, glyph, box);
    target.drawPixel(mapped.x, mapped.y, color);
  }
}

void GlyphMorphEngine::drawMorph(PixelCanvas& target,
                                 const GlyphPoints& from,
                                 const GlyphPoints& to,
                                 const Rect& box,
                                 CRGB color,
                                 float progress,
                                 bool reverse) const {
  if (box.empty()) return;
  if (progress <= 0.0f || from.empty()) {
    drawGlyph(target, from, box, color);
    return;
  }
  if (progress >= 1.0f || to.empty()) {
    drawGlyph(target, to, box, color);
    return;
  }

  const size_t fromCount = from.points.size();
  const size_t toCount = to.points.size();
  const size_t count = fromCount > toCount ? fromCount : toCount;
  if (count == 0) return;

  for (size_t i = 0; i < count; ++i) {
    const size_t fromIndex = (i * fromCount) / count;
    size_t toIndex = (i * toCount) / count;
    if (reverse && toCount > 0) toIndex = toCount - 1 - toIndex;

    const Point a = mapPoint(from.points[fromIndex], from, box);
    const Point b = mapPoint(to.points[toIndex], to, box);
    const int16_t x = static_cast<int16_t>(a.x + (b.x - a.x) * progress);
    const int16_t y = static_cast<int16_t>(a.y + (b.y - a.y) * progress);
    target.drawPixel(x, y, color);
  }
}

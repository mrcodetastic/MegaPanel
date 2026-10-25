#pragma once

#include <stdint.h>
#include <vector>
#include "core/Types.hpp"

// A glyph reduced to its lit pixels. Bounds are in glyph-local coordinates.
struct GlyphPoints {
  std::vector<Point> points;
  int16_t minX;
  int16_t minY;
  int16_t maxX;
  int16_t maxY;

  GlyphPoints() : minX(0), minY(0), maxX(-1), maxY(-1) {}

  bool empty() const { return points.empty(); }

  void recomputeBounds() {
    if (points.empty()) {
      minX = minY = 0;
      maxX = maxY = -1;
      return;
    }
    minX = maxX = points[0].x;
    minY = maxY = points[0].y;
    for (const Point& p : points) {
      if (p.x < minX) minX = p.x;
      if (p.x > maxX) maxX = p.x;
      if (p.y < minY) minY = p.y;
      if (p.y > maxY) maxY = p.y;
    }
  }
};

class IGlyphSource {
 public:
  virtual ~IGlyphSource() = default;
  virtual bool glyph(uint8_t character, GlyphPoints& out) = 0;
};

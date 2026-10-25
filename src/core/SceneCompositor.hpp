#pragma once

#include "PixelCanvas.hpp"
#include "Types.hpp"

// Streams the three layers into the HAL framebuffer. It deliberately does not
// allocate a fourth full-screen layer.
class SceneCompositor {
 public:
  void compose(const PixelCanvas* background,
               const PixelCanvas* foreground,
               const PixelCanvas* system,
               const LayerStyle& backgroundStyle,
               const LayerStyle& foregroundStyle,
               const LayerStyle& systemStyle,
               CRGB* destination,
               const ScreenSpec& screen) const;

 private:
  static CRGB blend(CRGB from, CRGB to, uint8_t amount);
  static bool isTransparent(CRGB color, const LayerStyle& style);
};

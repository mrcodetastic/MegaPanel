#include "SceneCompositor.hpp"

CRGB SceneCompositor::blend(CRGB from, CRGB to, uint8_t amount) {
  if (amount == 0) return from;
  if (amount == 255) return to;
  const uint16_t inverse = static_cast<uint16_t>(255u - amount);
  return CRGB(
      static_cast<uint8_t>((static_cast<uint16_t>(from.r) * inverse +
                            static_cast<uint16_t>(to.r) * amount + 127u) / 255u),
      static_cast<uint8_t>((static_cast<uint16_t>(from.g) * inverse +
                            static_cast<uint16_t>(to.g) * amount + 127u) / 255u),
      static_cast<uint8_t>((static_cast<uint16_t>(from.b) * inverse +
                            static_cast<uint16_t>(to.b) * amount + 127u) / 255u));
}

bool SceneCompositor::isTransparent(CRGB color, const LayerStyle& style) {
  if (!style.enabled || style.alpha == 0) return true;
  switch (style.transparency) {
    case LayerTransparency::Opaque:
      return false;
    case LayerTransparency::BlackIsTransparent:
      return color == CRGB::Black;
    case LayerTransparency::ColorKey:
      return color == style.colorKey;
  }
  return false;
}

void SceneCompositor::compose(const PixelCanvas* background,
                              const PixelCanvas* foreground,
                              const PixelCanvas* system,
                              const LayerStyle& backgroundStyle,
                              const LayerStyle& foregroundStyle,
                              const LayerStyle& systemStyle,
                              CRGB* destination,
                              const ScreenSpec& screen) const {
  if (!destination || screen.width == 0 || screen.height == 0) return;

  const bool useBackground = background && background->isInitialized() && backgroundStyle.enabled;
  const bool useForeground = foreground && foreground->isInitialized() && foregroundStyle.enabled;
  const bool useSystem = system && system->isInitialized() && systemStyle.enabled;

  for (uint16_t y = 0; y < screen.height; ++y) {
    for (uint16_t x = 0; x < screen.width; ++x) {
      CRGB color = CRGB::Black;

      if (useBackground) {
        color = blend(CRGB::Black, background->getPixel(x, y), backgroundStyle.alpha);
      }

      if (useForeground) {
        const CRGB fg = foreground->getPixel(x, y);
        if (!isTransparent(fg, foregroundStyle)) {
          color = blend(color, fg, foregroundStyle.alpha);
        }
      }

      if (useSystem) {
        const CRGB sys = system->getPixel(x, y);
        if (!isTransparent(sys, systemStyle)) {
          color = blend(color, sys, systemStyle.alpha);
        }
      }

      destination[static_cast<uint32_t>(y) * screen.width + x] = color;
    }
  }
}

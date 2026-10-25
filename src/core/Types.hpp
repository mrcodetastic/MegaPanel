#pragma once

#include <Arduino.h>
#include <GFX_Lite.h>

// Logical panel size. Outputs must never assume a fixed resolution.
struct ScreenSpec {
  uint16_t width;
  uint16_t height;

  bool isValid() const { return width > 0 && height > 0; }
  bool operator==(const ScreenSpec& other) const {
    return width == other.width && height == other.height;
  }
  bool operator!=(const ScreenSpec& other) const { return !(*this == other); }
};

struct Point {
  int16_t x;
  int16_t y;
};

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;

  int16_t right() const { return static_cast<int16_t>(x + w); }
  int16_t bottom() const { return static_cast<int16_t>(y + h); }
  int16_t centerX() const { return static_cast<int16_t>(x + w / 2); }
  int16_t centerY() const { return static_cast<int16_t>(y + h / 2); }
  bool empty() const { return w <= 0 || h <= 0; }

  bool contains(int16_t px, int16_t py) const {
    return px >= x && py >= y && px < right() && py < bottom();
  }
};

enum class LayerId : uint8_t {
  Background = 0,
  Foreground = 1,
  System = 2
};

enum LayerBits : uint8_t {
  LAYER_BG  = 1u << 0,
  LAYER_FG  = 1u << 1,
  LAYER_SYS = 1u << 2
};

enum class OutputCategory : uint8_t {
  BackgroundPattern,
  ForegroundPattern,
  DisplayOutput,
  SystemOverlay
};

enum class LayerTransparency : uint8_t {
  Opaque,
  BlackIsTransparent,
  ColorKey
};

// Style applied by the compositor to one of the three drawable layers.
struct LayerStyle {
  bool enabled;
  uint8_t alpha;
  LayerTransparency transparency;
  CRGB colorKey;
  bool dimWhenCovered;

  LayerStyle()
      : enabled(true),
        alpha(255),
        transparency(LayerTransparency::Opaque),
        colorKey(CRGB::Black),
        dimWhenCovered(false) {}
};

inline const char* outputCategoryName(OutputCategory category) {
  switch (category) {
    case OutputCategory::BackgroundPattern: return "background";
    case OutputCategory::ForegroundPattern: return "foreground";
    case OutputCategory::DisplayOutput: return "display";
    case OutputCategory::SystemOverlay: return "system";
  }
  return "unknown";
}

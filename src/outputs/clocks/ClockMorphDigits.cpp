#include "ClockMorphDigits.hpp"

#include <Arduino.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/Picopixel.h>

#include "core/FrameContext.hpp"
#include "glyph/GfxFontGlyphSource.hpp"
#include "glyph/Packed1bppGlyphSource.hpp"
#include "glyph/modern_dig_15x32_font.h"
#include "glyph/seg7_dig_15x32_font.h"
#include "util/Logger.hpp"

namespace {

CRGB parseHexColor(const char* text, CRGB fallback) {
  if (!text) return fallback;
  if (*text == '#') ++text;
  if (strlen(text) != 6) return fallback;
  char* end = nullptr;
  const uint32_t value = static_cast<uint32_t>(strtoul(text, &end, 16));
  if (!end || *end != '\0') return fallback;
  return CRGB(static_cast<uint8_t>((value >> 16) & 0xFF),
              static_cast<uint8_t>((value >> 8) & 0xFF),
              static_cast<uint8_t>(value & 0xFF));
}

void colorToHex(CRGB color, char* out, size_t capacity) {
  snprintf(out, capacity, "#%02X%02X%02X", color.r, color.g, color.b);
}

void copyString(char* destination, size_t capacity, const char* source) {
  if (!destination || capacity == 0) return;
  if (!source) {
    destination[0] = '\0';
    return;
  }
  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

}  // namespace

ClockMorphDigits::ClockMorphDigits()
    : screen_{0, 0},
      layout_(),
      color_(CRGB::White),
      morphMs_(320),
      reverse_(false),
      showColon_(true),
      started_(false),
      firstFrame_(true),
      fontName_{0},
      current_{0, 0, 0, 0},
      target_{0, 0, 0, 0},
      morphStart_{0, 0, 0, 0},
      morphing_{false, false, false, false} {
  copyString(fontName_, sizeof(fontName_), "morph_modern");
}

const OutputMeta& ClockMorphDigits::metaStatic() {
  static const OutputMeta meta = {
      "clock.morph",
      "Morphing digits",
      "1.0",
      "Ported from oled_morphing_clock",
      OutputCategory::DisplayOutput,
      LAYER_FG,
      LayerId::Foreground,
      ScreenSpec{0, 0},
      ScreenSpec{32, 32},
      0,
      0,
      30,
      25,
      true,
      true,
      false,
      false,
      false,
      false};
  return meta;
}

void ClockMorphDigits::schemaStatic(JsonObject& out) {
  JsonObject font = out["font"].to<JsonObject>();
  font["type"] = "select";
  JsonArray options = font["options"].to<JsonArray>();
  options.add("morph_modern");
  options.add("morph_seg7");
  options.add("picopixel");
  options.add("sans12");
  font["default"] = "morph_modern";

  JsonObject color = out["color"].to<JsonObject>();
  color["type"] = "color";
  color["default"] = "#FFFFFF";

  JsonObject morph = out["morphMs"].to<JsonObject>();
  morph["type"] = "range";
  morph["min"] = 0;
  morph["max"] = 2000;
  morph["step"] = 10;
  morph["default"] = 320;

  JsonObject reverse = out["reverse"].to<JsonObject>();
  reverse["type"] = "bool";
  reverse["default"] = false;

  JsonObject colon = out["showColon"].to<JsonObject>();
  colon["type"] = "bool";
  colon["default"] = true;
}

bool ClockMorphDigits::configure(const ScreenSpec& screen, JsonObjectConst config) {
  screen_ = screen;
  applyLiveConfig(config);
  return screen_.isValid();
}

void ClockMorphDigits::applyLiveConfig(JsonObjectConst config) {
  copyString(fontName_, sizeof(fontName_), config["font"] | "morph_modern");
  color_ = parseHexColor(config["color"] | "#FFFFFF", CRGB::White);
  morphMs_ = static_cast<uint16_t>(config["morphMs"] | 320);
  if (morphMs_ > 2000) morphMs_ = 2000;
  reverse_ = config["reverse"] | false;
  showColon_ = config["showColon"] | true;
}

bool ClockMorphDigits::start(OutputContext& ctx) {
  (void)ctx;
  stop();

  if (strcmp(fontName_, "morph_seg7") == 0) {
    source_.reset(new Packed1bppGlyphSource(seg7_dig_15x32));
  } else if (strcmp(fontName_, "picopixel") == 0) {
    source_.reset(new GfxFontGlyphSource(&Picopixel));
  } else if (strcmp(fontName_, "sans12") == 0) {
    source_.reset(new GfxFontGlyphSource(&FreeSansBold12pt7b));
  } else {
    source_.reset(new Packed1bppGlyphSource(modern_dig_15x32));
  }

  if (!source_) {
    LOGE("morph", "no glyph source");
    return false;
  }
  if (!buildGlyphs()) {
    LOGE("morph", "failed to build glyphs for %s", fontName_);
    stop();
    return false;
  }

  uint8_t digitH = screen_.height >= 100 ? 40 : (screen_.width < 100 ? 28 : 32);
  uint8_t digitW = static_cast<uint8_t>(digitH / 2);
  if (digitW < 4) digitW = 4;
  layout_ = ClockLayout::choose(screen_, digitW, digitH, 2, false);

  if (layout_.arrangement == ClockArrangement::Custom) {
    const Layout base(screen_, 1);
    layout_.time = base.safe;
    const int16_t halfW = static_cast<int16_t>(base.safe.w / 2);
    const int16_t halfH = static_cast<int16_t>(base.safe.h / 2);
    layout_.digits[0] = Rect{base.safe.x, base.safe.y, halfW, halfH};
    layout_.digits[1] = Rect{static_cast<int16_t>(base.safe.x + halfW), base.safe.y, halfW, halfH};
    layout_.digits[3] = Rect{base.safe.x, static_cast<int16_t>(base.safe.y + halfH), halfW, halfH};
    layout_.digits[4] = Rect{static_cast<int16_t>(base.safe.x + halfW),
                             static_cast<int16_t>(base.safe.y + halfH), halfW, halfH};
    layout_.digits[2] = Rect{base.safe.centerX() - 1, base.safe.centerY() - 1, 2, 2};
  }

  firstFrame_ = true;
  started_ = true;
  LOGI("morph", "started with %s at %ux%u", fontName_, screen_.width, screen_.height);
  return true;
}

bool ClockMorphDigits::buildGlyphs() {
  bool ok = false;
  for (uint8_t i = 0; i < 10; ++i) {
    if (!source_->glyph(static_cast<uint8_t>('0' + i), glyphs_[i])) {
      LOGE("morph", "missing digit %u", i);
    } else {
      ok = true;
    }
  }
  source_->glyph(':', glyphs_[10]);
  return ok;
}

void ClockMorphDigits::stop() {
  source_.reset();
  for (GlyphPoints& glyph : glyphs_) {
    glyph.points.clear();
    glyph.recomputeBounds();
  }
  for (uint8_t i = 0; i < 4; ++i) {
    morphing_[i] = false;
    morphStart_[i] = 0;
  }
  started_ = false;
  firstFrame_ = true;
}

void ClockMorphDigits::drawColon(PixelCanvas& target, const FrameContext& frame) const {
  if (!showColon_) return;
  if (((frame.nowMs / 500u) % 2u) != 0u) return;
  const int16_t cx = layout_.digits[2].centerX();
  const int16_t cy = layout_.time.centerY();
  target.fillRect(static_cast<int16_t>(cx - 1), static_cast<int16_t>(cy - 4), 2, 2, color_);
  target.fillRect(static_cast<int16_t>(cx - 1), static_cast<int16_t>(cy + 2), 2, 2, color_);
}

void ClockMorphDigits::render(PixelCanvas& target, const FrameContext& frame) {
  if (!started_) return;
  target.clear(CRGB::Black);

  if (!frame.timeValid) {
    target.drawTextCentered("SYNC", static_cast<int16_t>(screen_.height / 2 - 4), color_, 1);
    return;
  }

  int hour = frame.localTime.tm_hour;
  if (!frame.use24Hour) {
    hour %= 12;
    if (hour == 0) hour = 12;
  }
  const uint8_t values[4] = {
      static_cast<uint8_t>(hour / 10),
      static_cast<uint8_t>(hour % 10),
      static_cast<uint8_t>(frame.localTime.tm_min / 10),
      static_cast<uint8_t>(frame.localTime.tm_min % 10)};

  if (firstFrame_) {
    for (uint8_t i = 0; i < 4; ++i) {
      current_[i] = values[i];
      target_[i] = values[i];
      morphing_[i] = false;
    }
    firstFrame_ = false;
  }

  const uint8_t slots[4] = {0, 1, 3, 4};
  for (uint8_t i = 0; i < 4; ++i) {
    if (!morphing_[i] && values[i] != current_[i]) {
      target_[i] = values[i];
      morphStart_[i] = frame.nowMs;
      morphing_[i] = true;
      if (morphMs_ == 0) {
        current_[i] = target_[i];
        morphing_[i] = false;
      }
    }

    const Rect& box = layout_.digits[slots[i]];
    if (morphing_[i]) {
      float progress = static_cast<float>(frame.nowMs - morphStart_[i]) / static_cast<float>(morphMs_);
      if (progress >= 1.0f) {
        current_[i] = target_[i];
        morphing_[i] = false;
        progress = 1.0f;
      }
      engine_.drawMorph(target, glyphs_[current_[i]], glyphs_[target_[i]],
                        box, color_, progress, reverse_);
    } else {
      engine_.drawGlyph(target, glyphs_[current_[i]], box, color_);
    }
  }

  drawColon(target, frame);
}

void ClockMorphDigits::onConfigChanged(JsonObjectConst config) {
  applyLiveConfig(config);
}

void ClockMorphDigits::writeConfig(JsonObject& out) const {
  out["font"] = fontName_;
  char hex[8];
  colorToHex(color_, hex, sizeof(hex));
  out["color"] = hex;
  out["morphMs"] = morphMs_;
  out["reverse"] = reverse_;
  out["showColon"] = showColon_;
}

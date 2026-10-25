#pragma once

#include "PatternOutput.hpp"

#define AMBIENT_OUTPUT_META(CLASS, ID, NAME)                                      \
 public:                                                                          \
  const OutputMeta& meta() const override { return metaStatic(); }                \
  static const OutputMeta& metaStatic() {                                         \
    static const OutputMeta m = {ID, NAME, "1.0", "MegaPanel",                    \
                                 OutputCategory::BackgroundPattern,               \
                                 LAYER_BG | LAYER_FG, LayerId::Background,        \
                                 ScreenSpec{0, 0}, ScreenSpec{16, 16}, 0, 0,      \
                                 30, 20, true, false, false, false, false, false}; \
    return m;                                                                     \
  }                                                                               \
  void buildConfigSchema(JsonObject& out) const override { schemaStatic(out); }   \
  static void schemaStatic(JsonObject& out) { PatternOutput::commonSchema(out); }

class AmbientStarfield : public PatternOutput {
  AMBIENT_OUTPUT_META(AmbientStarfield, "pattern.starfield", "Starfield")
 protected:
  void resetPattern() override;
  void releasePattern() override {}
  void drawPattern(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct Star {
    float x;
    float y;
    float z;
    CRGB color;
  };
  static constexpr uint16_t kStarCount = 100;
  Star stars_[kStarCount];
};

class AmbientPlasma : public PatternOutput {
  AMBIENT_OUTPUT_META(AmbientPlasma, "pattern.plasma", "Plasma")
 protected:
  void resetPattern() override;
  void releasePattern() override {}
  void drawPattern(PixelCanvas& target, const FrameContext& frame) override;

 private:
  uint8_t time_;
};

class AmbientRain : public PatternOutput {
  AMBIENT_OUTPUT_META(AmbientRain, "pattern.rain", "Rain")
 protected:
  void resetPattern() override;
  void releasePattern() override {}
  void drawPattern(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct Drop {
    int16_t x;
    int16_t y;
    uint8_t speed;
    CRGB color;
  };
  static constexpr uint16_t kDropCount = 96;
  Drop drops_[kDropCount];
};

class AmbientMatrix : public PatternOutput {
  AMBIENT_OUTPUT_META(AmbientMatrix, "pattern.matrix", "Matrix rain")
 protected:
  void resetPattern() override;
  void releasePattern() override {}
  void drawPattern(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct Column {
    int16_t y;
    uint8_t speed;
    uint8_t length;
  };
  static constexpr uint16_t kMaxColumns = 128;
  Column columns_[kMaxColumns];
  uint16_t columnCount_;
};

class AmbientFire : public PatternOutput {
  AMBIENT_OUTPUT_META(AmbientFire, "pattern.fire", "Fire")
 protected:
  void resetPattern() override;
  void releasePattern() override;
  void drawPattern(PixelCanvas& target, const FrameContext& frame) override;

 private:
  uint8_t* fire_;
  uint16_t width_;
  uint16_t height_;
};

class AmbientWave : public PatternOutput {
  AMBIENT_OUTPUT_META(AmbientWave, "pattern.wave", "Wave")
 protected:
  void resetPattern() override;
  void releasePattern() override {}
  void drawPattern(PixelCanvas& target, const FrameContext& frame) override;

 private:
  uint16_t phase_;
};

class AmbientSpiro : public PatternOutput {
  AMBIENT_OUTPUT_META(AmbientSpiro, "pattern.spiro", "Spiro")
 protected:
  void resetPattern() override;
  void releasePattern() override {}
  void drawPattern(PixelCanvas& target, const FrameContext& frame) override;

 private:
  uint16_t theta_;
  uint8_t hue_;
  uint8_t count_;
};

class AmbientMandala : public PatternOutput {
  AMBIENT_OUTPUT_META(AmbientMandala, "pattern.mandala", "Mandala")
 protected:
  void resetPattern() override;
  void releasePattern() override {}
  void drawPattern(PixelCanvas& target, const FrameContext& frame) override;

 private:
  uint16_t phase_;
};

class AmbientFireworks : public PatternOutput {
  AMBIENT_OUTPUT_META(AmbientFireworks, "pattern.fireworks", "Fireworks")
 protected:
  void resetPattern() override;
  void releasePattern() override {}
  void drawPattern(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct Particle {
    float x;
    float y;
    float vx;
    float vy;
    uint8_t life;
    CRGB color;
  };
  static constexpr uint8_t kFireworks = 3;
  static constexpr uint8_t kParticles = 32;
  Particle particles_[kFireworks][kParticles];
  uint8_t cooldown_[kFireworks];
  void launch(uint8_t index, bool explode);
};

// ---------------------------------------------------------------------------
// Animated ambient scenes ported from animatedpixelclock/ambient.
// ---------------------------------------------------------------------------
class AmbientStars : public PatternOutput {
  AMBIENT_OUTPUT_META(AmbientStars, "ambient.stars", "Layered stars")
 protected:
  void resetPattern() override;
  void releasePattern() override;
  void drawPattern(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct Star {
    float x;
    float y;
    float drift;
    float phase;
    uint8_t layer;
  };
  Star* stars_;
  uint16_t count_;
  bool shooting_;
  float shootingX_;
  float shootingY_;
  float shootingVx_;
  float shootingTimer_;
};

class AmbientInvaders : public PatternOutput {
  AMBIENT_OUTPUT_META(AmbientInvaders, "ambient.invaders", "Invaders patrol")
 protected:
  void resetPattern() override;
  void releasePattern() override {}
  void drawPattern(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct Bomb {
    float x;
    float y;
    float vy;
    bool active;
  };
  float fleetX_;
  float fleetY_;
  float fleetDir_;
  float bobPhase_;
  float bombTimer_;
  Bomb bombs_[4];
};

class AmbientPacmanChase : public PatternOutput {
  AMBIENT_OUTPUT_META(AmbientPacmanChase, "ambient.pacman_chase", "Pac-Man chase")
 protected:
  void resetPattern() override;
  void releasePattern() override {}
  void drawPattern(PixelCanvas& target, const FrameContext& frame) override;

 private:
  float pacX_;
  float pacDir_;
  float pacSpeed_;
  float ghostX_[3];
  float ghostSpeed_[3];
  float pelletPhase_;
};

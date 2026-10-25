#include "AmbientPatterns.hpp"

#include <Arduino.h>
#include <math.h>
#include <new>

#include "core/FrameContext.hpp"

namespace {

int16_t clamp16(int16_t value, int16_t low, int16_t high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

}  // namespace

// ---------------------------------------------------------------------------
// Starfield
// ---------------------------------------------------------------------------
void AmbientStarfield::resetPattern() {
  for (uint16_t i = 0; i < kStarCount; ++i) {
    stars_[i].x = static_cast<float>(random8(0, 64)) - 32.0f;
    stars_[i].y = static_cast<float>(random8(0, 64)) - 32.0f;
    stars_[i].z = static_cast<float>(random8(1, 33));
    stars_[i].color = color_;
  }
}

void AmbientStarfield::drawPattern(PixelCanvas& target, const FrameContext& frame) {
  (void)frame;
  target.dim(220);

  const int16_t cx = static_cast<int16_t>(screen_.width / 2);
  const int16_t cy = static_cast<int16_t>(screen_.height / 2);
  const float step = 0.18f + static_cast<float>(speed_) / 320.0f;

  for (uint16_t i = 0; i < kStarCount; ++i) {
    Star& star = stars_[i];
    star.z -= step;
    if (star.z <= 0.5f) {
      star.x = static_cast<float>(random8(0, 64)) - 32.0f;
      star.y = static_cast<float>(random8(0, 64)) - 32.0f;
      star.z = 32.0f;
      star.color = color_;
    }

    const float k = (static_cast<float>(screen_.width < screen_.height ? screen_.width : screen_.height) / 2.0f) / star.z;
    const int16_t x = static_cast<int16_t>(cx + star.x * k / 32.0f);
    const int16_t y = static_cast<int16_t>(cy + star.y * k / 32.0f);
    if (x < 0 || y < 0 || x >= screen_.width || y >= screen_.height) continue;

    uint8_t brightness = static_cast<uint8_t>(255.0f - star.z * 6.0f);
    if (brightness < 16) brightness = 16;
    CRGB c = star.color;
    c.nscale8(brightness);
    target.drawPixel(x, y, c);
  }
}

// ---------------------------------------------------------------------------
// Plasma
// ---------------------------------------------------------------------------
void AmbientPlasma::resetPattern() { time_ = random8(); }

void AmbientPlasma::drawPattern(PixelCanvas& target, const FrameContext& frame) {
  (void)frame;
  for (uint16_t y = 0; y < screen_.height; ++y) {
    for (uint16_t x = 0; x < screen_.width; ++x) {
      const uint8_t a = sin8(static_cast<uint8_t>(x * 3) + time_);
      const uint8_t b = cos8(static_cast<uint8_t>(y * 4) - time_);
      const uint8_t c = sin8(static_cast<uint8_t>((x + y) * 2) + static_cast<uint8_t>(time_ / 2));
      const uint8_t value = static_cast<uint8_t>(a + b + c);
      target.drawPixel(x, y, ColorFromPalette(RainbowColors_p, value));
    }
  }
  time_ = static_cast<uint8_t>(time_ + speed_ / 8 + 1);
}

// ---------------------------------------------------------------------------
// Rain
// ---------------------------------------------------------------------------
void AmbientRain::resetPattern() {
  for (uint16_t i = 0; i < kDropCount; ++i) {
    drops_[i].x = static_cast<int16_t>(random8(0, static_cast<uint8_t>(screen_.width)));
    drops_[i].y = static_cast<int16_t>(random8(0, static_cast<uint8_t>(screen_.height)));
    drops_[i].speed = random8(2, 7);
    drops_[i].color = color_;
  }
}

void AmbientRain::drawPattern(PixelCanvas& target, const FrameContext& frame) {
  (void)frame;
  target.dim(200);
  for (uint16_t i = 0; i < kDropCount; ++i) {
    Drop& drop = drops_[i];
    target.drawPixel(drop.x, drop.y, drop.color);
    CRGB tail = drop.color;
    tail.nscale8(96);
    target.drawPixel(drop.x, static_cast<int16_t>(drop.y - 1), tail);
    drop.y = static_cast<int16_t>(drop.y + drop.speed);
    if (drop.y >= screen_.height) {
      drop.x = static_cast<int16_t>(random8(0, static_cast<uint8_t>(screen_.width)));
      drop.y = static_cast<int16_t>(-random8(0, 8));
      drop.speed = random8(2, 7);
      drop.color = color_;
    }
  }
}

// ---------------------------------------------------------------------------
// Matrix rain
// ---------------------------------------------------------------------------
void AmbientMatrix::resetPattern() {
  columnCount_ = screen_.width < kMaxColumns ? screen_.width : kMaxColumns;
  for (uint16_t i = 0; i < columnCount_; ++i) {
    columns_[i].y = static_cast<int16_t>(random8(0, static_cast<uint8_t>(screen_.height)));
    columns_[i].speed = random8(1, 4);
    columns_[i].length = static_cast<uint8_t>(random8(4, 14));
  }
}

void AmbientMatrix::drawPattern(PixelCanvas& target, const FrameContext& frame) {
  (void)frame;
  target.dim(180);
  for (uint16_t x = 0; x < columnCount_; ++x) {
    Column& column = columns_[x];
    const uint8_t length = column.length == 0 ? 1 : column.length;
    for (uint8_t i = 0; i < length; ++i) {
      const int16_t y = static_cast<int16_t>(column.y - i);
      if (y < 0 || y >= screen_.height) continue;
      CRGB c = color_;
      const uint8_t fade = static_cast<uint8_t>(255 - (i * 200) / length);
      c.nscale8(fade);
      target.drawPixel(x, y, c);
    }
    column.y = static_cast<int16_t>(column.y + column.speed);
    if (column.y - length > screen_.height) {
      column.y = static_cast<int16_t>(-random8(0, 12));
      column.length = static_cast<uint8_t>(random8(4, 14));
      column.speed = random8(1, 4);
    }
  }
}

// ---------------------------------------------------------------------------
// Fire
// ---------------------------------------------------------------------------
void AmbientFire::resetPattern() {
  releasePattern();
  width_ = screen_.width;
  height_ = screen_.height;
  if (width_ == 0 || height_ == 0) return;
  fire_ = new (std::nothrow) uint8_t[static_cast<size_t>(width_) * height_]();
}

void AmbientFire::releasePattern() {
  delete[] fire_;
  fire_ = nullptr;
  width_ = 0;
  height_ = 0;
}

void AmbientFire::drawPattern(PixelCanvas& target, const FrameContext& frame) {
  (void)frame;
  if (!fire_ || width_ == 0 || height_ == 0) return;

  for (uint16_t x = 0; x < width_; ++x) {
    fire_[static_cast<size_t>(height_ - 1) * width_ + x] = random8();
  }

  for (int16_t y = static_cast<int16_t>(height_ - 2); y >= 0; --y) {
    for (uint16_t x = 0; x < width_; ++x) {
      const uint16_t below = fire_[static_cast<size_t>(y + 1) * width_ + x];
      const uint16_t left = x > 0 ? fire_[static_cast<size_t>(y + 1) * width_ + (x - 1)] : below;
      const uint16_t right = x + 1 < width_ ? fire_[static_cast<size_t>(y + 1) * width_ + (x + 1)] : below;
      uint16_t value = static_cast<uint16_t>((below * 2 + left + right) / 4);
      if (value > 3) value -= random8(0, 4);
      fire_[static_cast<size_t>(y) * width_ + x] = static_cast<uint8_t>(value);
    }
  }

  for (uint16_t y = 0; y < height_; ++y) {
    for (uint16_t x = 0; x < width_; ++x) {
      const uint8_t value = fire_[static_cast<size_t>(y) * width_ + x];
      target.drawPixel(x, y, ColorFromPalette(HeatColors_p, value));
    }
  }
}

// ---------------------------------------------------------------------------
// Wave
// ---------------------------------------------------------------------------
void AmbientWave::resetPattern() { phase_ = 0; }

void AmbientWave::drawPattern(PixelCanvas& target, const FrameContext& frame) {
  (void)frame;
  target.clear(CRGB::Black);
  const int16_t center = static_cast<int16_t>(screen_.height / 2);
  for (uint16_t x = 0; x < screen_.width; ++x) {
    const uint8_t value = sin8(static_cast<uint8_t>(x * 4) + static_cast<uint8_t>(phase_));
    const int16_t offset = static_cast<int16_t>((static_cast<int16_t>(value) - 128) * screen_.height / 512);
    const int16_t y = clamp16(static_cast<int16_t>(center + offset), 0,
                              static_cast<int16_t>(screen_.height - 1));
    target.drawPixel(x, y, color_);
    if (y + 1 < screen_.height) target.drawPixel(x, static_cast<int16_t>(y + 1), color_);
  }
  phase_ = static_cast<uint16_t>(phase_ + speed_ / 2 + 1);
}

// ---------------------------------------------------------------------------
// Spiro
// ---------------------------------------------------------------------------
void AmbientSpiro::resetPattern() {
  theta_ = 0;
  hue_ = 0;
  count_ = 1;
}

void AmbientSpiro::drawPattern(PixelCanvas& target, const FrameContext& frame) {
  (void)frame;
  target.dim(246);
  theta_ = static_cast<uint16_t>(theta_ + speed_ / 4 + 1);
  ++hue_;

  const int16_t cx = static_cast<int16_t>(screen_.width / 2);
  const int16_t cy = static_cast<int16_t>(screen_.height / 2);
  const int16_t radius = static_cast<int16_t>((screen_.width < screen_.height ? screen_.width : screen_.height) / 3);
  const uint8_t count = count_ == 0 ? 1 : count_;

  for (uint8_t i = 0; i < count; ++i) {
    const uint8_t angle = static_cast<uint8_t>(theta_ + (i * 256u) / count);
    const int16_t x = static_cast<int16_t>(cx + (static_cast<int16_t>(sin8(angle)) - 128) * radius / 128);
    const int16_t y = static_cast<int16_t>(cy + (static_cast<int16_t>(cos8(angle)) - 128) * radius / 128);
    target.drawPixel(x, y, ColorFromPalette(RainbowColors_p, static_cast<uint8_t>(hue_ + i * 24)));
  }

  if ((theta_ & 0x01FFu) == 0) {
    if (count_ >= 8) {
      count_ = 1;
    } else {
      ++count_;
    }
  }
}

// ---------------------------------------------------------------------------
// Mandala
// ---------------------------------------------------------------------------
void AmbientMandala::resetPattern() { phase_ = 0; }

void AmbientMandala::drawPattern(PixelCanvas& target, const FrameContext& frame) {
  (void)frame;
  const int16_t cx = static_cast<int16_t>(screen_.width / 2);
  const int16_t cy = static_cast<int16_t>(screen_.height / 2);
  for (uint16_t y = 0; y < screen_.height; ++y) {
    for (uint16_t x = 0; x < screen_.width; ++x) {
      const int16_t dx = static_cast<int16_t>(x) - cx;
      const int16_t dy = static_cast<int16_t>(y) - cy;
      const uint8_t a = sin8(static_cast<uint8_t>(dx * 3) + static_cast<uint8_t>(phase_));
      const uint8_t b = cos8(static_cast<uint8_t>(dy * 3) - static_cast<uint8_t>(phase_));
      const uint8_t c = sin8(static_cast<uint8_t>((dx + dy) * 2) - static_cast<uint8_t>(phase_ / 2));
      target.drawPixel(x, y, ColorFromPalette(RainbowColors_p, static_cast<uint8_t>(a + b + c)));
    }
  }
  phase_ = static_cast<uint16_t>(phase_ + speed_ / 8 + 1);
}

// ---------------------------------------------------------------------------
// Fireworks
// ---------------------------------------------------------------------------
void AmbientFireworks::resetPattern() {
  for (uint8_t i = 0; i < kFireworks; ++i) {
    cooldown_[i] = static_cast<uint8_t>(random8(0, 40));
    for (uint8_t p = 0; p < kParticles; ++p) {
      particles_[i][p].life = 0;
    }
  }
}

void AmbientFireworks::launch(uint8_t index, bool explode) {
  (void)explode;
  const int16_t cx = static_cast<int16_t>(random8(8, static_cast<uint8_t>(screen_.width > 16 ? screen_.width - 8 : screen_.width)));
  const int16_t cy = static_cast<int16_t>(random8(8, static_cast<uint8_t>(screen_.height > 24 ? screen_.height / 2 : screen_.height)));
  const uint8_t hue = random8();
  for (uint8_t p = 0; p < kParticles; ++p) {
    const float angle = static_cast<float>(random8(0, 256)) * 6.2831853f / 256.0f;
    const float speed = 0.35f + static_cast<float>(random8(0, 100)) / 160.0f;
    Particle& particle = particles_[index][p];
    particle.x = static_cast<float>(cx);
    particle.y = static_cast<float>(cy);
    particle.vx = cosf(angle) * speed;
    particle.vy = sinf(angle) * speed;
    particle.life = static_cast<uint8_t>(28 + random8(0, 35));
    particle.color = CHSV(static_cast<uint8_t>(hue + random8(0, 24)), 255, 255);
  }
}

void AmbientFireworks::drawPattern(PixelCanvas& target, const FrameContext& frame) {
  (void)frame;
  target.dim(220);

  for (uint8_t i = 0; i < kFireworks; ++i) {
    bool anyAlive = false;
    for (uint8_t p = 0; p < kParticles; ++p) {
      Particle& particle = particles_[i][p];
      if (particle.life == 0) continue;
      anyAlive = true;
      particle.x += particle.vx;
      particle.y += particle.vy;
      particle.vy += 0.045f;
      --particle.life;
      CRGB c = particle.color;
      c.nscale8(static_cast<uint8_t>(32 + particle.life * 5));
      target.drawPixel(static_cast<int16_t>(particle.x), static_cast<int16_t>(particle.y), c);
    }

    if (!anyAlive) {
      if (cooldown_[i] > 0) {
        --cooldown_[i];
      } else {
        launch(i, true);
        cooldown_[i] = static_cast<uint8_t>(random8(20, 80));
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Layered stars (animatedpixelclock ambient_stars.cpp)
// ---------------------------------------------------------------------------
void AmbientStars::resetPattern() {
  const uint32_t area = static_cast<uint32_t>(screen_.width) * screen_.height;
  uint16_t count = static_cast<uint16_t>(area / 28u);
  if (count < 48) count = 48;
  if (count > 160) count = 160;

  delete[] stars_;
  stars_ = new (std::nothrow) Star[count];
  count_ = stars_ ? count : 0;
  for (uint16_t i = 0; i < count_; ++i) {
    stars_[i].x = static_cast<float>(random8(static_cast<uint8_t>(screen_.width)));
    stars_[i].y = static_cast<float>(random8(static_cast<uint8_t>(screen_.height)));
    stars_[i].layer = static_cast<uint8_t>(random8(3));
    stars_[i].drift = 2.0f + static_cast<float>(stars_[i].layer) * 5.0f;
    stars_[i].phase = static_cast<float>(random16()) / 4096.0f;
  }
  shooting_ = false;
  shootingX_ = 0.0f;
  shootingY_ = 0.0f;
  shootingVx_ = 0.0f;
  shootingTimer_ = 2.0f;
}

void AmbientStars::releasePattern() {
  delete[] stars_;
  stars_ = nullptr;
  count_ = 0;
}

void AmbientStars::drawPattern(PixelCanvas& target, const FrameContext& frame) {
  const float dt = frame.deltaSec > 0.0f ? frame.deltaSec : 0.02f;
  const float speedScale = 0.3f + static_cast<float>(speed_) * 0.14f;
  const float now = static_cast<float>(frame.nowMs) * 0.002f;

  for (uint16_t i = 0; i < count_; ++i) {
    Star& star = stars_[i];
    star.y += star.drift * speedScale * dt;
    if (star.y >= static_cast<float>(screen_.height)) {
      star.y -= static_cast<float>(screen_.height);
      star.x = static_cast<float>(random8(static_cast<uint8_t>(screen_.width)));
    }
    const float twinkle =
        0.45f + 0.55f * fabsf(sinf(now * (1.0f + static_cast<float>(star.layer) * 0.35f) + star.phase));
    CRGB c = color_;
    c.nscale8(static_cast<uint8_t>(twinkle * 255.0f));
    target.drawPixel(static_cast<int16_t>(star.x), static_cast<int16_t>(star.y), c);
    if (star.layer == 2) {
      target.drawPixel(static_cast<int16_t>(star.x + 1.0f), static_cast<int16_t>(star.y), c);
    }
  }

  shootingTimer_ -= dt;
  if (!shooting_ && shootingTimer_ <= 0.0f) {
    shooting_ = true;
    shootingX_ = static_cast<float>(random8(static_cast<uint8_t>(screen_.width / 2)));
    shootingY_ = 0.0f;
    shootingVx_ = 60.0f + static_cast<float>(random8(40));
    shootingTimer_ = 6.0f + static_cast<float>(random8(50)) * 0.1f;
  }
  if (shooting_) {
    shootingX_ += shootingVx_ * dt;
    shootingY_ += shootingVx_ * 0.45f * dt;
    target.drawPixel(static_cast<int16_t>(shootingX_), static_cast<int16_t>(shootingY_),
                     CRGB(255, 255, 255));
    target.drawPixel(static_cast<int16_t>(shootingX_ - 2.0f),
                     static_cast<int16_t>(shootingY_ - 1.0f), color_);
    if (shootingX_ > static_cast<float>(screen_.width) ||
        shootingY_ > static_cast<float>(screen_.height)) {
      shooting_ = false;
    }
  }
}

// ---------------------------------------------------------------------------
// Invaders patrol (animatedpixelclock ambient_invaders.cpp)
// ---------------------------------------------------------------------------
namespace {

const char* const kAmbientInvader[5] = {
    "..###..",
    "#######",
    "##.#.##",
    "#######",
    ".#.#.#.",
};

void drawAmbientSprite(PixelCanvas& target, int16_t x, int16_t y, const char* const* rows,
                       uint8_t rowCount, CRGB color) {
  for (uint8_t row = 0; row < rowCount; ++row) {
    for (int16_t col = 0; rows[row][col]; ++col) {
      if (rows[row][col] == '#') {
        target.drawPixel(static_cast<int16_t>(x + col), static_cast<int16_t>(y + row), color);
      }
    }
  }
}

}  // namespace

void AmbientInvaders::resetPattern() {
  fleetX_ = 2.0f;
  fleetY_ = 2.0f;
  fleetDir_ = 1.0f;
  bobPhase_ = 0.0f;
  bombTimer_ = 1.5f;
  for (uint8_t i = 0; i < 4; ++i) {
    bombs_[i].active = false;
    bombs_[i].x = 0.0f;
    bombs_[i].y = 0.0f;
    bombs_[i].vy = 0.0f;
  }
}

void AmbientInvaders::drawPattern(PixelCanvas& target, const FrameContext& frame) {
  const float dt = frame.deltaSec > 0.0f ? frame.deltaSec : 0.02f;
  const float pace = 3.0f + static_cast<float>(speed_) * 0.9f;
  const float fleetWidth = 30.0f;

  fleetX_ += fleetDir_ * pace * dt;
  if (fleetX_ < 1.0f) {
    fleetX_ = 1.0f;
    fleetDir_ = 1.0f;
  } else if (fleetX_ + fleetWidth > static_cast<float>(screen_.width) - 1.0f) {
    fleetX_ = static_cast<float>(screen_.width) - 1.0f - fleetWidth;
    fleetDir_ = -1.0f;
  }
  bobPhase_ += dt * 2.4f;
  const float bob = sinf(bobPhase_) * 1.5f;

  const bool flip = (frame.nowMs / 460u) % 2u == 0u;
  for (uint8_t row = 0; row < 2; ++row) {
    for (uint8_t col = 0; col < 4; ++col) {
      if (row == 1 && col == 3) continue;
      const int16_t x = static_cast<int16_t>(fleetX_ + static_cast<float>(col) * 8.0f);
      const int16_t y = static_cast<int16_t>(fleetY_ + bob + static_cast<float>(row) * 7.0f);
      drawAmbientSprite(target, x, y + (flip ? 0 : 1), kAmbientInvader, 5, color_);
    }
  }

  bombTimer_ -= dt;
  if (bombTimer_ <= 0.0f) {
    for (uint8_t i = 0; i < 4; ++i) {
      if (bombs_[i].active) continue;
      bombs_[i].active = true;
      bombs_[i].x = fleetX_ + static_cast<float>(random8(4)) * 8.0f + 3.0f;
      bombs_[i].y = fleetY_ + bob + 14.0f;
      bombs_[i].vy = 22.0f + static_cast<float>(random8(20));
      break;
    }
    bombTimer_ = 1.2f + static_cast<float>(random8(30)) * 0.1f;
  }

  for (uint8_t i = 0; i < 4; ++i) {
    Bomb& bomb = bombs_[i];
    if (!bomb.active) continue;
    bomb.y += bomb.vy * dt;
    target.drawPixel(static_cast<int16_t>(bomb.x), static_cast<int16_t>(bomb.y), CRGB(255, 120, 90));
    if (bomb.y > static_cast<float>(screen_.height)) bomb.active = false;
  }
}

// ---------------------------------------------------------------------------
// Pac-Man chase (animatedpixelclock ambient_pacman_chase.cpp)
// ---------------------------------------------------------------------------
namespace {

const char* const kAmbientGhost[7] = {
    ".#####.",
    "#######",
    "#..#..#",
    "#######",
    "#######",
    "#######",
    "#.#.#.#",
};

}  // namespace

void AmbientPacmanChase::resetPattern() {
  pacX_ = -4.0f;
  pacDir_ = 1.0f;
  pacSpeed_ = 9.0f;
  pelletPhase_ = 0.0f;
  for (uint8_t i = 0; i < 3; ++i) {
    ghostX_[i] = -10.0f - static_cast<float>(i) * 6.0f;
    ghostSpeed_[i] = 6.0f + static_cast<float>(i) * 1.5f;
  }
}

void AmbientPacmanChase::drawPattern(PixelCanvas& target, const FrameContext& frame) {
  const float dt = frame.deltaSec > 0.0f ? frame.deltaSec : 0.02f;
  const float pace = 5.0f + static_cast<float>(speed_) * 1.4f;
  const float width = static_cast<float>(screen_.width);
  const int16_t rowY = static_cast<int16_t>(screen_.height / 2);
  const float wrapAt = width + 6.0f;

  pacX_ += pacDir_ * pace * dt;
  if (pacX_ > wrapAt) pacX_ = -4.0f;

  for (uint8_t i = 0; i < 3; ++i) {
    ghostX_[i] += ghostSpeed_[i] * dt;
    if (ghostX_[i] > wrapAt) ghostX_[i] = -8.0f;
  }

  pelletPhase_ += pace * dt;
  if (pelletPhase_ > 4.0f) pelletPhase_ -= 4.0f;
  for (int16_t x = 0; x < static_cast<int16_t>(width); ++x) {
    const int16_t offset = static_cast<int16_t>(fmodf(static_cast<float>(x) + pelletPhase_, 8.0f));
    if (offset != 0) continue;
    target.drawPixel(x, static_cast<int16_t>(rowY + 5), CRGB(255, 230, 200));
  }

  const int16_t pacX = static_cast<int16_t>(pacX_);
  const CRGB black(0, 0, 0);
  target.fillCircle(pacX, rowY, 3, color_);
  const int16_t mouth = static_cast<int16_t>(1.0f + fabsf(sinf(static_cast<float>(frame.nowMs) * 0.011f)) * 3.0f);
  target.fillTriangle(pacX, rowY, static_cast<int16_t>(pacX + 5), static_cast<int16_t>(rowY - mouth),
                      static_cast<int16_t>(pacX + 5), static_cast<int16_t>(rowY + mouth), black);

  for (uint8_t i = 0; i < 3; ++i) {
    const int16_t gx = static_cast<int16_t>(ghostX_[i]);
    drawAmbientSprite(target, gx, static_cast<int16_t>(rowY - 3), kAmbientGhost, 7,
                      CRGB(240 - i * 40, 80 + i * 20, 90 + i * 30));
  }
}

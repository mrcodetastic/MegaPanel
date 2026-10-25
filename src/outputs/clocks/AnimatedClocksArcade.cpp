#include "AnimatedClocks.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "core/FrameContext.hpp"

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kTwoPi = 6.28318531f;

float clampf(float value, float low, float high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

float wrapf(float value, float low, float high) {
  const float span = high - low;
  if (span <= 0.0f) return low;
  while (value < low) value += span;
  while (value >= high) value -= span;
  return value;
}

// Spring used by the digit bounce of Mario, Pong and Bomberman. -1 marks an
// idle slot.
void springStep(int8_t* offset, float* velocity, float dt) {
  if (*offset == 0 && *velocity == 0.0f) return;
  *velocity += 70.0f * dt;
  float value = static_cast<float>(*offset) + *velocity * dt;
  if (value >= 0.0f) {
    value = 0.0f;
    *velocity = 0.0f;
  }
  *offset = static_cast<int8_t>(value);
}

void kickDigit(int8_t* offset, float* velocity, uint8_t index, uint8_t changed, float strength) {
  if (index != changed) return;
  *offset = static_cast<int8_t>(-strength);
  *velocity = 0.0f;
}

const char* const kMarioStand[8] = {
    "..RRR...",
    ".RRRRR..",
    "..SSS...",
    ".SBSBS..",
    "..SSS...",
    "..BBB...",
    ".BB.BB..",
    "BB...BB.",
};

const char* const kMarioJump[8] = {
    "..RRR...",
    ".RRRRR..",
    "S.SSS.S.",
    ".SSSSS..",
    "..BBB...",
    ".B...B..",
    "B.....B.",
    "........",
};

const char* const kGoomba[6] = {
    ".BBBBB.",
    "BBBBBBB",
    "BWB.BWB",
    "BBBBBBB",
    ".B.B.B.",
    "BB.B.BB",
};

const char* const kInvaderA[6] = {
    "..W.W..",
    "...W...",
    "..WWW..",
    ".WW.WW.",
    "WWW.WWW",
    "W.....W",
};

const char* const kInvaderB[6] = {
    "..W.W..",
    "W..W..W",
    "W.WWW.W",
    "WWW.WWW",
    ".W...W.",
    "..W.W..",
};

const char* const kGhost[7] = {
    ".GGGG.",
    "GGGGGG",
    "GWWGGW",
    "GWBGBW",
    "GGGGGG",
    "GGGGGG",
    "G.GG.G",
};

const char* const kBomber[7] = {
    ".HHH..",
    "HWWWH.",
    "HWBWH.",
    ".HHH..",
    ".BBB..",
    "BBBBB.",
    ".B.B..",
};

void drawCircleOutline(PixelCanvas& target, int16_t cx, int16_t cy, int16_t radius, CRGB color) {
  int16_t x = radius;
  int16_t y = 0;
  int16_t err = 1 - radius;
  while (x >= y) {
    target.drawPixel(static_cast<int16_t>(cx + x), static_cast<int16_t>(cy + y), color);
    target.drawPixel(static_cast<int16_t>(cx + y), static_cast<int16_t>(cy + x), color);
    target.drawPixel(static_cast<int16_t>(cx - y), static_cast<int16_t>(cy + x), color);
    target.drawPixel(static_cast<int16_t>(cx - x), static_cast<int16_t>(cy + y), color);
    target.drawPixel(static_cast<int16_t>(cx - x), static_cast<int16_t>(cy - y), color);
    target.drawPixel(static_cast<int16_t>(cx - y), static_cast<int16_t>(cy - x), color);
    target.drawPixel(static_cast<int16_t>(cx + y), static_cast<int16_t>(cy - x), color);
    target.drawPixel(static_cast<int16_t>(cx + x), static_cast<int16_t>(cy - y), color);
    ++y;
    if (err < 0) {
      err += 2 * y + 1;
    } else {
      --x;
      err += 2 * (y - x) + 1;
    }
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// Mario
// ---------------------------------------------------------------------------
struct ClockMario::State {
  float x;
  float dir;
  float goombaX;
  float goombaDir;
  float jumpY;
  float jumpV;
  bool jumping;
  float legTimer;
  bool legFrame;
  float pipeX;
  int8_t offset[5];
  float offsetV[5];
};

ClockMario::ClockMario() : state_(nullptr) {}

void ClockMario::resetClock() {
  if (!state_) state_ = new (std::nothrow) State();
  if (!state_) return;
  State& s = *state_;
  s.x = 4.0f;
  s.dir = 1.0f;
  s.goombaX = static_cast<float>(screen_.width) * 0.6f;
  s.goombaDir = -1.0f;
  s.jumpY = 0.0f;
  s.jumpV = 0.0f;
  s.jumping = false;
  s.legTimer = 0.0f;
  s.legFrame = false;
  s.pipeX = static_cast<float>(screen_.width) * 0.78f;
  for (uint8_t i = 0; i < 5; ++i) {
    s.offset[i] = 0;
    s.offsetV[i] = 0.0f;
  }
}

void ClockMario::releaseClock() {
  delete state_;
  state_ = nullptr;
}

void ClockMario::updateClock(const FrameContext& frame, float dt) {
  (void)frame;
  if (!state_) return;
  State& s = *state_;
  dt = clampf(dt, 0.0f, 0.1f);
  const float pace = 10.0f + static_cast<float>(speed_) * 2.4f;

  s.x += s.dir * pace * dt;
  if (s.x > static_cast<float>(screen_.width) - 10.0f) {
    s.x = static_cast<float>(screen_.width) - 10.0f;
    s.dir = -1.0f;
  }
  if (s.x < 2.0f) {
    s.x = 2.0f;
    s.dir = 1.0f;
  }

  s.goombaX += s.goombaDir * (pace * 0.45f) * dt;
  if (s.goombaX < 2.0f || s.goombaX > static_cast<float>(screen_.width) - 8.0f) {
    s.goombaDir = -s.goombaDir;
    s.goombaX = clampf(s.goombaX, 2.0f, static_cast<float>(screen_.width) - 8.0f);
  }

  if (minuteChanged()) {
    s.jumping = true;
    s.jumpV = -34.0f;
  }
  if (s.jumping) {
    s.jumpY += s.jumpV * dt;
    s.jumpV += 120.0f * dt;
    if (s.jumpY >= 0.0f) {
      s.jumpY = 0.0f;
      s.jumpV = 0.0f;
      s.jumping = false;
    }
  }

  s.legTimer += dt;
  if (s.legTimer > 0.22f) {
    s.legTimer = 0.0f;
    s.legFrame = !s.legFrame;
  }

  for (uint8_t i = 0; i < 5; ++i) {
    kickDigit(&s.offset[i], &s.offsetV[i], i, changedIndex(), 5.0f);
    springStep(&s.offset[i], &s.offsetV[i], dt);
  }
}

void ClockMario::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!state_) return;
  if (!haveTime()) {
    drawSyncNotice(target, frame);
    return;
  }
  State& s = *state_;
  const int16_t width = static_cast<int16_t>(screen_.width);
  const int16_t height = static_cast<int16_t>(screen_.height);
  const int16_t groundY = static_cast<int16_t>(height - 2);
  const int16_t sceneTop = static_cast<int16_t>(digitY() + digitHeight());

  const CRGB ground(0, 140, 60);
  const CRGB brick(180, 80, 40);
  const CRGB leaf(0, 200, 80);
  const CRGB coin(255, 200, 0);
  const CRGB red(220, 30, 30);
  const CRGB skin(255, 190, 150);
  const CRGB blue(40, 80, 220);
  const CRGB white(255, 255, 255);
  const CRGB brown(140, 90, 40);
  const char* const marioKeys = "SB";
  const CRGB marioColors[2] = {skin, blue};
  const char* const goombaKeys = "BWB";
  const CRGB goombaColors[3] = {brown, brown, white};

  target.drawFastHLine(0, groundY, width, ground);
  for (int16_t gx = 0; gx < width; gx += 8) {
    target.drawPixel(gx, static_cast<int16_t>(groundY - 1), ground);
  }

  // Pipe
  const int16_t pipeTop = static_cast<int16_t>(groundY - 6);
  target.fillRect(static_cast<int16_t>(s.pipeX), pipeTop, 5, 6, leaf);
  target.fillRect(static_cast<int16_t>(s.pipeX - 1), static_cast<int16_t>(pipeTop - 2), 7, 2, leaf);
  target.drawFastVLine(static_cast<int16_t>(s.pipeX + 1), static_cast<int16_t>(pipeTop + 1), 4, brick);

  // Goomba
  if (sceneTop + 8 < groundY) {
    drawSprite(target, static_cast<int16_t>(s.goombaX), static_cast<int16_t>(groundY - 5), kGoomba, 6,
               goombaKeys, goombaColors, 3);
  }

  // Mario
  const int16_t marioY = static_cast<int16_t>(groundY - 7 + s.jumpY);
  const char* const* marioRows = s.jumping ? kMarioJump : kMarioStand;
  drawSolidSprite(target, static_cast<int16_t>(s.x), marioY, marioRows, 8, red);
  drawSprite(target, static_cast<int16_t>(s.x), marioY, marioRows, 8, marioKeys, marioColors, 2);

  // Coin pops out of the jump
  if (s.jumping) {
    const int16_t coinY = static_cast<int16_t>(marioY - 6 + (s.jumpY < -10.0f ? 0 : 3));
    drawCircleOutline(target, static_cast<int16_t>(s.x + 4), coinY, 2, coin);
  }

  drawDigits(target, plates_, s.offset);
  if (showDate_) drawDateRow(target, frame, CRGB(150, 150, 150));
}

// ---------------------------------------------------------------------------
// Space invaders
// ---------------------------------------------------------------------------
struct ClockSpace::State {
  float fleetX;
  float fleetY;
  float fleetDir;
  float shipX;
  float shadowX;
  bool laserActive;
  float laserX;
  float laserY;
  uint32_t nextAutoShotMs;
  struct Fragment {
    float x;
    float y;
    float vy;
    bool active;
  } fragments[8];
};

ClockSpace::ClockSpace() : state_(nullptr) {}

void ClockSpace::resetClock() {
  if (!state_) state_ = new (std::nothrow) State();
  if (!state_) return;
  State& s = *state_;
  s.fleetX = 4.0f;
  s.fleetY = static_cast<float>(digitY() + digitHeight() + 2);
  s.fleetDir = 1.0f;
  s.shipX = static_cast<float>(screen_.width) * 0.5f;
  s.shadowX = s.shipX;
  s.laserActive = false;
  s.laserX = s.shipX;
  s.laserY = static_cast<float>(screen_.height);
  s.nextAutoShotMs = 0;
  for (uint8_t i = 0; i < 8; ++i) s.fragments[i].active = false;
}

void ClockSpace::releaseClock() {
  delete state_;
  state_ = nullptr;
}

void ClockSpace::updateClock(const FrameContext& frame, float dt) {
  if (!state_) return;
  State& s = *state_;
  dt = clampf(dt, 0.0f, 0.1f);
  const float width = static_cast<float>(screen_.width);
  const float pace = 6.0f + static_cast<float>(speed_) * 1.6f;

  s.fleetX += s.fleetDir * pace * dt;
  const float fleetWidth = 3 * 10.0f;
  if (s.fleetX < 1.0f) {
    s.fleetX = 1.0f;
    s.fleetDir = 1.0f;
    s.fleetY += 2.0f;
  } else if (s.fleetX + fleetWidth > width - 1.0f) {
    s.fleetX = width - 1.0f - fleetWidth;
    s.fleetDir = -1.0f;
    s.fleetY += 2.0f;
  }
  const float maxFleetY = static_cast<float>(screen_.height) * 0.78f;
  if (s.fleetY > maxFleetY) s.fleetY = static_cast<float>(digitY() + digitHeight() + 2);

  s.shipX = width * 0.5f + sinf(static_cast<float>(frame.nowMs) * 0.0012f) * width * 0.28f;
  s.shadowX += (s.shipX - s.shadowX) * clampf(dt * 6.0f, 0.0f, 1.0f);

  const bool autoShot = static_cast<int32_t>(frame.nowMs - s.nextAutoShotMs) >= 0;
  if (!s.laserActive && (minuteChanged() || autoShot)) {
    s.laserActive = true;
    s.laserX = s.shadowX;
    s.laserY = static_cast<float>(screen_.height - 6);
    s.nextAutoShotMs = frame.nowMs + 4500;
  }

  if (s.laserActive) {
    s.laserY -= 90.0f * dt;
    if (s.laserY <= s.fleetY + 4.0f) {
      s.laserActive = false;
      for (uint8_t i = 0; i < 8; ++i) {
        State::Fragment& f = s.fragments[i];
        if (f.active) continue;
        f.active = true;
        f.x = s.laserX;
        f.y = s.fleetY + 4.0f;
        f.vy = 26.0f + static_cast<float>(i) * 3.0f;
        break;
      }
    }
  }

  for (uint8_t i = 0; i < 8; ++i) {
    State::Fragment& f = s.fragments[i];
    if (!f.active) continue;
    f.y += f.vy * dt;
    if (f.y > static_cast<float>(screen_.height)) f.active = false;
  }
}

void ClockSpace::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!state_) return;
  if (!haveTime()) {
    drawSyncNotice(target, frame);
    return;
  }
  State& s = *state_;
  const int16_t height = static_cast<int16_t>(screen_.height);
  const CRGB alien(90, 240, 120);
  const CRGB ship(160, 220, 255);
  const CRGB shot(255, 220, 90);

  for (uint8_t row = 0; row < 2; ++row) {
    for (uint8_t col = 0; col < 3; ++col) {
      const int16_t x = static_cast<int16_t>(s.fleetX + col * 10.0f);
      const int16_t y = static_cast<int16_t>(s.fleetY + row * 8.0f);
      const bool flip = ((frame.nowMs / 420u) + row + col) % 2u == 0u;
      drawSolidSprite(target, x, y, flip ? kInvaderA : kInvaderB, 6, alien);
    }
  }

  const int16_t groundY = static_cast<int16_t>(height - 2);
  target.drawFastHLine(0, groundY, static_cast<int16_t>(screen_.width), CRGB(0, 120, 60));

  const int16_t shipY = static_cast<int16_t>(height - 4);
  target.fillRect(static_cast<int16_t>(s.shadowX - 2.0f), shipY, 5, 1, ship);
  target.drawPixel(static_cast<int16_t>(s.shadowX), static_cast<int16_t>(shipY - 1), ship);

  if (s.laserActive) {
    target.drawFastVLine(static_cast<int16_t>(s.laserX), static_cast<int16_t>(s.laserY), 3, shot);
  }

  for (uint8_t i = 0; i < 8; ++i) {
    if (s.fragments[i].active) {
      target.drawPixel(static_cast<int16_t>(s.fragments[i].x), static_cast<int16_t>(s.fragments[i].y),
                       alien);
    }
  }

  drawDigits(target, plates_, nullptr);
  if (showDate_) drawDateRow(target, frame, CRGB(140, 140, 140));
}

// ---------------------------------------------------------------------------
// Pong / breakout
// ---------------------------------------------------------------------------
struct ClockPong::State {
  float ballX[3];
  float ballY[3];
  float ballVx[3];
  float ballVy[3];
  float paddleX;
  int8_t offset[5];
  float offsetV[5];
};

ClockPong::ClockPong() : state_(nullptr) {}

void ClockPong::resetClock() {
  if (!state_) state_ = new (std::nothrow) State();
  if (!state_) return;
  State& s = *state_;
  const float width = static_cast<float>(screen_.width);
  for (uint8_t i = 0; i < 3; ++i) {
    s.ballX[i] = width * (0.25f + 0.25f * i);
    s.ballY[i] = static_cast<float>(digitY() + digitHeight() + 6 + i * 5);
    s.ballVx[i] = (i % 2 == 0) ? 26.0f : -22.0f;
    s.ballVy[i] = (i % 3 == 0) ? 18.0f : -16.0f;
  }
  s.paddleX = width * 0.5f;
  for (uint8_t i = 0; i < 5; ++i) {
    s.offset[i] = 0;
    s.offsetV[i] = 0.0f;
  }
}

void ClockPong::releaseClock() {
  delete state_;
  state_ = nullptr;
}

void ClockPong::updateClock(const FrameContext& frame, float dt) {
  (void)frame;
  if (!state_) return;
  State& s = *state_;
  dt = clampf(dt, 0.0f, 0.1f);
  const float width = static_cast<float>(screen_.width);
  const float top = static_cast<float>(digitY() + digitHeight() + 1);
  const float bottom = static_cast<float>(screen_.height - 3);
  const float speedScale = 0.6f + static_cast<float>(speed_) * 0.12f;

  for (uint8_t i = 0; i < 3; ++i) {
    s.ballX[i] += s.ballVx[i] * speedScale * dt;
    s.ballY[i] += s.ballVy[i] * speedScale * dt;
    if (s.ballX[i] < 1.0f || s.ballX[i] > width - 2.0f) {
      s.ballVx[i] = -s.ballVx[i];
      s.ballX[i] = clampf(s.ballX[i], 1.0f, width - 2.0f);
    }
    if (s.ballY[i] < top || s.ballY[i] > bottom) {
      s.ballVy[i] = -s.ballVy[i];
      s.ballY[i] = clampf(s.ballY[i], top, bottom);
    }
  }

  float target = s.ballX[0];
  for (uint8_t i = 1; i < 3; ++i) {
    if (fabsf(s.ballY[i] - bottom) < fabsf(s.ballY[0] - bottom)) target = s.ballX[i];
  }
  s.paddleX += (target - s.paddleX) * clampf(dt * 5.0f, 0.0f, 1.0f);
  s.paddleX = clampf(s.paddleX, 4.0f, width - 5.0f);

  for (uint8_t i = 0; i < 5; ++i) {
    kickDigit(&s.offset[i], &s.offsetV[i], i, changedIndex(), 4.0f);
    springStep(&s.offset[i], &s.offsetV[i], dt);
  }
}

void ClockPong::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!state_) return;
  if (!haveTime()) {
    drawSyncNotice(target, frame);
    return;
  }
  State& s = *state_;
  const int16_t width = static_cast<int16_t>(screen_.width);
  const int16_t height = static_cast<int16_t>(screen_.height);
  const int16_t top = static_cast<int16_t>(digitY() + digitHeight() + 1);
  const CRGB wall(80, 120, 200);
  const CRGB ball(255, 255, 255);
  const CRGB paddle(120, 255, 160);

  target.drawFastVLine(0, top, static_cast<int16_t>(height - top), wall);
  target.drawFastVLine(static_cast<int16_t>(width - 1), top, static_cast<int16_t>(height - top), wall);

  for (uint8_t i = 0; i < 3; ++i) {
    target.fillRect(static_cast<int16_t>(s.ballX[i]), static_cast<int16_t>(s.ballY[i]), 2, 2, ball);
  }

  target.fillRect(static_cast<int16_t>(s.paddleX - 3.0f), static_cast<int16_t>(height - 2), 7, 2,
                  paddle);

  drawDigits(target, plates_, s.offset);
  if (showDate_) drawDateRow(target, frame, CRGB(140, 140, 140));
}

// ---------------------------------------------------------------------------
// Pac-Man
// ---------------------------------------------------------------------------
struct ClockPacman::State {
  float pacX;
  float pacDir;
  float mouth;
  float ghostX[2];
  float ghostDir[2];
  uint8_t* pellets;
  uint16_t pelletCount;
  float spacing;
  float rowY;
  float points;
};

ClockPacman::ClockPacman() : state_(nullptr) {}

void ClockPacman::resetClock() {
  if (!state_) state_ = new (std::nothrow) State();
  if (!state_) return;
  State& s = *state_;
  s.pacX = 2.0f;
  s.pacDir = 1.0f;
  s.mouth = 0.0f;
  s.spacing = 4.0f;
  s.rowY = static_cast<float>(digitY() + digitHeight() + (screen_.height - digitY() - digitHeight()) / 2);
  s.pelletCount = static_cast<uint16_t>(static_cast<float>(screen_.width) / s.spacing);
  s.pellets = new (std::nothrow) uint8_t[s.pelletCount];
  if (s.pellets) memset(s.pellets, 1, s.pelletCount);
  s.ghostX[0] = static_cast<float>(screen_.width) - 10.0f;
  s.ghostX[1] = static_cast<float>(screen_.width) - 20.0f;
  s.ghostDir[0] = -1.0f;
  s.ghostDir[1] = -1.0f;
  s.points = 0.0f;
}

void ClockPacman::releaseClock() {
  if (state_) {
    delete[] state_->pellets;
    state_->pellets = nullptr;
    delete state_;
    state_ = nullptr;
  }
}

void ClockPacman::updateClock(const FrameContext& frame, float dt) {
  if (!state_) return;
  State& s = *state_;
  dt = clampf(dt, 0.0f, 0.1f);
  const float width = static_cast<float>(screen_.width);
  const float pace = 12.0f + static_cast<float>(speed_) * 2.2f;
  const float boost = secondsSinceChange() < 1.2f ? 1.8f : 1.0f;

  s.pacX += s.pacDir * pace * boost * dt;
  if (s.pacX > width - 3.0f) {
    s.pacX = width - 3.0f;
    s.pacDir = -1.0f;
  } else if (s.pacX < 2.0f) {
    s.pacX = 2.0f;
    s.pacDir = 1.0f;
  }
  s.mouth = fabsf(sinf(static_cast<float>(frame.nowMs) * 0.012f));

  if (s.pellets) {
    const int16_t index = static_cast<int16_t>(s.pacX / s.spacing);
    if (index >= 0 && index < static_cast<int16_t>(s.pelletCount) && s.pellets[index]) {
      s.pellets[index] = 0;
      s.points += 1.0f;
    }
    // Refill the row when Pac-Man has eaten it all.
    bool any = false;
    for (uint16_t i = 0; i < s.pelletCount; ++i) {
      if (s.pellets[i]) {
        any = true;
        break;
      }
    }
    if (!any) memset(s.pellets, 1, s.pelletCount);
  }

  for (uint8_t g = 0; g < 2; ++g) {
    const float target = s.pacX + (g == 0 ? 6.0f : -6.0f);
    s.ghostX[g] += (target - s.ghostX[g]) * clampf(dt * (0.8f + g * 0.3f), 0.0f, 1.0f);
    s.ghostDir[g] = (target > s.ghostX[g]) ? 1.0f : -1.0f;
    s.ghostX[g] = clampf(s.ghostX[g], 0.0f, width - 7.0f);
  }
}

void ClockPacman::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!state_) return;
  if (!haveTime()) {
    drawSyncNotice(target, frame);
    return;
  }
  State& s = *state_;
  const CRGB pellet(255, 230, 190);
  const CRGB pac(255, 220, 40);
  const CRGB ghostBody(255, 90, 90);
  const CRGB ghostEye(255, 255, 255);
  const CRGB black(0, 0, 0);
  const char* const ghostKeys = "GW";
  const CRGB ghostColors[2] = {ghostBody, ghostEye};

  if (s.pellets) {
    for (uint16_t i = 0; i < s.pelletCount; ++i) {
      if (!s.pellets[i]) continue;
      target.drawPixel(static_cast<int16_t>(i * s.spacing + 1), static_cast<int16_t>(s.rowY), pellet);
    }
  }

  const int16_t pacX = static_cast<int16_t>(s.pacX);
  const int16_t pacY = static_cast<int16_t>(s.rowY);
  target.fillCircle(pacX, pacY, 3, pac);
  const int16_t mouth = static_cast<int16_t>(1.0f + s.mouth * 3.0f);
  if (s.pacDir >= 0.0f) {
    target.fillTriangle(pacX, pacY, static_cast<int16_t>(pacX + 5), static_cast<int16_t>(pacY - mouth),
                        static_cast<int16_t>(pacX + 5), static_cast<int16_t>(pacY + mouth), black);
  } else {
    target.fillTriangle(pacX, pacY, static_cast<int16_t>(pacX - 5), static_cast<int16_t>(pacY - mouth),
                        static_cast<int16_t>(pacX - 5), static_cast<int16_t>(pacY + mouth), black);
  }

  for (uint8_t g = 0; g < 2; ++g) {
    const int16_t gx = static_cast<int16_t>(s.ghostX[g]);
    const int16_t gy = static_cast<int16_t>(s.rowY - 3);
    drawSprite(target, gx, gy, kGhost, 7, ghostKeys, ghostColors, 2);
    target.drawPixel(static_cast<int16_t>(gx + (s.ghostDir[g] > 0.0f ? 2 : 4)),
                     static_cast<int16_t>(gy + 3), black);
  }

  drawDigits(target, plates_, nullptr);
  if (changedIndex() < 5 && secondsSinceChange() < 0.4f) {
    target.fillRect(static_cast<int16_t>(digitX(changedIndex()) - 1),
                    static_cast<int16_t>(digitY() - 1), static_cast<int16_t>(digitWidth() + 2),
                    static_cast<int16_t>(digitHeight() + 2), CRGB(0, 0, 0));
  }
  if (showDate_) drawDateRow(target, frame, CRGB(140, 140, 140));
}

// ---------------------------------------------------------------------------
// Bomberman
// ---------------------------------------------------------------------------
struct ClockBomberman::State {
  float x;
  float dir;
  bool bombActive;
  float bombX;
  float fuse;
  float blast;
  bool blasting;
  float dropTimer;
};

ClockBomberman::ClockBomberman() : state_(nullptr) {}

void ClockBomberman::resetClock() {
  if (!state_) state_ = new (std::nothrow) State();
  if (!state_) return;
  State& s = *state_;
  s.x = 2.0f;
  s.dir = 1.0f;
  s.bombActive = false;
  s.bombX = 0.0f;
  s.fuse = 0.0f;
  s.blast = 0.0f;
  s.blasting = false;
  s.dropTimer = 0.0f;
}

void ClockBomberman::releaseClock() {
  delete state_;
  state_ = nullptr;
}

void ClockBomberman::updateClock(const FrameContext& frame, float dt) {
  (void)frame;
  if (!state_) return;
  State& s = *state_;
  dt = clampf(dt, 0.0f, 0.1f);
  const float width = static_cast<float>(screen_.width);
  const float pace = 9.0f + static_cast<float>(speed_) * 2.0f;

  s.x += s.dir * pace * dt;
  if (s.x > width - 8.0f) {
    s.x = width - 8.0f;
    s.dir = -1.0f;
  } else if (s.x < 2.0f) {
    s.x = 2.0f;
    s.dir = 1.0f;
  }

  s.dropTimer += dt;
  const bool timeToDrop = !s.bombActive && !s.blasting && s.dropTimer > 5.0f;
  if (timeToDrop || (minuteChanged() && !s.bombActive && !s.blasting)) {
    s.bombActive = true;
    s.bombX = s.x + 2.0f;
    s.fuse = 0.0f;
    s.dropTimer = 0.0f;
  }

  if (s.bombActive) {
    s.fuse += dt;
    if (s.fuse > 1.6f) {
      s.bombActive = false;
      s.blasting = true;
      s.blast = 0.0f;
    }
  }

  if (s.blasting) {
    s.blast += 26.0f * dt;
    if (s.blast > 14.0f) {
      s.blasting = false;
      s.blast = 0.0f;
    }
  }
}

void ClockBomberman::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!state_) return;
  if (!haveTime()) {
    drawSyncNotice(target, frame);
    return;
  }
  State& s = *state_;
  const int16_t height = static_cast<int16_t>(screen_.height);
  const int16_t groundY = static_cast<int16_t>(height - 2);
  const CRGB ground(90, 90, 120);
  const CRGB skin(255, 200, 160);
  const CRGB suit(240, 240, 250);
  const CRGB bomb(30, 30, 40);
  const CRGB spark(255, 120, 0);
  const CRGB trouser(40, 90, 200);
  const char* const bomberKeys = "HWB";
  const CRGB bomberColors[3] = {suit, skin, trouser};

  target.drawFastHLine(0, groundY, static_cast<int16_t>(screen_.width), ground);

  drawSprite(target, static_cast<int16_t>(s.x), static_cast<int16_t>(groundY - 6), kBomber, 7,
             bomberKeys, bomberColors, 3);

  if (s.bombActive) {
    const int16_t bx = static_cast<int16_t>(s.bombX);
    const int16_t by = static_cast<int16_t>(groundY - 3);
    target.fillCircle(bx, by, 2, bomb);
    const bool blink = (static_cast<uint32_t>(s.fuse * 8.0f) % 2u) == 0u;
    target.drawPixel(static_cast<int16_t>(bx + 1), static_cast<int16_t>(by - 3),
                     blink ? spark : CRGB(255, 240, 120));
  }

  if (s.blasting) {
    const int16_t cx = static_cast<int16_t>(s.bombX);
    const int16_t cy = static_cast<int16_t>(groundY - 2);
    const int16_t radius = static_cast<int16_t>(s.blast);
    drawCircleOutline(target, cx, cy, radius, spark);
    drawCircleOutline(target, cx, cy, static_cast<int16_t>(radius / 2), CRGB(255, 220, 60));
  }

  drawDigits(target, plates_, nullptr);
  if (s.blasting && changedIndex() < 5) {
    target.fillRect(static_cast<int16_t>(digitX(changedIndex()) - 1),
                    static_cast<int16_t>(digitY() - 1), static_cast<int16_t>(digitWidth() + 2),
                    static_cast<int16_t>(digitHeight() + 2), CRGB(0, 0, 0));
  }
  if (showDate_) drawDateRow(target, frame, CRGB(140, 140, 140));
}

// ---------------------------------------------------------------------------
// TRON
// ---------------------------------------------------------------------------
struct ClockTron::State {
  struct Bike {
    float x;
    float y;
    int8_t dx;
    int8_t dy;
    float trailX[80];
    float trailY[80];
    uint8_t trailCount;
    uint8_t trailHead;

    void reset(float px, float py, int8_t ndx, int8_t ndy) {
      x = px;
      y = py;
      dx = ndx;
      dy = ndy;
      trailHead = 0;
      trailX[0] = px;
      trailY[0] = py;
      trailCount = 1;
    }
  } bikes[2];
  float stepTimer;
  uint32_t lastResetMs;
};

ClockTron::ClockTron() : state_(nullptr) {}

void ClockTron::resetClock() {
  if (!state_) state_ = new (std::nothrow) State();
  if (!state_) return;
  State& s = *state_;
  const float top = static_cast<float>(digitY() + digitHeight() + 1);
  const float bottom = static_cast<float>(screen_.height - 1);
  s.bikes[0].reset(2.0f, top, 1, 0);
  s.bikes[1].reset(static_cast<float>(screen_.width) - 3.0f, bottom, -1, 0);
  s.stepTimer = 0.0f;
  s.lastResetMs = 0;
}

void ClockTron::releaseClock() {
  delete state_;
  state_ = nullptr;
}

void ClockTron::updateClock(const FrameContext& frame, float dt) {
  if (!state_) return;
  State& s = *state_;
  dt = clampf(dt, 0.0f, 0.1f);
  s.stepTimer += dt;

  const float interval = 0.12f / (0.4f + static_cast<float>(speed_) * 0.12f);
  if (s.stepTimer < interval) return;
  s.stepTimer = 0.0f;

  const float top = static_cast<float>(digitY() + digitHeight() + 1);
  const float bottom = static_cast<float>(screen_.height - 1);
  const float left = 1.0f;
  const float right = static_cast<float>(screen_.width) - 2.0f;

  for (uint8_t b = 0; b < 2; ++b) {
    State::Bike& bike = s.bikes[b];
    bike.x += bike.dx * 2.0f;
    bike.y += bike.dy * 2.0f;

    bool collide = bike.x < left || bike.x > right || bike.y < top || bike.y > bottom;
    if (!collide) {
      for (uint8_t other = 0; other < 2 && !collide; ++other) {
        const State::Bike& path = s.bikes[other];
        for (uint8_t i = 0; i < path.trailCount; ++i) {
          if (fabsf(path.trailX[i] - bike.x) < 1.5f && fabsf(path.trailY[i] - bike.y) < 1.5f) {
            collide = true;
            break;
          }
        }
      }
    }

    if (collide) {
      s.bikes[0].reset(2.0f, top, 1, 0);
      s.bikes[1].reset(static_cast<float>(screen_.width) - 3.0f, bottom, -1, 0);
      s.lastResetMs = frame.nowMs;
      continue;
    }

    // Mostly straight lines with the occasional turn, like the light cycles.
    const uint8_t turn = static_cast<uint8_t>((bike.x + bike.y + static_cast<float>(frame.nowMs) / 97.0f));
    if ((turn % 23u) == 0u) {
      const bool vertical = bike.dy != 0;
      if (vertical) {
        bike.dx = (turn & 1u) ? 1 : -1;
        bike.dy = 0;
      } else {
        bike.dy = (turn & 2u) ? 1 : -1;
        bike.dx = 0;
      }
    }

    bike.trailHead = static_cast<uint8_t>((bike.trailHead + 1) % 80);
    bike.trailX[bike.trailHead] = bike.x;
    bike.trailY[bike.trailHead] = bike.y;
    if (bike.trailCount < 80) ++bike.trailCount;
  }

  if (minuteChanged()) {
    s.bikes[0].reset(2.0f, top, 1, 0);
    s.bikes[1].reset(static_cast<float>(screen_.width) - 3.0f, bottom, -1, 0);
  }
}

void ClockTron::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!state_) return;
  if (!haveTime()) {
    drawSyncNotice(target, frame);
    return;
  }
  State& s = *state_;
  const CRGB colors[2] = {CRGB(80, 220, 255), CRGB(255, 150, 40)};

  for (uint8_t b = 0; b < 2; ++b) {
    const State::Bike& bike = s.bikes[b];
    const uint8_t count = bike.trailCount;
    const uint8_t start = static_cast<uint8_t>((bike.trailHead + 80 - count + 1) % 80);
    int16_t previousX = -1;
    int16_t previousY = -1;
    for (uint8_t i = 0; i < count; ++i) {
      const uint8_t index = static_cast<uint8_t>((start + i) % 80);
      const int16_t x = static_cast<int16_t>(bike.trailX[index]);
      const int16_t y = static_cast<int16_t>(bike.trailY[index]);
      if (previousX >= 0) {
        const int16_t deltaX = static_cast<int16_t>(x > previousX ? x - previousX : previousX - x);
        const int16_t deltaY = static_cast<int16_t>(y > previousY ? y - previousY : previousY - y);
        const int16_t steps = deltaX > deltaY ? deltaX : deltaY;
        for (int16_t step = 1; step <= steps; ++step) {
          const int16_t px = static_cast<int16_t>(previousX + (x - previousX) * step / (steps ? steps : 1));
          const int16_t py = static_cast<int16_t>(previousY + (y - previousY) * step / (steps ? steps : 1));
          target.drawPixel(px, py, colors[b]);
        }
      }
      previousX = x;
      previousY = y;
    }
    target.fillRect(static_cast<int16_t>(bike.x), static_cast<int16_t>(bike.y), 2, 2, colors[b]);
  }

  drawDigits(target, plates_, nullptr);
  if (showDate_) drawDateRow(target, frame, CRGB(120, 140, 160));
}

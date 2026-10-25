#include "AnimatedClocks.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "core/FrameContext.hpp"

namespace {

constexpr float kPi = 3.14159265f;

float clampf(float value, float low, float high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

// Cheap deterministic PRNG. Seeded per output instance and advanced only from
// the owning clock, so two snake clocks never share state.
uint16_t nextRandom(uint16_t& seed) {
  seed = static_cast<uint16_t>(seed * 2053u + 13849u);
  return static_cast<uint16_t>(seed >> 4);
}

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

const char* const kDinoRunA[9] = {
    "....GGG..",
    "....GGGG.",
    "...GGWG..",
    "...GGG...",
    "GGGGG....",
    ".GGGG....",
    "..GG.....",
    "..G.G....",
    ".G...G...",
};

const char* const kDinoRunB[9] = {
    "....GGG..",
    "....GGGG.",
    "...GGWG..",
    "...GGG...",
    "GGGGG....",
    ".GGGG....",
    "..GG.....",
    ".G..G....",
    "G....G...",
};

const char* const kPtero[5] = {
    "G.....G",
    ".G.G.G.",
    "..GGG..",
    ".G.G.G.",
    "G.....G",
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

void drawPolygon(PixelCanvas& target, const float* xs, const float* ys, uint8_t count, CRGB color) {
  for (uint8_t i = 0; i < count; ++i) {
    const uint8_t j = static_cast<uint8_t>((i + 1) % count);
    const int16_t x0 = static_cast<int16_t>(xs[i]);
    const int16_t y0 = static_cast<int16_t>(ys[i]);
    const int16_t x1 = static_cast<int16_t>(xs[j]);
    const int16_t y1 = static_cast<int16_t>(ys[j]);
    const int16_t dx = static_cast<int16_t>(x1 > x0 ? x1 - x0 : x0 - x1);
    const int16_t dy = static_cast<int16_t>(y1 > y0 ? y1 - y0 : y0 - y1);
    const int16_t steps = dx > dy ? dx : dy;
    for (int16_t step = 0; step <= steps; ++step) {
      const int16_t px = static_cast<int16_t>(x0 + (x1 - x0) * step / (steps ? steps : 1));
      const int16_t py = static_cast<int16_t>(y0 + (y1 - y0) * step / (steps ? steps : 1));
      target.drawPixel(px, py, color);
    }
  }
}

// Tetromino cells in a 4x4 box, bit 0 = top-left.
const uint16_t kPieces[7] = {
    0x0F00,  // I
    0x0660,  // O
    0x0E40,  // L
    0x0622,  // J
    0x06C0,  // T
    0x0C60,  // S
    0x0630,  // Z
};

}  // namespace

// ---------------------------------------------------------------------------
// Snake
// ---------------------------------------------------------------------------
struct ClockSnake::State {
  uint8_t cell;
  uint8_t cols;
  uint8_t rows;
  struct Cell {
    uint8_t x;
    uint8_t y;
  } body[96];
  uint8_t length;
  int8_t dx;
  int8_t dy;
  uint8_t foodX;
  uint8_t foodY;
  float timer;
  float interval;
  uint8_t grow;
  uint16_t seed;
};

ClockSnake::ClockSnake() : state_(nullptr) {}

void ClockSnake::resetClock() {
  if (!state_) state_ = new (std::nothrow) State();
  if (!state_) return;
  State& s = *state_;
  s.cell = static_cast<uint8_t>(clampf(static_cast<float>(screen_.height) / 24.0f, 2.0f, 4.0f));
  s.cols = static_cast<uint8_t>(screen_.width / s.cell);
  const uint16_t sceneHeight =
      static_cast<uint16_t>(screen_.height - digitY() - digitHeight() - 1);
  s.rows = static_cast<uint8_t>(sceneHeight / s.cell);
  if (s.rows < 4) s.rows = 4;
  s.length = 4;
  s.dx = 1;
  s.dy = 0;
  for (uint8_t i = 0; i < s.length; ++i) {
    s.body[i].x = static_cast<uint8_t>(2 + i);
    s.body[i].y = static_cast<uint8_t>(s.rows / 2);
  }
  s.foodX = static_cast<uint8_t>(nextRandom(s.seed) % s.cols);
  s.foodY = static_cast<uint8_t>(nextRandom(s.seed) % s.rows);
  s.timer = 0.0f;
  s.interval = 0.20f - static_cast<float>(speed_) * 0.01f;
  s.grow = 0;
  s.seed = static_cast<uint16_t>(screen_.width * 31u + screen_.height * 17u + 7u);
}

void ClockSnake::releaseClock() {
  delete state_;
  state_ = nullptr;
}

void ClockSnake::updateClock(const FrameContext& frame, float dt) {
  (void)frame;
  if (!state_) return;
  State& s = *state_;
  dt = clampf(dt, 0.0f, 0.1f);
  s.timer += dt;

  const float interval = s.interval * (secondsSinceChange() < 2.0f ? 0.55f : 1.0f);
  if (s.timer < interval) return;
  s.timer = 0.0f;

  int8_t nextX = static_cast<int8_t>(s.body[0].x + s.dx);
  int8_t nextY = static_cast<int8_t>(s.body[0].y + s.dy);

  if (nextX < 0 || nextX >= static_cast<int8_t>(s.cols) || nextY < 0 ||
      nextY >= static_cast<int8_t>(s.rows)) {
    resetClock();
    return;
  }

  for (uint8_t i = 0; i < s.length; ++i) {
    if (s.body[i].x == static_cast<uint8_t>(nextX) && s.body[i].y == static_cast<uint8_t>(nextY)) {
      resetClock();
      return;
    }
  }

  // Steer toward the food so the snake keeps eating without user input.
  if (static_cast<uint8_t>(nextX) == s.foodX || static_cast<uint8_t>(nextY) == s.foodY) {
    if (static_cast<uint8_t>(nextY) != s.foodY && s.dx != 0) {
      s.dx = 0;
      s.dy = static_cast<int8_t>(s.foodY > static_cast<uint8_t>(nextY) ? 1 : -1);
    } else if (static_cast<uint8_t>(nextX) != s.foodX && s.dy != 0) {
      s.dy = 0;
      s.dx = static_cast<int8_t>(s.foodX > static_cast<uint8_t>(nextX) ? 1 : -1);
    }
  }

  nextX = static_cast<int8_t>(s.body[0].x + s.dx);
  nextY = static_cast<int8_t>(s.body[0].y + s.dy);
  if (nextX < 0 || nextX >= static_cast<int8_t>(s.cols) || nextY < 0 ||
      nextY >= static_cast<int8_t>(s.rows)) {
    resetClock();
    return;
  }

  const uint8_t newLength = static_cast<uint8_t>(s.length < 96 ? s.length + (s.grow || (nextX == s.foodX && nextY == s.foodY) ? 1 : 0) : s.length);
  for (uint8_t i = s.length; i > 0 && i < 96; --i) {
    s.body[i] = s.body[i - 1];
  }
  s.body[0].x = static_cast<uint8_t>(nextX);
  s.body[0].y = static_cast<uint8_t>(nextY);
  s.length = newLength;

  if (s.grow > 0) --s.grow;
  if (nextX == s.foodX && nextY == s.foodY) {
    s.foodX = static_cast<uint8_t>(nextRandom(s.seed) % s.cols);
    s.foodY = static_cast<uint8_t>(nextRandom(s.seed) % s.rows);
    if (s.length > 90) s.length = 4;
  }

  if (minuteChanged()) {
    s.grow = static_cast<uint8_t>(s.grow + 3);
  }
}

void ClockSnake::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!state_) return;
  if (!haveTime()) {
    drawSyncNotice(target, frame);
    return;
  }
  State& s = *state_;
  const int16_t top = static_cast<int16_t>(digitY() + digitHeight() + 1);
  const CRGB body(60, 220, 110);
  const CRGB head(200, 255, 200);
  const CRGB food(255, 90, 90);

  target.drawRect(0, top, static_cast<int16_t>(screen_.width),
                  static_cast<int16_t>(screen_.height - top - 1), body);

  const bool foodVisible = (frame.nowMs / 400u) % 2u == 0u;
  if (foodVisible) {
    target.fillRect(static_cast<int16_t>(s.foodX * s.cell), static_cast<int16_t>(top + s.foodY * s.cell),
                    s.cell, s.cell, food);
  }

  for (uint8_t i = s.length; i > 0; --i) {
    const uint8_t index = static_cast<uint8_t>(i - 1);
    const uint8_t size = static_cast<uint8_t>(s.cell > 1 ? s.cell - 1 : 1);
    target.fillRect(static_cast<int16_t>(s.body[index].x * s.cell),
                    static_cast<int16_t>(top + s.body[index].y * s.cell), size, size,
                    index == 0 ? head : body);
  }

  drawDigits(target, plates_, nullptr);
  if (showDate_) drawDateRow(target, frame, CRGB(140, 140, 140));
}

// ---------------------------------------------------------------------------
// Tetris
// ---------------------------------------------------------------------------
struct ClockTetris::State {
  uint8_t cell;
  uint8_t cols;
  uint8_t rows;
  uint8_t* board;
  uint8_t piece;
  int8_t pieceX;
  int8_t pieceY;
  float fallTimer;
  float interval;
  float flash;
  uint8_t flashRow;
  uint16_t seed;
};

ClockTetris::ClockTetris() : state_(nullptr) {}

void ClockTetris::resetClock() {
  if (!state_) state_ = new (std::nothrow) State();
  if (!state_) return;
  State& s = *state_;
  s.cell = static_cast<uint8_t>(clampf(static_cast<float>(screen_.height) / 26.0f, 2.0f, 4.0f));
  s.cols = static_cast<uint8_t>(screen_.width / s.cell);
  const uint16_t sceneHeight =
      static_cast<uint16_t>(screen_.height - digitY() - digitHeight() - 1);
  s.rows = static_cast<uint8_t>(sceneHeight / s.cell);
  if (s.rows < 5) s.rows = 5;
  const uint16_t cells = static_cast<uint16_t>(s.cols * s.rows);
  s.board = new (std::nothrow) uint8_t[cells];
  if (s.board) memset(s.board, 0, cells);
  s.piece = static_cast<uint8_t>(nextRandom(s.seed) % 7u);
  s.pieceX = static_cast<int8_t>(s.cols / 2 - 2);
  s.pieceY = 0;
  s.fallTimer = 0.0f;
  s.interval = 0.65f - static_cast<float>(speed_) * 0.04f;
  s.flash = 0.0f;
  s.flashRow = 0;
  s.seed = 0x2A5Bu;
}

void ClockTetris::releaseClock() {
  if (state_) {
    delete[] state_->board;
    state_->board = nullptr;
    delete state_;
    state_ = nullptr;
  }
}

namespace {

bool tetrisFits(const uint8_t* board, uint8_t cols, uint8_t rows, uint16_t mask, int8_t px, int8_t py) {
  for (uint8_t row = 0; row < 4; ++row) {
    for (uint8_t col = 0; col < 4; ++col) {
      if ((mask & (1u << (15 - (row * 4 + col)))) == 0) continue;
      const int16_t x = static_cast<int16_t>(px + col);
      const int16_t y = static_cast<int16_t>(py + row);
      if (x < 0 || x >= static_cast<int16_t>(cols) || y >= static_cast<int16_t>(rows)) return false;
      if (y >= 0 && board[y * cols + x]) return false;
    }
  }
  return true;
}

}  // namespace

void ClockTetris::updateClock(const FrameContext& frame, float dt) {
  (void)frame;
  if (!state_ || !state_->board) return;
  State& s = *state_;
  dt = clampf(dt, 0.0f, 0.1f);

  if (s.flash > 0.0f) {
    s.flash -= dt;
    return;
  }

  s.fallTimer += dt;
  const float interval = s.interval * (minuteChanged() ? 0.35f : 1.0f);
  if (s.fallTimer < interval) return;
  s.fallTimer = 0.0f;

  const uint16_t mask = kPieces[s.piece % 7u];
  if (tetrisFits(s.board, s.cols, s.rows, mask, s.pieceX, static_cast<int8_t>(s.pieceY + 1))) {
    ++s.pieceY;
    return;
  }

  // Lock the piece.
  for (uint8_t row = 0; row < 4; ++row) {
    for (uint8_t col = 0; col < 4; ++col) {
      if ((mask & (1u << (15 - (row * 4 + col)))) == 0) continue;
      const int16_t x = static_cast<int16_t>(s.pieceX + col);
      const int16_t y = static_cast<int16_t>(s.pieceY + row);
      if (y >= 0 && y < static_cast<int16_t>(s.rows) && x >= 0 && x < static_cast<int16_t>(s.cols)) {
        s.board[y * s.cols + x] = 1;
      }
    }
  }

  // Clear full rows.
  for (int16_t row = static_cast<int16_t>(s.rows - 1); row >= 0; --row) {
    bool full = true;
    for (uint8_t col = 0; col < s.cols; ++col) {
      if (!s.board[row * s.cols + col]) {
        full = false;
        break;
      }
    }
    if (!full) continue;
    for (int16_t y = row; y > 0; --y) {
      memcpy(&s.board[y * s.cols], &s.board[(y - 1) * s.cols], s.cols);
    }
    memset(&s.board[0], 0, s.cols);
    s.flash = 0.25f;
    s.flashRow = static_cast<uint8_t>(row);
    break;
  }

  s.piece = static_cast<uint8_t>(nextRandom(s.seed) % 7u);
  s.pieceX = static_cast<int8_t>(s.cols / 2 - 2);
  s.pieceY = 0;
  if (!tetrisFits(s.board, s.cols, s.rows, kPieces[s.piece], s.pieceX, s.pieceY)) {
    memset(s.board, 0, static_cast<size_t>(s.cols) * s.rows);
  }
}

void ClockTetris::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!state_ || !state_->board) return;
  if (!haveTime()) {
    drawSyncNotice(target, frame);
    return;
  }
  State& s = *state_;
  const int16_t top = static_cast<int16_t>(digitY() + digitHeight() + 1);
  const CRGB block(120, 180, 255);
  const CRGB piece(255, 200, 80);
  const CRGB flash(255, 255, 255);

  for (uint8_t row = 0; row < s.rows; ++row) {
    for (uint8_t col = 0; col < s.cols; ++col) {
      if (!s.board[row * s.cols + col]) continue;
      target.fillRect(static_cast<int16_t>(col * s.cell), static_cast<int16_t>(top + row * s.cell),
                      s.cell, s.cell, block);
    }
  }

  const uint16_t mask = kPieces[s.piece % 7u];
  for (uint8_t row = 0; row < 4; ++row) {
    for (uint8_t col = 0; col < 4; ++col) {
      if ((mask & (1u << (15 - (row * 4 + col)))) == 0) continue;
      const int16_t x = static_cast<int16_t>(s.pieceX + col);
      const int16_t y = static_cast<int16_t>(s.pieceY + row);
      if (y < 0) continue;
      target.fillRect(static_cast<int16_t>(x * s.cell), static_cast<int16_t>(top + y * s.cell), s.cell,
                      s.cell, piece);
    }
  }

  if (s.flash > 0.0f) {
    target.fillRect(0, static_cast<int16_t>(top + s.flashRow * s.cell),
                    static_cast<int16_t>(s.cols * s.cell), s.cell, flash);
  }

  drawDigits(target, plates_, nullptr);
  if (showDate_) drawDateRow(target, frame, CRGB(140, 140, 140));
}

// ---------------------------------------------------------------------------
// Asteroids
// ---------------------------------------------------------------------------
struct ClockAsteroids::State {
  struct Rock {
    float x;
    float y;
    float vx;
    float vy;
    float angle;
    float spin;
    float radius;
    float verts[6];
  } rocks[3];
  float shipX;
  bool bulletActive;
  float bulletX;
  float bulletY;
  uint32_t nextShotMs;
  struct Shard {
    float x;
    float y;
    float vx;
    float vy;
    uint8_t life;
    bool active;
  } shards[10];
};

ClockAsteroids::ClockAsteroids() : state_(nullptr) {}

void ClockAsteroids::resetClock() {
  if (!state_) state_ = new (std::nothrow) State();
  if (!state_) return;
  State& s = *state_;
  const float width = static_cast<float>(screen_.width);
  const float top = static_cast<float>(digitY() + digitHeight() + 2);
  const float height = static_cast<float>(screen_.height);
  for (uint8_t i = 0; i < 3; ++i) {
    State::Rock& r = s.rocks[i];
    r.x = width * (0.2f + 0.3f * i);
    r.y = top + (height - top) * (0.25f + 0.2f * i);
    r.vx = (i % 2 == 0) ? 12.0f : -10.0f;
    r.vy = (i % 3 == 0) ? 7.0f : -6.0f;
    r.angle = static_cast<float>(i) * 1.1f;
    r.spin = (i % 2 == 0) ? 0.7f : -0.5f;
    r.radius = 3.0f + static_cast<float>(i);
    for (uint8_t v = 0; v < 6; ++v) {
      r.verts[v] = r.radius * (0.65f + 0.35f * static_cast<float>((i + v) % 3) / 2.0f);
    }
  }
  s.shipX = width * 0.5f;
  s.bulletActive = false;
  s.bulletX = s.shipX;
  s.bulletY = height - 6.0f;
  s.nextShotMs = 0;
  for (uint8_t i = 0; i < 10; ++i) s.shards[i].active = false;
}

void ClockAsteroids::releaseClock() {
  delete state_;
  state_ = nullptr;
}

void ClockAsteroids::updateClock(const FrameContext& frame, float dt) {
  if (!state_) return;
  State& s = *state_;
  dt = clampf(dt, 0.0f, 0.1f);
  const float width = static_cast<float>(screen_.width);
  const float top = static_cast<float>(digitY() + digitHeight() + 1);
  const float bottom = static_cast<float>(screen_.height - 3);
  const float scale = 0.5f + static_cast<float>(speed_) * 0.1f;

  for (uint8_t i = 0; i < 3; ++i) {
    State::Rock& r = s.rocks[i];
    r.x += r.vx * scale * dt;
    r.y += r.vy * scale * dt;
    r.angle += r.spin * dt;
    if (r.x < 2.0f) r.x = width - 2.0f;
    if (r.x > width - 2.0f) r.x = 2.0f;
    if (r.y < top) r.y = bottom;
    if (r.y > bottom) r.y = top;
  }

  s.shipX = width * 0.5f + sinf(static_cast<float>(frame.nowMs) * 0.0009f) * width * 0.3f;

  const bool autoShot = static_cast<int32_t>(frame.nowMs - s.nextShotMs) >= 0;
  if (!s.bulletActive && (minuteChanged() || autoShot)) {
    s.bulletActive = true;
    s.bulletX = s.shipX;
    s.bulletY = bottom - 4.0f;
    s.nextShotMs = frame.nowMs + 3800;
  }

  if (s.bulletActive) {
    s.bulletY -= 110.0f * dt;
    for (uint8_t i = 0; i < 3; ++i) {
      State::Rock& r = s.rocks[i];
      const float dx = s.bulletX - r.x;
      const float dy = s.bulletY - r.y;
      if (dx * dx + dy * dy > r.radius * r.radius) continue;
      s.bulletActive = false;
      for (uint8_t k = 0; k < 10; ++k) {
        State::Shard& sh = s.shards[k];
        if (sh.active) continue;
        sh.active = true;
        sh.x = r.x;
        sh.y = r.y;
        sh.vx = -30.0f + static_cast<float>(k) * 6.0f;
        sh.vy = -20.0f + static_cast<float>(k % 4) * 12.0f;
        sh.life = 12;
        break;
      }
      r.x = width * 0.5f;
      r.y = top + 4.0f;
      break;
    }
    if (s.bulletY < top - 4.0f) s.bulletActive = false;
  }

  for (uint8_t i = 0; i < 10; ++i) {
    State::Shard& sh = s.shards[i];
    if (!sh.active) continue;
    sh.x += sh.vx * dt;
    sh.y += sh.vy * dt;
    if (sh.life == 0) sh.active = false;
    else --sh.life;
  }
}

void ClockAsteroids::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!state_) return;
  if (!haveTime()) {
    drawSyncNotice(target, frame);
    return;
  }
  State& s = *state_;
  const CRGB rock(170, 170, 190);
  const CRGB ship(140, 255, 180);
  const CRGB shot(255, 240, 140);

  float xs[6];
  float ys[6];
  for (uint8_t i = 0; i < 3; ++i) {
    const State::Rock& r = s.rocks[i];
    for (uint8_t v = 0; v < 6; ++v) {
      const float angle = r.angle + kPi * 2.0f * static_cast<float>(v) / 6.0f;
      xs[v] = r.x + cosf(angle) * r.verts[v];
      ys[v] = r.y + sinf(angle) * r.verts[v];
    }
    drawPolygon(target, xs, ys, 6, rock);
  }

  for (uint8_t i = 0; i < 10; ++i) {
    if (!s.shards[i].active) continue;
    target.drawPixel(static_cast<int16_t>(s.shards[i].x), static_cast<int16_t>(s.shards[i].y), rock);
  }

  const int16_t shipY = static_cast<int16_t>(screen_.height - 4);
  target.fillTriangle(static_cast<int16_t>(s.shipX), static_cast<int16_t>(shipY - 3),
                      static_cast<int16_t>(s.shipX - 3), static_cast<int16_t>(shipY + 1),
                      static_cast<int16_t>(s.shipX + 3), static_cast<int16_t>(shipY + 1), ship);

  if (s.bulletActive) {
    target.drawFastVLine(static_cast<int16_t>(s.bulletX), static_cast<int16_t>(s.bulletY), 3, shot);
  }

  drawDigits(target, plates_, nullptr);
  if (showDate_) drawDateRow(target, frame, CRGB(140, 140, 140));
}

// ---------------------------------------------------------------------------
// Dino
// ---------------------------------------------------------------------------
struct ClockDino::State {
  float dinoY;
  float dinoV;
  bool jumping;
  float groundPhase;
  float cactusX[3];
  uint8_t cactusType[3];
  float cloudX[3];
  float cloudY[3];
  float pteroX;
  bool pteroActive;
  float pteroY;
  float frameTimer;
  uint8_t frame;
  float jumpTimer;
  int8_t offset[5];
  float offsetV[5];
  uint16_t seed;
};

ClockDino::ClockDino() : state_(nullptr) {}

void ClockDino::resetClock() {
  if (!state_) state_ = new (std::nothrow) State();
  if (!state_) return;
  State& s = *state_;
  s.dinoY = 0.0f;
  s.dinoV = 0.0f;
  s.jumping = false;
  s.groundPhase = 0.0f;
  const float width = static_cast<float>(screen_.width);
  for (uint8_t i = 0; i < 3; ++i) {
    s.cactusX[i] = width * (0.4f + 0.4f * i);
    s.cactusType[i] = static_cast<uint8_t>(i % 2);
    s.cloudX[i] = width * (0.1f + 0.35f * i);
    s.cloudY[i] = static_cast<float>(digitY() + 2 + i * 3);
  }
  s.pteroX = width;
  s.pteroActive = false;
  s.pteroY = static_cast<float>(digitY() + digitHeight() + 3);
  s.frameTimer = 0.0f;
  s.frame = 0;
  s.jumpTimer = 0.0f;
  s.seed = 0x51D6u;
  for (uint8_t i = 0; i < 5; ++i) {
    s.offset[i] = 0;
    s.offsetV[i] = 0.0f;
  }
}

void ClockDino::releaseClock() {
  delete state_;
  state_ = nullptr;
}

void ClockDino::updateClock(const FrameContext& frame, float dt) {
  if (!state_) return;
  State& s = *state_;
  dt = clampf(dt, 0.0f, 0.1f);
  const float width = static_cast<float>(screen_.width);
  const float pace = 18.0f + static_cast<float>(speed_) * 5.0f;

  s.groundPhase += pace * dt;
  if (s.groundPhase > 9999.0f) s.groundPhase = 0.0f;

  s.frameTimer += dt;
  if (s.frameTimer > 0.14f) {
    s.frameTimer = 0.0f;
    s.frame = static_cast<uint8_t>((s.frame + 1) % 2);
  }

  s.jumpTimer += dt;
  if (!s.jumping && (minuteChanged() || s.jumpTimer > 6.5f)) {
    s.jumping = true;
    s.dinoV = -46.0f;
    s.jumpTimer = 0.0f;
  }
  if (s.jumping) {
    s.dinoY += s.dinoV * dt;
    s.dinoV += 150.0f * dt;
    if (s.dinoY >= 0.0f) {
      s.dinoY = 0.0f;
      s.dinoV = 0.0f;
      s.jumping = false;
    }
  }

  for (uint8_t i = 0; i < 3; ++i) {
    s.cactusX[i] -= pace * (0.55f + 0.05f * i) * dt;
    if (s.cactusX[i] < -6.0f) {
      s.cactusX[i] = width + 10.0f + static_cast<float>(nextRandom(s.seed) % 20u);
      s.cactusType[i] = static_cast<uint8_t>(nextRandom(s.seed) % 2u);
    }
  }

  for (uint8_t i = 0; i < 3; ++i) {
    s.cloudX[i] -= pace * 0.12f * dt;
    if (s.cloudX[i] < -14.0f) s.cloudX[i] = width + 6.0f;
  }

  if (!s.pteroActive && (nextRandom(s.seed) % 900u) == 0u) {
    s.pteroActive = true;
    s.pteroX = width + 6.0f;
  }
  if (s.pteroActive) {
    s.pteroX -= pace * 1.4f * dt;
    if (s.pteroX < -8.0f) s.pteroActive = false;
  }

  if (minuteChanged()) {
    for (uint8_t i = 0; i < 5; ++i) {
      if (i != changedIndex()) continue;
      s.offset[i] = static_cast<int8_t>(-digitHeight());
      s.offsetV[i] = 0.0f;
    }
  }
  for (uint8_t i = 0; i < 5; ++i) springStep(&s.offset[i], &s.offsetV[i], dt);
  (void)frame;
}

void ClockDino::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!state_) return;
  if (!haveTime()) {
    drawSyncNotice(target, frame);
    return;
  }
  State& s = *state_;
  const int16_t height = static_cast<int16_t>(screen_.height);
  const int16_t groundY = static_cast<int16_t>(height - 3);
  const CRGB dino(140, 200, 140);
  const CRGB cloud(80, 90, 110);
  const CRGB cactus(90, 190, 110);
  const CRGB ground(180, 170, 140);

  for (uint8_t i = 0; i < 3; ++i) {
    target.drawFastHLine(static_cast<int16_t>(s.cloudX[i]), static_cast<int16_t>(s.cloudY[i]), 6, cloud);
    target.drawFastHLine(static_cast<int16_t>(s.cloudX[i] + 2.0f),
                         static_cast<int16_t>(s.cloudY[i] + 1), 5, cloud);
  }

  target.drawFastHLine(0, groundY, static_cast<int16_t>(screen_.width), ground);
  for (int16_t gx = 0; gx < static_cast<int16_t>(screen_.width); gx += 16) {
    const int16_t offset = static_cast<int16_t>(fmodf(s.groundPhase, 16.0f));
    target.drawFastHLine(static_cast<int16_t>(gx - offset + 4), static_cast<int16_t>(groundY + 1), 3,
                         ground);
  }

  for (uint8_t i = 0; i < 3; ++i) {
    const int16_t cx = static_cast<int16_t>(s.cactusX[i]);
    if (cx < -4 || cx > static_cast<int16_t>(screen_.width) + 4) continue;
    const int16_t baseY = static_cast<int16_t>(groundY - 1);
    target.fillRect(cx, static_cast<int16_t>(baseY - 4), 2, 5, cactus);
    target.fillRect(static_cast<int16_t>(cx - 1), static_cast<int16_t>(baseY - 6), 1, 3, cactus);
    if (s.cactusType[i]) {
      target.fillRect(static_cast<int16_t>(cx + 2), static_cast<int16_t>(baseY - 5), 1, 2, cactus);
    }
  }

  const char* const* dinoRows = s.frame ? kDinoRunB : kDinoRunA;
  const int16_t dinoX = 3;
  const int16_t dinoYy = static_cast<int16_t>(groundY - 9 + s.dinoY);
  const char* const dinoKeys = "GW";
  const CRGB dinoColors[2] = {dino, CRGB(255, 255, 255)};
  drawSprite(target, dinoX, dinoYy, dinoRows, 9, dinoKeys, dinoColors, 2);

  if (s.pteroActive) {
    const char* const pteroKeys = "G";
    const CRGB pteroColors[1] = {CRGB(220, 120, 120)};
    drawSprite(target, static_cast<int16_t>(s.pteroX), static_cast<int16_t>(s.pteroY), kPtero, 5,
               pteroKeys, pteroColors, 1);
  }

  drawDigits(target, plates_, s.offset);
  if (showDate_) drawDateRow(target, frame, CRGB(150, 150, 150));
}

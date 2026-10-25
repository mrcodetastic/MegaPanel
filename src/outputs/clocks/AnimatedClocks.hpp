#pragma once

#include "AnimatedClock.hpp"

// Extra schema fragments shared by the animated clocks.
void noExtraSchema(JsonObject& out);
void clockWorldSchema(JsonObject& out);

// Common metadata block: every animated clock is a foreground display output
// that needs the clock, runs at 30 fps by default and prefers 25 s in a
// playlist.
#define ANIMATED_CLOCK_META(CLASS, ID, NAME, SOURCE, NEEDS_WX, EXTRA_SCHEMA)      \
 public:                                                                          \
  const OutputMeta& meta() const override { return metaStatic(); }                 \
  static const OutputMeta& metaStatic() {                                          \
    static const OutputMeta m = {                                                  \
        ID, NAME, "1.0", SOURCE, OutputCategory::DisplayOutput, LAYER_FG,          \
        LayerId::Foreground, ScreenSpec{0, 0}, ScreenSpec{32, 24}, 0, 0, 30, 25,   \
        true, true, false, NEEDS_WX, false, false};                                \
    return m;                                                                      \
  }                                                                                \
  static void schemaStatic(JsonObject& out) {                                      \
    commonSchema(out);                                                             \
    EXTRA_SCHEMA(out);                                                             \
  }                                                                                \
  void buildConfigSchema(JsonObject& out) const override { schemaStatic(out); }    \
 public:

// ---------------------------------------------------------------------------
// Arcade: each clock is a small game behind or around the digits.
// ---------------------------------------------------------------------------
class ClockMario : public AnimatedClock {
  ANIMATED_CLOCK_META(ClockMario, "clock.mario", "Mario clock",
                      "Ported from animatedpixelclock clock_mario.cpp", false, noExtraSchema)
 public:
  ClockMario();
  void resetClock() override;
  void releaseClock() override;
  void updateClock(const FrameContext& frame, float dt) override;
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct State;
  State* state_;
};

class ClockSpace : public AnimatedClock {
  ANIMATED_CLOCK_META(ClockSpace, "clock.space", "Space invaders clock",
                      "Ported from animatedpixelclock clock_space.cpp", false, noExtraSchema)
 public:
  ClockSpace();
  void resetClock() override;
  void releaseClock() override;
  void updateClock(const FrameContext& frame, float dt) override;
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct State;
  State* state_;
};

class ClockPong : public AnimatedClock {
  ANIMATED_CLOCK_META(ClockPong, "clock.pong", "Pong clock",
                      "Ported from animatedpixelclock clock_pong.cpp", false, noExtraSchema)
 public:
  ClockPong();
  void resetClock() override;
  void releaseClock() override;
  void updateClock(const FrameContext& frame, float dt) override;
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct State;
  State* state_;
};

class ClockPacman : public AnimatedClock {
  ANIMATED_CLOCK_META(ClockPacman, "clock.pacman", "Pac-Man clock",
                      "Ported from animatedpixelclock clock_pacman.cpp", false, noExtraSchema)
 public:
  ClockPacman();
  void resetClock() override;
  void releaseClock() override;
  void updateClock(const FrameContext& frame, float dt) override;
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct State;
  State* state_;
};

class ClockBomberman : public AnimatedClock {
  ANIMATED_CLOCK_META(ClockBomberman, "clock.bomberman", "Bomberman clock",
                      "Ported from animatedpixelclock clock_bomberman.cpp", false, noExtraSchema)
 public:
  ClockBomberman();
  void resetClock() override;
  void releaseClock() override;
  void updateClock(const FrameContext& frame, float dt) override;
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct State;
  State* state_;
};

class ClockTron : public AnimatedClock {
  ANIMATED_CLOCK_META(ClockTron, "clock.tron", "TRON clock",
                      "Ported from animatedpixelclock clock_tron.cpp", false, noExtraSchema)
 public:
  ClockTron();
  void resetClock() override;
  void releaseClock() override;
  void updateClock(const FrameContext& frame, float dt) override;
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct State;
  State* state_;
};

// ---------------------------------------------------------------------------
// Motion: physics or scroll-driven scenes.
// ---------------------------------------------------------------------------
class ClockSnake : public AnimatedClock {
  ANIMATED_CLOCK_META(ClockSnake, "clock.snake", "Snake clock",
                      "Ported from animatedpixelclock clock_snake.cpp", false, noExtraSchema)
 public:
  ClockSnake();
  void resetClock() override;
  void releaseClock() override;
  void updateClock(const FrameContext& frame, float dt) override;
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct State;
  State* state_;
};

class ClockTetris : public AnimatedClock {
  ANIMATED_CLOCK_META(ClockTetris, "clock.tetris", "Tetris clock",
                      "Ported from animatedpixelclock clock_tetris.cpp", false, noExtraSchema)
 public:
  ClockTetris();
  void resetClock() override;
  void releaseClock() override;
  void updateClock(const FrameContext& frame, float dt) override;
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct State;
  State* state_;
};

class ClockAsteroids : public AnimatedClock {
  ANIMATED_CLOCK_META(ClockAsteroids, "clock.asteroids", "Asteroids clock",
                      "Ported from animatedpixelclock clock_asteroids.cpp", false, noExtraSchema)
 public:
  ClockAsteroids();
  void resetClock() override;
  void releaseClock() override;
  void updateClock(const FrameContext& frame, float dt) override;
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct State;
  State* state_;
};

class ClockDino : public AnimatedClock {
  ANIMATED_CLOCK_META(ClockDino, "clock.dino", "Dino clock",
                      "Ported from animatedpixelclock clock_dino.cpp", false, noExtraSchema)
 public:
  ClockDino();
  void resetClock() override;
  void releaseClock() override;
  void updateClock(const FrameContext& frame, float dt) override;
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct State;
  State* state_;
};

// ---------------------------------------------------------------------------
// Scenes: full-screen displays rather than digits over a moving background.
// ---------------------------------------------------------------------------
class ClockMatrix : public AnimatedClock {
  ANIMATED_CLOCK_META(ClockMatrix, "clock.matrix", "Matrix rain clock",
                      "Ported from animatedpixelclock clock_matrix.cpp", false, noExtraSchema)
 public:
  ClockMatrix();
  void resetClock() override;
  void releaseClock() override;
  void updateClock(const FrameContext& frame, float dt) override;
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct State;
  State* state_;
};

class ClockWeather : public AnimatedClock {
  ANIMATED_CLOCK_META(ClockWeather, "clock.weather", "Weather clock",
                      "Ported from animatedpixelclock clock_weather.cpp", true, noExtraSchema)
 public:
  ClockWeather();
  void resetClock() override;
  void releaseClock() override;
  void updateClock(const FrameContext& frame, float dt) override;
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;

 private:
  struct State;
  State* state_;
};

class ClockWorld : public AnimatedClock {
  ANIMATED_CLOCK_META(ClockWorld, "clock.world", "World clock",
                      "Ported from animatedpixelclock worldclock/", false, clockWorldSchema)
 public:
  ClockWorld();
  bool configure(const ScreenSpec& screen, JsonObjectConst config) override;
  void resetClock() override;
  void releaseClock() override;
  void updateClock(const FrameContext& frame, float dt) override;
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;
  void writeConfig(JsonObject& out) const override;
  void onConfigChanged(JsonObjectConst config) override;

 private:
  struct State;
  State* state_;
  bool rotate_;
  bool showMap_;
};

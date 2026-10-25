#pragma once

#include "SimpleClock.hpp"

#define STATIC_CLOCK_META(CLASS, ID, NAME)                                          \
 public:                                                                            \
  const OutputMeta& meta() const override { return metaStatic(); }                  \
  static const OutputMeta& metaStatic() {                                           \
    static const OutputMeta m = {ID, NAME, "1.0", "MegaPanel",                      \
                                 OutputCategory::DisplayOutput, LAYER_FG,           \
                                 LayerId::Foreground, ScreenSpec{0, 0},             \
                                 ScreenSpec{32, 24}, 0, 0, 5, 25, true, true,       \
                                 false, false, false, false};                       \
    return m;                                                                       \
  }                                                                                 \
  void buildConfigSchema(JsonObject& out) const override { schemaStatic(out); }     \
  static void schemaStatic(JsonObject& out) { SimpleClock::commonSchema(out); }

class ClockLarge : public SimpleClock {
  STATIC_CLOCK_META(ClockLarge, "clock.large", "Large clock")
 protected:
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;
};

class ClockWords : public SimpleClock {
  STATIC_CLOCK_META(ClockWords, "clock.words", "Time in words")
 protected:
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;
};

class ClockWorldMapCw : public SimpleClock {
  STATIC_CLOCK_META(ClockWorldMapCw, "clock.worldmap.cw", "World map clock")
 protected:
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;
};

class ClockMarioCw : public SimpleClock {
  STATIC_CLOCK_META(ClockMarioCw, "clock.mario.cw", "Mario clock")
 protected:
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;
};

class ClockPacmanCw : public SimpleClock {
  STATIC_CLOCK_META(ClockPacmanCw, "clock.pacman.cw", "Pac-Man clock")
 protected:
  void drawClock(PixelCanvas& target, const FrameContext& frame) override;
};

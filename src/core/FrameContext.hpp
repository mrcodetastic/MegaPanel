#pragma once

#include <time.h>
#include "Types.hpp"

struct WeatherData;  // implemented by WeatherService later

// Everything an output needs to render one frame. All fields are read-only to
// outputs; services own the underlying state.
struct FrameContext {
  uint32_t nowMs;
  uint32_t deltaMs;
  float deltaSec;
  uint32_t frameIndex;
  ScreenSpec screen;
  struct tm localTime;
  int64_t epoch;
  bool timeValid;
  bool ntpSynced;
  bool use24Hour;
  uint8_t dateFormat;
  const WeatherData* weather;
  uint8_t luxPercent;
  bool presence;

  FrameContext()
      : nowMs(0),
        deltaMs(0),
        deltaSec(0.0f),
        frameIndex(0),
        screen{0, 0},
        localTime{},
        epoch(0),
        timeValid(false),
        ntpSynced(false),
        use24Hour(true),
        dateFormat(0),
        weather(nullptr),
        luxPercent(100),
        presence(true) {}
};

#pragma once

#include "Types.hpp"

// Static description of an output. This is compiled into the firmware; it is
// not allocated at runtime.
struct OutputMeta {
  const char* id;
  const char* name;
  const char* version;
  const char* author;
  OutputCategory category;
  uint8_t allowedLayers;
  LayerId defaultLayer;
  ScreenSpec designSize;
  ScreenSpec minSize;
  uint16_t maxWidth;
  uint16_t maxHeight;
  uint8_t defaultFps;
  uint16_t defaultDwellSec;
  bool animated;
  bool needsTime;
  bool needsNetwork;
  bool needsWeather;
  bool needsFilesystem;
  bool optionalFeature;
};

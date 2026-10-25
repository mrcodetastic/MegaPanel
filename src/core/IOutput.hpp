#pragma once

#include <ArduinoJson.h>
#include "OutputMeta.hpp"
#include "Types.hpp"

class PixelCanvas;
class OutputContext;
struct FrameContext;

// Base class for every pattern, clock, media output and system overlay.
//
// The object is constructed once at boot (or as a static instance) and lives
// for the firmware's lifetime. Per-output buffers are reserved in start() and
// released in stop(). configure() is called while stopped.
class IOutput {
 public:
  virtual ~IOutput() = default;

  virtual const OutputMeta& meta() const = 0;

  // Store the screen and configuration. No heavy allocation here.
  virtual bool configure(const ScreenSpec& screen, JsonObjectConst config) = 0;

  // Reserve every buffer this output needs and prepare state.
  virtual bool start(OutputContext& ctx) = 0;

  // Free everything reserved by start() and return to a clean stopped state.
  virtual void stop() = 0;

  // Draw one frame into the layer owned by the SceneManager.
  virtual void render(PixelCanvas& target, const FrameContext& frame) = 0;

  // Apply live-safe settings (colour, speed, opacity). Structural changes go
  // through stop(), configure(), start().
  virtual void onConfigChanged(JsonObjectConst config) { (void)config; }

  virtual void buildConfigSchema(JsonObject& out) const = 0;
  virtual void writeConfig(JsonObject& out) const = 0;

  virtual bool isAnimated() const { return meta().animated; }
  virtual uint8_t preferredFps() const { return meta().defaultFps; }
  virtual bool wantsFullClear() const { return true; }
};

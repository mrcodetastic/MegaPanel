#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Lightweight runtime diagnostics shared by /api/status and the serial log.
class DiagnosticsService {
 public:
  DiagnosticsService();

  void begin();
  void loopStart();
  void loopEnd();

  void noteFrame(uint32_t renderUs, uint32_t composeUs, uint32_t showUs, uint8_t targetFps);

  uint32_t fps() const { return fps_; }
  uint32_t loopMaxUs() const { return loopMaxUs_; }
  uint32_t renderUs() const { return renderUs_; }
  uint32_t composeUs() const { return composeUs_; }
  uint32_t showUs() const { return showUs_; }
  uint8_t targetFps() const { return targetFps_; }

  void toJson(JsonObject out) const;

 private:
  uint32_t loopStartUs_;
  uint32_t loopMaxUs_;
  uint32_t loopCount_;
  uint32_t frameCount_;
  uint32_t framesThisSecond_;
  uint32_t lastFpsMs_;
  uint32_t fps_;
  uint32_t renderUs_;
  uint32_t composeUs_;
  uint32_t showUs_;
  uint8_t targetFps_;
};

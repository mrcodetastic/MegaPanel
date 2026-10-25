#pragma once

#include <Arduino.h>
#include "core/Types.hpp"

// Process-wide notification state. The system overlay reads it; the web API
// writes it.
class NotificationService {
 public:
  static NotificationService& instance();

  void show(const char* text, CRGB color, int8_t icon, uint32_t durationMs, uint8_t position);
  void dismiss();
  bool active();

  const char* text() const { return text_; }
  CRGB color() const { return color_; }
  int8_t icon() const { return icon_; }
  uint8_t position() const { return position_; }
  uint32_t remainingMs();

 private:
  NotificationService();

  char text_[201];
  CRGB color_;
  int8_t icon_;
  uint8_t position_;
  uint32_t startMs_;
  uint32_t durationMs_;
  bool enabled_;
};

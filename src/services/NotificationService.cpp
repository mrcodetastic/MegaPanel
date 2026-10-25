#include "NotificationService.hpp"

#include <cstring>

NotificationService& NotificationService::instance() {
  static NotificationService service;
  return service;
}

NotificationService::NotificationService()
    : color_(CRGB::White), icon_(-1), position_(0), startMs_(0), durationMs_(0), enabled_(false) {
  text_[0] = '\0';
}

void NotificationService::show(const char* text, CRGB color, int8_t icon,
                               uint32_t durationMs, uint8_t position) {
  if (!text) text = "";
  strncpy(text_, text, sizeof(text_) - 1);
  text_[sizeof(text_) - 1] = '\0';
  color_ = color;
  icon_ = icon;
  position_ = position ? 1 : 0;
  durationMs_ = durationMs == 0 ? 5000 : durationMs;
  if (durationMs_ > 60000) durationMs_ = 60000;
  startMs_ = millis();
  enabled_ = true;
}

void NotificationService::dismiss() {
  enabled_ = false;
  text_[0] = '\0';
}

bool NotificationService::active() {
  if (!enabled_) return false;
  if (millis() - startMs_ >= durationMs_) {
    enabled_ = false;
    return false;
  }
  return true;
}

uint32_t NotificationService::remainingMs() {
  if (!enabled_) return 0;
  const uint32_t elapsed = millis() - startMs_;
  return elapsed >= durationMs_ ? 0 : durationMs_ - elapsed;
}

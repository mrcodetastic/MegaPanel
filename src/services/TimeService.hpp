#pragma once

#include <time.h>
#include "app/AppConfig.hpp"

// Owns the system clock and timezone. Uses the ESP32 native SNTP client and
// POSIX timezone strings; no third-party time library is required.
class TimeService {
 public:
  TimeService();

  void begin(const TimeConfig& config);
  void loop();

  const struct tm& localTime() const { return localTime_; }
  int64_t epoch() const { return epoch_; }
  bool valid() const { return valid_; }
  bool synced() const { return synced_; }
  uint16_t minuteOfDay() const;

  void formatTime(char* out, size_t capacity, bool use24Hour, bool showColon) const;
  void formatDate(char* out, size_t capacity, uint8_t dateFormat) const;

  const TimeConfig& config() const { return config_; }

 private:
  TimeConfig config_;
  struct tm localTime_;
  int64_t epoch_;
  bool valid_;
  bool synced_;
};

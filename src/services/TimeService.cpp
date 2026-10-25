#include "TimeService.hpp"

#include <Arduino.h>
#include <esp_sntp.h>
#include <string.h>
#include "util/Logger.hpp"

TimeService::TimeService() : epoch_(0), valid_(false), synced_(false) {
  memset(&localTime_, 0, sizeof(localTime_));
}

void TimeService::begin(const TimeConfig& config) {
  config_ = config;

  const char* posix = config_.posix[0] ? config_.posix : "UTC0";
  const char* ntp1 = config_.ntp1[0] ? config_.ntp1 : "pool.ntp.org";
  const char* ntp2 = config_.ntp2[0] ? config_.ntp2 : nullptr;

  configTzTime(posix, ntp1, ntp2);
  LOGI("time", "SNTP started: tz=%s ntp1=%s ntp2=%s", posix, ntp1, ntp2 ? ntp2 : "-");

  loop();
}

void TimeService::loop() {
  const time_t now = time(nullptr);
  epoch_ = static_cast<int64_t>(now);
  localtime_r(&now, &localTime_);
  valid_ = localTime_.tm_year > 120;  // 2020 or later
  synced_ = valid_ && sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED;
}

uint16_t TimeService::minuteOfDay() const {
  if (!valid_) return 0;
  return static_cast<uint16_t>(localTime_.tm_hour * 60 + localTime_.tm_min);
}

void TimeService::formatTime(char* out, size_t capacity, bool use24Hour, bool showColon) const {
  if (!out || capacity == 0) return;
  if (!valid_) {
    snprintf(out, capacity, "--%c--", showColon ? ':' : ' ');
    return;
  }

  int hour = localTime_.tm_hour;
  if (!use24Hour) {
    hour %= 12;
    if (hour == 0) hour = 12;
  }
  snprintf(out, capacity, "%02d%c%02d", hour, showColon ? ':' : ' ', localTime_.tm_min);
}

void TimeService::formatDate(char* out, size_t capacity, uint8_t dateFormat) const {
  if (!out || capacity == 0) return;
  if (!valid_) {
    snprintf(out, capacity, "--/--/----");
    return;
  }

  const int day = localTime_.tm_mday;
  const int month = localTime_.tm_mon + 1;
  const int year = localTime_.tm_year + 1900;

  switch (dateFormat) {
    case 1:
      snprintf(out, capacity, "%02d/%02d/%04d", month, day, year);
      break;
    case 2:
      snprintf(out, capacity, "%04d-%02d-%02d", year, month, day);
      break;
    case 3:
      snprintf(out, capacity, "%02d.%02d.%04d", day, month, year);
      break;
    case 0:
    default:
      snprintf(out, capacity, "%02d/%02d/%04d", day, month, year);
      break;
  }
}

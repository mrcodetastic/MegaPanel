#include "BrightnessService.hpp"

#include <string.h>
#include "hal/IMatrixPanel.hpp"
#include "services/ConfigService.hpp"
#include "services/LightSensorService.hpp"
#include "services/PresenceService.hpp"
#include "services/TimeService.hpp"
#include "util/Logger.hpp"

namespace {

bool minuteInWindow(uint16_t now, uint16_t start, uint16_t end) {
  if (start == end) return false;
  if (start < end) return now >= start && now < end;
  return now >= start || now < end;
}

uint8_t clampByte(int value, uint8_t low, uint8_t high) {
  if (value < low) return low;
  if (value > high) return high;
  return static_cast<uint8_t>(value);
}

void copyReason(char* destination, size_t capacity, const char* source) {
  if (!destination || capacity == 0) return;
  strncpy(destination, source ? source : "", capacity - 1);
  destination[capacity - 1] = '\0';
}

}  // namespace

BrightnessService::BrightnessService()
    : panel_(nullptr),
      config_(nullptr),
      light_(nullptr),
      presence_(nullptr),
      time_(nullptr),
      lastCheckMs_(0),
      target_(128),
      lastApplied_(255),
      forcedOff_(false),
      forceNext_(true),
      reason_{0} {
  copyReason(reason_, sizeof(reason_), "init");
}

void BrightnessService::begin(IMatrixPanel* panel,
                              ConfigService* config,
                              LightSensorService* light,
                              PresenceService* presence,
                              TimeService* time) {
  panel_ = panel;
  config_ = config;
  light_ = light;
  presence_ = presence;
  time_ = time;
  forceNext_ = true;
  lastCheckMs_ = 0;
  updateTarget(millis());
  apply();
}

void BrightnessService::loop(uint32_t nowMs) {
  if (!forceNext_ && nowMs - lastCheckMs_ < 500) return;
  lastCheckMs_ = nowMs;
  forceNext_ = false;
  updateTarget(nowMs);
  apply();
}

void BrightnessService::updateTarget(uint32_t nowMs) {
  (void)nowMs;
  if (!config_) return;

  const BrightnessConfig& brightness = config_->get().brightness;
  uint8_t value = clampByte(brightness.manual, 1, 255);
  const char* why = "manual";

  if (forcedOff_) {
    value = 0;
    why = "forced off";
  } else if (presence_ && presence_->config().enabled && !presence_->present() &&
             presence_->config().absentAction == 0) {
    value = 0;
    why = "PIR absent";
  } else {
    const uint16_t nowMinute = time_ ? time_->minuteOfDay() : 0;
    const bool timeValid = time_ && time_->valid();

    if (timeValid && brightness.offEnabled &&
        minuteInWindow(nowMinute, brightness.offStartMin, brightness.offEndMin)) {
      value = 0;
      why = "schedule off";
    } else if (timeValid && brightness.dimEnabled &&
               minuteInWindow(nowMinute, brightness.dimStartMin, brightness.dimEndMin)) {
      value = clampByte(brightness.dimLevel, 1, 255);
      why = "schedule dim";
    } else if (light_ && light_->config().enabled && light_->valid()) {
      const uint8_t percent = light_->percent();
      const uint8_t low = config_->get().light.minBrightness;
      const uint8_t high = config_->get().light.maxBrightness;
      value = static_cast<uint8_t>(low + (static_cast<uint16_t>(high - low) * percent) / 100u);
      why = "LDR";
    }

    if (presence_ && presence_->config().enabled && !presence_->present() &&
        presence_->config().absentAction == 1) {
      value = value < presence_->config().absentDim ? value : presence_->config().absentDim;
      why = "PIR dim";
    }
  }

  target_ = clampByte(value, 0, 255);
  copyReason(reason_, sizeof(reason_), why);
}

void BrightnessService::apply() {
  if (!panel_) return;
  if (target_ == lastApplied_) return;
  panel_->setBrightness(target_);
  lastApplied_ = target_;
  LOGD("bright", "set %u (%s)", target_, reason_);
}

void BrightnessService::setManual(uint8_t value) {
  if (!config_) return;
  config_->mutableConfig().brightness.manual = clampByte(value, 1, 255);
  config_->requestSave();
  forceNext_ = true;
}

void BrightnessService::setForcedOff(bool forced) {
  forcedOff_ = forced;
  forceNext_ = true;
}

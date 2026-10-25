#pragma once

#include <Arduino.h>

class IMatrixPanel;
class ConfigService;
class LightSensorService;
class PresenceService;
class TimeService;

// Resolves the display target from manual brightness, schedule, LDR and PIR.
class BrightnessService {
 public:
  BrightnessService();

  void begin(IMatrixPanel* panel,
             ConfigService* config,
             LightSensorService* light,
             PresenceService* presence,
             TimeService* time);

  void loop(uint32_t nowMs);

  void setManual(uint8_t value);
  void setForcedOff(bool forced);
  bool forcedOff() const { return forcedOff_; }

  uint8_t target() const { return target_; }
  const char* reason() const { return reason_; }

 private:
  void updateTarget(uint32_t nowMs);
  void apply();

  IMatrixPanel* panel_;
  ConfigService* config_;
  LightSensorService* light_;
  PresenceService* presence_;
  TimeService* time_;
  uint32_t lastCheckMs_;
  uint8_t target_;
  uint8_t lastApplied_;
  bool forcedOff_;
  bool forceNext_;
  char reason_[24];
};

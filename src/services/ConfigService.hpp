#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "app/AppConfig.hpp"

// Loads, validates and persists the configuration. Core settings are stored as
// one JSON document in NVS; per-output settings are kept in the same document
// under the "outputs" object. Writes are debounced to reduce flash wear.
class ConfigService {
 public:
  ConfigService();

  bool begin();
  void loop(uint32_t nowMs);

  const AppConfig& get() const { return config_; }
  AppConfig& mutableConfig() { return config_; }

  void requestSave();
  void saveNow();
  void reset();

  void toJson(JsonObject root) const;
  bool fromJson(JsonObjectConst root);

  JsonVariantConst outputConfig(const char* id) const;
  JsonObjectConst outputs() const;

  bool setScene(const char* backgroundId,
                const char* foregroundId,
                uint8_t backgroundAlpha,
                uint8_t foregroundAlpha,
                uint8_t systemAlpha,
                bool statusCorners,
                bool notifications);

  const char* lastError() const { return lastError_; }

 private:
  bool load();
  void setError(const char* message);

  Preferences preferences_;
  AppConfig config_;
  JsonDocument outputsDocument_;
  bool dirty_;
  uint32_t saveAtMs_;
  char lastError_[96];
};

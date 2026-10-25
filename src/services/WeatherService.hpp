#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <new>

#include "app/AppConfig.hpp"

// Condition groups used by the weather clock and the icons. They follow the
// Open-Meteo / WMO weather-code families, collapsed to what a 16x16 glyph can
// actually say.
enum class WeatherIcon : uint8_t {
  Unknown = 0,
  Clear,
  PartlyCloudy,
  Cloudy,
  Fog,
  Drizzle,
  Rain,
  Snow,
  Thunder,
  Wind,
};

// Immutable snapshot an output renders from. Copying it is cheap (under 48
// bytes), so the render loop never shares memory with the network task.
struct WeatherData {
  bool valid;
  bool stale;
  bool configured;
  uint32_t updatedMs;
  int16_t tempC;
  int16_t tempMaxC;
  int16_t tempMinC;
  uint8_t humidity;
  uint16_t code;
  uint8_t windKph;
  WeatherIcon icon;
  char sunrise[6];
  char sunset[6];
  bool fahrenheit;

  WeatherData();

  uint32_t ageMs(uint32_t nowMs) const;
  int16_t displayTemp(bool fahrenheit) const;
  int16_t displayTempMax(bool fahrenheit) const;
  int16_t displayTempMin(bool fahrenheit) const;
};

WeatherIcon weatherIconFromCode(int code);
const char* weatherIconName(WeatherIcon icon);

// Fetches the current conditions and today's summary from Open-Meteo. The
// request runs in its own FreeRTOS task so the render loop is never blocked;
// results are published under a mutex and read as a snapshot.
class WeatherService {
 public:
  WeatherService();
  ~WeatherService();

  void begin(const WeatherConfig& config);
  void applyConfig(const WeatherConfig& config);
  void loop(uint32_t nowMs);

  // Force a fetch on the next loop() pass, e.g. after the user saves a new
  // location.
  void requestRefresh();

  WeatherData snapshot() const;
  bool fetching() const;
  const char* lastError() const;

  void toJson(JsonObject& out, uint32_t nowMs) const;

 private:
  static void fetchTask(void* argument);
  void runFetch(float latitude, float longitude);
  void publish(const WeatherData& data);
  WeatherData copy() const;
  void setError(const char* message);

  mutable SemaphoreHandle_t mutex_;
  WeatherData data_;
  WeatherConfig config_;
  bool busy_;
  bool refreshRequested_;
  uint32_t nextAttemptMs_;
  uint32_t backoffMs_;
  TaskHandle_t task_;
  char lastError_[80];
};

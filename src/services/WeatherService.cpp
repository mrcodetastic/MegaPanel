#include "WeatherService.hpp"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cstdio>
#include <cstring>
#include <new>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "util/Logger.hpp"

namespace {

constexpr uint32_t kDefaultRefreshMs = 15u * 60u * 1000u;
constexpr uint32_t kMinBackoffMs = 60u * 1000u;
constexpr uint32_t kMaxBackoffMs = 30u * 60u * 1000u;
constexpr uint32_t kStaleAfterMs = 3u * 60u * 60u * 1000u;
constexpr uint32_t kHttpTimeoutMs = 8000;

void copyText(char* destination, size_t capacity, const char* source) {
  if (!destination || capacity == 0) return;
  if (!source) {
    destination[0] = '\0';
    return;
  }
  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

// "2026-10-25T07:35" -> "07:35". Leaves the field empty when Open-Meteo omits
// the array element (polar day/night).
void copyClockTail(char* destination, size_t capacity, const char* iso) {
  if (!destination || capacity == 0) return;
  destination[0] = '\0';
  if (!iso) return;
  const size_t length = strlen(iso);
  if (length < 5) return;
  copyText(destination, capacity, iso + length - 5);
}

int16_t roundToInt16(float value) {
  const float rounded = value >= 0.0f ? value + 0.5f : value - 0.5f;
  return static_cast<int16_t>(rounded);
}

// Handed to the worker task so it never reads config_ while the web thread is
// rewriting it. The task owns the allocation.
struct FetchRequest {
  class WeatherService* self;
  float latitude;
  float longitude;
};

}  // namespace

WeatherData::WeatherData()
    : valid(false),
      stale(false),
      configured(false),
      updatedMs(0),
      tempC(0),
      tempMaxC(0),
      tempMinC(0),
      humidity(0),
      code(0),
      windKph(0),
      icon(WeatherIcon::Unknown),
      sunrise{0},
      sunset{0},
      fahrenheit(false) {}

uint32_t WeatherData::ageMs(uint32_t nowMs) const {
  if (!valid) return 0;
  return nowMs - updatedMs;
}

int16_t WeatherData::displayTemp(bool fahrenheit) const {
  if (!fahrenheit) return tempC;
  return roundToInt16(static_cast<float>(tempC) * 9.0f / 5.0f + 32.0f);
}

int16_t WeatherData::displayTempMax(bool fahrenheit) const {
  if (!fahrenheit) return tempMaxC;
  return roundToInt16(static_cast<float>(tempMaxC) * 9.0f / 5.0f + 32.0f);
}

int16_t WeatherData::displayTempMin(bool fahrenheit) const {
  if (!fahrenheit) return tempMinC;
  return roundToInt16(static_cast<float>(tempMinC) * 9.0f / 5.0f + 32.0f);
}

WeatherIcon weatherIconFromCode(int code) {
  switch (code) {
    case 0: return WeatherIcon::Clear;
    case 1:
    case 2: return WeatherIcon::PartlyCloudy;
    case 3: return WeatherIcon::Cloudy;
    case 45:
    case 48: return WeatherIcon::Fog;
    case 51:
    case 53:
    case 55: return WeatherIcon::Drizzle;
    case 56:
    case 57: return WeatherIcon::Rain;
    case 61:
    case 63:
    case 65:
    case 66:
    case 67: return WeatherIcon::Rain;
    case 71:
    case 73:
    case 75:
    case 77: return WeatherIcon::Snow;
    case 80:
    case 81:
    case 82: return WeatherIcon::Rain;
    case 85:
    case 86: return WeatherIcon::Snow;
    case 95:
    case 96:
    case 99: return WeatherIcon::Thunder;
    case 100: return WeatherIcon::Wind;
    default: return WeatherIcon::Unknown;
  }
}

const char* weatherIconName(WeatherIcon icon) {
  switch (icon) {
    case WeatherIcon::Clear: return "clear";
    case WeatherIcon::PartlyCloudy: return "partly";
    case WeatherIcon::Cloudy: return "cloudy";
    case WeatherIcon::Fog: return "fog";
    case WeatherIcon::Drizzle: return "drizzle";
    case WeatherIcon::Rain: return "rain";
    case WeatherIcon::Snow: return "snow";
    case WeatherIcon::Thunder: return "thunder";
    case WeatherIcon::Wind: return "wind";
    default: return "unknown";
  }
}

WeatherService::WeatherService()
    : mutex_(nullptr),
      data_(),
      config_(),
      busy_(false),
      refreshRequested_(false),
      nextAttemptMs_(0),
      backoffMs_(kMinBackoffMs),
      task_(nullptr),
      lastError_{0} {}

WeatherService::~WeatherService() {
  if (mutex_) {
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
  }
}

void WeatherService::begin(const WeatherConfig& config) {
  mutex_ = xSemaphoreCreateMutex();
  config_ = config;
  data_.configured = config.enabled && config.latitude != 0.0f && config.longitude != 0.0f;
  nextAttemptMs_ = millis() + 5000;
}

void WeatherService::applyConfig(const WeatherConfig& config) {
  const bool moved = config.latitude != config_.latitude || config.longitude != config_.longitude;
  config_ = config;
  data_.configured = config.enabled && config.latitude != 0.0f && config.longitude != 0.0f;
  if (moved) {
    data_.valid = false;
    data_.stale = false;
    backoffMs_ = kMinBackoffMs;
    nextAttemptMs_ = millis() + 1000;
  }
  if (!config_.enabled) data_.valid = false;
}

void WeatherService::requestRefresh() {
  refreshRequested_ = true;
  nextAttemptMs_ = millis();
  backoffMs_ = kMinBackoffMs;
}

WeatherData WeatherService::copy() const {
  WeatherData result;
  if (!mutex_) return result;
  // The render loop copies this every frame, so never wait long for the
  // network task: a missed sample just repeats the previous snapshot.
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(2)) == pdTRUE) {
    result = data_;
    xSemaphoreGive(mutex_);
  }
  return result;
}

WeatherData WeatherService::snapshot() const {
  WeatherData result = copy();
  if (result.valid && result.ageMs(millis()) > kStaleAfterMs) result.stale = true;
  result.fahrenheit = config_.fahrenheit;
  return result;
}

bool WeatherService::fetching() const { return busy_; }

const char* WeatherService::lastError() const { return lastError_; }

void WeatherService::loop(uint32_t nowMs) {
  const bool configured = config_.enabled && config_.latitude != 0.0f && config_.longitude != 0.0f;
  if (!configured) {
    data_.configured = false;
    return;
  }
  data_.configured = true;

  if (busy_) return;
  if (WiFi.status() != WL_CONNECTED) {
    data_.stale = data_.valid;
    return;
  }

  if (refreshRequested_) {
    refreshRequested_ = false;
    nextAttemptMs_ = nowMs;
  }
  if (static_cast<int32_t>(nowMs - nextAttemptMs_) < 0) return;

  FetchRequest* request = new (std::nothrow) FetchRequest{this, config_.latitude, config_.longitude};
  if (!request) {
    LOGE("weather", "no memory for fetch request");
    nextAttemptMs_ = nowMs + kMinBackoffMs;
    return;
  }

  busy_ = true;
  if (xTaskCreatePinnedToCore(fetchTask, "wx-fetch", 8192, request, 1, &task_, 0) != pdPASS) {
    busy_ = false;
    task_ = nullptr;
    delete request;
    LOGE("weather", "failed to start fetch task");
    nextAttemptMs_ = nowMs + kMinBackoffMs;
  }
}

void WeatherService::fetchTask(void* argument) {
  FetchRequest* request = static_cast<FetchRequest*>(argument);
  WeatherService* self = request ? request->self : nullptr;
  if (!self) {
    delete request;
    vTaskDelete(nullptr);
    return;
  }
  self->runFetch(request->latitude, request->longitude);
  delete request;

  const uint32_t now = millis();
  self->busy_ = false;
  self->task_ = nullptr;

  WeatherData current = self->copy();
  const uint32_t interval = self->config_.refreshMin > 0
                                ? static_cast<uint32_t>(self->config_.refreshMin) * 60000u
                                : kDefaultRefreshMs;
  if (current.valid) {
    self->backoffMs_ = kMinBackoffMs;
    self->nextAttemptMs_ = now + interval;
  } else {
    self->nextAttemptMs_ = now + self->backoffMs_;
    self->backoffMs_ = self->backoffMs_ * 2u > kMaxBackoffMs ? kMaxBackoffMs : self->backoffMs_ * 2u;
  }
  vTaskDelete(nullptr);
}

void WeatherService::runFetch(float latitude, float longitude) {
  char url[256];
  snprintf(url,
           sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
           "&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset"
           "&timezone=auto&forecast_days=1",
           static_cast<double>(latitude),
           static_cast<double>(longitude));

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(kHttpTimeoutMs / 1000);

  HTTPClient http;
  http.setConnectTimeout(kHttpTimeoutMs);
  http.setTimeout(kHttpTimeoutMs);
  http.setReuse(false);
  if (!http.begin(client, url)) {
    setError("http begin failed");
    LOGE("weather", "http begin failed");
    return;
  }

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    snprintf(lastError_, sizeof(lastError_), "http %d", status);
    LOGE("weather", "fetch failed: http %d", status);
    http.end();
    return;
  }

  WeatherData next;
  next.valid = false;
  next.stale = false;
  next.configured = true;

  JsonDocument filter;
  filter["current"]["temperature_2m"] = true;
  filter["current"]["relative_humidity_2m"] = true;
  filter["current"]["weather_code"] = true;
  filter["current"]["wind_speed_10m"] = true;
  filter["daily"]["temperature_2m_max"][0] = true;
  filter["daily"]["temperature_2m_min"][0] = true;
  filter["daily"]["sunrise"][0] = true;
  filter["daily"]["sunset"][0] = true;

  JsonDocument document;
  const DeserializationError error =
      deserializeJson(document, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (error) {
    snprintf(lastError_, sizeof(lastError_), "json: %s", error.c_str());
    LOGE("weather", "json parse failed: %s", error.c_str());
    return;
  }

  JsonObjectConst current = document["current"];
  JsonObjectConst daily = document["daily"];
  if (current.isNull() || daily.isNull()) {
    setError("missing fields");
    LOGE("weather", "response missing current/daily");
    return;
  }

  next.code = static_cast<uint16_t>(current["weather_code"] | 0);
  next.icon = weatherIconFromCode(next.code);
  next.tempC = roundToInt16(current["temperature_2m"] | 0.0f);
  next.humidity = static_cast<uint8_t>(current["relative_humidity_2m"] | 0);
  next.windKph = static_cast<uint8_t>(current["wind_speed_10m"] | 0.0f);
  next.tempMaxC = roundToInt16(daily["temperature_2m_max"][0] | 0.0f);
  next.tempMinC = roundToInt16(daily["temperature_2m_min"][0] | 0.0f);
  copyClockTail(next.sunrise, sizeof(next.sunrise), daily["sunrise"][0] | "");
  copyClockTail(next.sunset, sizeof(next.sunset), daily["sunset"][0] | "");
  next.valid = true;
  next.updatedMs = millis();
  next.stale = false;

  lastError_[0] = '\0';
  publish(next);
  LOGI("weather", "%d C, code %u, %u%% humidity", next.tempC, next.code, next.humidity);
}

void WeatherService::publish(const WeatherData& data) {
  if (!mutex_) {
    data_ = data;
    return;
  }
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
    data_ = data;
    xSemaphoreGive(mutex_);
  }
}

void WeatherService::setError(const char* message) { copyText(lastError_, sizeof(lastError_), message); }

void WeatherService::toJson(JsonObject& out, uint32_t nowMs) const {
  const WeatherData data = copy();
  out["configured"] = data.configured;
  out["enabled"] = config_.enabled;
  out["valid"] = data.valid;
  out["stale"] = data.stale || (data.valid && data.ageMs(nowMs) > kStaleAfterMs);
  out["fetching"] = busy_;
  out["latitude"] = config_.latitude;
  out["longitude"] = config_.longitude;
  out["fahrenheit"] = config_.fahrenheit;
  out["refreshMin"] = config_.refreshMin;
  out["ageSec"] = data.valid ? data.ageMs(nowMs) / 1000u : 0u;
  out["error"] = lastError_;
  if (!data.valid) return;

  out["tempC"] = data.tempC;
  out["tempMaxC"] = data.tempMaxC;
  out["tempMinC"] = data.tempMinC;
  out["temp"] = data.displayTemp(config_.fahrenheit);
  out["tempMax"] = data.displayTempMax(config_.fahrenheit);
  out["tempMin"] = data.displayTempMin(config_.fahrenheit);
  out["humidity"] = data.humidity;
  out["windKph"] = data.windKph;
  out["code"] = data.code;
  out["icon"] = weatherIconName(data.icon);
  out["sunrise"] = data.sunrise;
  out["sunset"] = data.sunset;
}

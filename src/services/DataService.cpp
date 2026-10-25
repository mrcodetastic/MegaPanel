#include "DataService.hpp"

#include <WiFi.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "util/HttpJson.hpp"
#include "util/JsonPath.hpp"
#include "util/Logger.hpp"

namespace {

constexpr uint32_t kMinBackoffMs = 60u * 1000u;
constexpr uint32_t kMaxBackoffMs = 15u * 60u * 1000u;
constexpr uint32_t kStaleFloorMs = 5u * 60u * 1000u;

void copyText(char* destination, size_t capacity, const char* source) {
  if (!destination || capacity == 0) return;
  if (!source) {
    destination[0] = '\0';
    return;
  }
  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

bool readNumber(JsonVariantConst value, float* out) {
  if (value.is<float>() || value.is<double>() || value.is<int>() || value.is<long>() ||
      value.is<unsigned long>()) {
    *out = value.as<float>();
    return true;
  }
  const char* text = value.as<const char*>();
  if (!text) return false;

  char clean[32];
  size_t length = 0;
  for (const char* p = text; *p && length < sizeof(clean) - 1; ++p) {
    const char c = *p;
    if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E') {
      clean[length++] = c;
    }
  }
  clean[length] = '\0';
  if (length == 0) return false;

  char* end = nullptr;
  const float parsed = strtof(clean, &end);
  if (!end || *end != '\0') return false;
  *out = parsed;
  return true;
}

uint32_t refreshIntervalMs(const DataSourceConfig& source) {
  const uint32_t seconds = source.refreshSec >= 10 ? source.refreshSec : 120;
  return seconds * 1000u;
}

}  // namespace

DataQuote::DataQuote()
    : valid(false),
      stale(false),
      updatedMs(0),
      price(0.0f),
      change(0.0f),
      changePercent(0.0f),
      name{0},
      error{0} {}

uint32_t DataQuote::ageMs(uint32_t nowMs) const {
  if (!valid) return 0;
  return nowMs - updatedMs;
}

QuoteTrend DataQuote::trend() const {
  const float movement = changePercent != 0.0f ? changePercent : change;
  if (movement > 0.0f) return QuoteTrend::Up;
  if (movement < 0.0f) return QuoteTrend::Down;
  return QuoteTrend::Flat;
}

DataService::DataService()
    : mutex_(nullptr),
      busy_(false),
      refreshRequested_(false),
      task_(nullptr) {
  for (uint8_t i = 0; i < DataConfig::kMaxSources; ++i) {
    nextAttemptMs_[i] = 0;
    backoffMs_[i] = kMinBackoffMs;
  }
}

DataService::~DataService() {
  if (mutex_) {
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
  }
}

void DataService::begin(const DataConfig& config) {
  mutex_ = xSemaphoreCreateMutex();
  config_ = config;
  const uint32_t now = millis();
  for (uint8_t i = 0; i < DataConfig::kMaxSources; ++i) {
    nextAttemptMs_[i] = now + 3000;
    backoffMs_[i] = kMinBackoffMs;
  }
}

void DataService::applyConfig(const DataConfig& config) {
  bool changed = config.enabled != config_.enabled || config.count != config_.count;
  for (uint8_t i = 0; i < config.count && i < DataConfig::kMaxSources && !changed; ++i) {
    if (strcmp(config.sources[i].url, config_.sources[i].url) != 0) changed = true;
  }
  config_ = config;
  if (!changed) return;

  const uint32_t now = millis();
  for (uint8_t i = 0; i < DataConfig::kMaxSources; ++i) {
    nextAttemptMs_[i] = now + 1000;
    backoffMs_[i] = kMinBackoffMs;
  }
}

void DataService::requestRefresh(uint8_t index) {
  refreshRequested_ = true;
  const uint32_t now = millis();
  if (index == 0xFF) {
    for (uint8_t i = 0; i < DataConfig::kMaxSources; ++i) {
      nextAttemptMs_[i] = now;
      backoffMs_[i] = kMinBackoffMs;
    }
  } else if (index < DataConfig::kMaxSources) {
    nextAttemptMs_[index] = now;
    backoffMs_[index] = kMinBackoffMs;
  }
}

DataQuote DataService::copy(uint8_t index) const {
  DataQuote result;
  if (index >= DataConfig::kMaxSources || !mutex_) return result;
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(2)) == pdTRUE) {
    result = quotes_[index];
    xSemaphoreGive(mutex_);
  }
  return result;
}

DataQuote DataService::quote(uint8_t index) const {
  DataQuote result = copy(index);
  if (!result.valid || index >= DataConfig::kMaxSources) return result;
  const uint32_t staleAfter = refreshIntervalMs(config_.sources[index]) * 3u;
  if (result.ageMs(millis()) > (staleAfter > kStaleFloorMs ? staleAfter : kStaleFloorMs)) {
    result.stale = true;
  }
  return result;
}

uint8_t DataService::sourceCount() const { return config_.count; }

bool DataService::fetching() const { return busy_; }

void DataService::loop(uint32_t nowMs) {
  if (!config_.enabled || config_.count == 0 || busy_) return;
  if (WiFi.status() != WL_CONNECTED) return;

  FetchItem* items = nullptr;
  uint8_t count = 0;
  for (uint8_t i = 0; i < config_.count && i < DataConfig::kMaxSources; ++i) {
    const DataSourceConfig& source = config_.sources[i];
    if (!source.enabled || source.url[0] == '\0' || source.priceField[0] == '\0') continue;
    const bool due = refreshRequested_ ||
                     static_cast<int32_t>(nowMs - nextAttemptMs_[i]) >= 0;
    if (!due) continue;

    if (!items) {
      items = new (std::nothrow) FetchItem[DataConfig::kMaxSources];
      if (!items) {
        LOGE("data", "no memory for fetch items");
        return;
      }
    }
    items[count].index = i;
    items[count].config = source;
    ++count;
  }
  refreshRequested_ = false;

  if (count == 0) {
    delete[] items;
    return;
  }

  FetchJob* job = new (std::nothrow) FetchJob{this, items, count};
  if (!job) {
    delete[] items;
    return;
  }

  busy_ = true;
  if (xTaskCreatePinnedToCore(fetchTask, "data-fetch", 8192, job, 1, &task_, 0) != pdPASS) {
    busy_ = false;
    task_ = nullptr;
    LOGE("data", "failed to start fetch task");
    const uint32_t now = millis();
    for (uint8_t i = 0; i < count; ++i) nextAttemptMs_[items[i].index] = now + kMinBackoffMs;
    delete[] items;
    delete job;
  }
}

void DataService::fetchTask(void* argument) {
  FetchJob* job = static_cast<FetchJob*>(argument);
  if (!job || !job->self) {
    if (job) {
      delete[] job->items;
      delete job;
    }
    vTaskDelete(nullptr);
    return;
  }

  DataService* self = job->self;
  const uint8_t count = job->count;
  self->runFetch(job->items, count);

  const uint32_t now = millis();
  for (uint8_t i = 0; i < count; ++i) {
    const uint8_t index = job->items[i].index;
    const DataQuote quote = self->copy(index);
    if (quote.valid && quote.error[0] == '\0') {
      self->backoffMs_[index] = kMinBackoffMs;
      self->nextAttemptMs_[index] = now + refreshIntervalMs(job->items[i].config);
    } else {
      self->nextAttemptMs_[index] = now + self->backoffMs_[index];
      self->backoffMs_[index] =
          self->backoffMs_[index] * 2u > kMaxBackoffMs ? kMaxBackoffMs : self->backoffMs_[index] * 2u;
    }
  }

  delete[] job->items;
  delete job;
  self->busy_ = false;
  self->task_ = nullptr;
  vTaskDelete(nullptr);
}

void DataService::runFetch(const FetchItem* items, uint8_t count) {
  for (uint8_t i = 0; i < count; ++i) {
    const FetchItem& item = items[i];
    const DataSourceConfig& source = item.config;

    DataQuote next = copy(item.index);
    next.error[0] = '\0';

    String payload;
    char error[40] = {0};
    if (!httpGetJson(source.url, payload, error, sizeof(error))) {
      copyText(next.error, sizeof(next.error), error);
      // Keep the last good numbers: a transient failure should mark the quote
      // stale, not erase it.
      if (next.valid) next.stale = true;
      publish(item.index, next);
      LOGW("data", "%s: %s", source.label[0] ? source.label : "source", error);
      continue;
    }

    JsonDocument document;
    const DeserializationError parseError = deserializeJson(document, payload);
    if (parseError) {
      snprintf(next.error, sizeof(next.error), "json: %s", parseError.c_str());
      if (next.valid) next.stale = true;
      publish(item.index, next);
      LOGE("data", "json parse failed: %s", parseError.c_str());
      continue;
    }

    JsonVariantConst priceValue;
    float price = 0.0f;
    if (!resolveJsonPath(document.as<JsonVariantConst>(), source.priceField, priceValue) ||
        !readNumber(priceValue, &price)) {
      copyText(next.error, sizeof(next.error), "price field not found");
      if (next.valid) next.stale = true;
      publish(item.index, next);
      LOGE("data", "price field '%s' not found", source.priceField);
      continue;
    }

    float change = next.change;
    JsonVariantConst changeValue;
    if (source.changeField[0] &&
        resolveJsonPath(document.as<JsonVariantConst>(), source.changeField, changeValue)) {
      readNumber(changeValue, &change);
    }

    float changePercent = 0.0f;
    JsonVariantConst percentValue;
    bool havePercent = false;
    if (source.changePercentField[0] &&
        resolveJsonPath(document.as<JsonVariantConst>(), source.changePercentField, percentValue)) {
      havePercent = readNumber(percentValue, &changePercent);
    }
    if (!havePercent) {
      const float previous = price - change;
      changePercent = fabsf(previous) > 0.000001f ? change / previous * 100.0f : 0.0f;
    }

    if (source.nameField[0]) {
      JsonVariantConst nameValue;
      if (resolveJsonPath(document.as<JsonVariantConst>(), source.nameField, nameValue)) {
        const char* name = nameValue.as<const char*>();
        if (name && *name) copyText(next.name, sizeof(next.name), name);
        else if (next.name[0] == '\0') copyText(next.name, sizeof(next.name), source.label);
      }
    } else if (source.label[0]) {
      copyText(next.name, sizeof(next.name), source.label);
    }
    if (next.name[0] == '\0') copyText(next.name, sizeof(next.name), "ASSET");

    next.price = price;
    next.change = change;
    next.changePercent = changePercent;
    next.valid = true;
    next.stale = false;
    next.updatedMs = millis();
    next.error[0] = '\0';
    publish(item.index, next);
    LOGI("data", "%s %.4f (%+.2f%%)", next.name, price, changePercent);
  }
}

void DataService::publish(uint8_t index, const DataQuote& quote) {
  if (index >= DataConfig::kMaxSources) return;
  if (!mutex_) {
    quotes_[index] = quote;
    return;
  }
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
    quotes_[index] = quote;
    xSemaphoreGive(mutex_);
  }
}

void DataService::toJson(JsonObject& out, uint32_t nowMs) const {
  out["enabled"] = config_.enabled;
  out["fetching"] = busy_;
  JsonArray sources = out["sources"].to<JsonArray>();
  for (uint8_t i = 0; i < config_.count && i < DataConfig::kMaxSources; ++i) {
    const DataQuote quote = this->quote(i);
    JsonObject item = sources.add<JsonObject>();
    item["index"] = i;
    item["label"] = config_.sources[i].label;
    item["enabled"] = config_.sources[i].enabled;
    item["valid"] = quote.valid;
    item["stale"] = quote.stale;
    item["ageSec"] = quote.valid ? quote.ageMs(nowMs) / 1000u : 0u;
    item["name"] = quote.name;
    item["price"] = quote.price;
    item["change"] = quote.change;
    item["changePercent"] = quote.changePercent;
    item["error"] = quote.error;
  }
}

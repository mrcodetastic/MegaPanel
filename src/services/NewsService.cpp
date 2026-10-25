#include "NewsService.hpp"

#include <WiFi.h>
#include <cstdio>
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
constexpr uint32_t kStaleFloorMs = 15u * 60u * 1000u;

void copyText(char* destination, size_t capacity, const char* source) {
  if (!destination || capacity == 0) return;
  if (!source) {
    destination[0] = '\0';
    return;
  }
  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

// Collapses control characters, newlines and runs of spaces so a JSON title
// fits on a pixel line. UTF-8 bytes above 0x7F are preserved.
void copyHeadline(char* destination, size_t capacity, const char* source) {
  if (!destination || capacity == 0) return;
  destination[0] = '\0';
  if (!source) return;

  size_t written = 0;
  bool pendingSpace = false;
  for (const char* p = source; *p && written + 1 < capacity; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c < 0x20 || c == 0x7F) {
      pendingSpace = written > 0;
      continue;
    }
    if (c == ' ') {
      pendingSpace = written > 0;
      continue;
    }
    if (pendingSpace && written + 2 < capacity) {
      destination[written++] = ' ';
    }
    pendingSpace = false;
    destination[written++] = static_cast<char>(c);
  }
  destination[written] = '\0';
}

uint32_t refreshIntervalMs(const NewsConfig& config) {
  const uint32_t minutes = config.refreshMin >= 1 ? config.refreshMin : 10;
  return minutes * 60u * 1000u;
}

}  // namespace

NewsFeed::NewsFeed()
    : valid(false),
      stale(false),
      updatedMs(0),
      count(0),
      error{0},
      items{} {}

uint32_t NewsFeed::ageMs(uint32_t nowMs) const {
  if (!valid) return 0;
  return nowMs - updatedMs;
}

NewsService::NewsService()
    : mutex_(nullptr),
      feed_(),
      config_(),
      busy_(false),
      refreshRequested_(false),
      nextAttemptMs_(0),
      backoffMs_(kMinBackoffMs),
      task_(nullptr) {}

NewsService::~NewsService() {
  if (mutex_) {
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
  }
}

void NewsService::begin(const NewsConfig& config) {
  mutex_ = xSemaphoreCreateMutex();
  config_ = config;
  nextAttemptMs_ = millis() + 3000;
}

void NewsService::applyConfig(const NewsConfig& config) {
  const bool changed = config.enabled != config_.enabled ||
                       strcmp(config.url, config_.url) != 0 ||
                       strcmp(config.itemsField, config_.itemsField) != 0 ||
                       strcmp(config.titleField, config_.titleField) != 0;
  config_ = config;
  if (!changed) return;
  // Publish through the mutex: the fetch task may be reading the feed.
  NewsFeed cleared;
  publish(cleared);
  nextAttemptMs_ = millis() + 1000;
  backoffMs_ = kMinBackoffMs;
}

void NewsService::requestRefresh() {
  refreshRequested_ = true;
  nextAttemptMs_ = millis();
  backoffMs_ = kMinBackoffMs;
}

NewsFeed NewsService::copy() const {
  NewsFeed result;
  if (!mutex_) return result;
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(2)) == pdTRUE) {
    result = feed_;
    xSemaphoreGive(mutex_);
  }
  return result;
}

NewsFeed NewsService::snapshot() const {
  NewsFeed result = copy();
  if (result.valid && result.ageMs(millis()) > kStaleFloorMs) result.stale = true;
  return result;
}

bool NewsService::fetching() const { return busy_; }

void NewsService::loop(uint32_t nowMs) {
  const bool configured = config_.enabled && config_.url[0] != '\0';
  if (!configured || busy_) return;
  if (WiFi.status() != WL_CONNECTED) {
    NewsFeed current = copy();
    if (current.valid && !current.stale) {
      current.stale = true;
      publish(current);
    }
    return;
  }

  if (refreshRequested_) {
    refreshRequested_ = false;
    nextAttemptMs_ = nowMs;
  }
  if (static_cast<int32_t>(nowMs - nextAttemptMs_) < 0) return;

  FetchJob* job = new (std::nothrow) FetchJob{this, config_};
  if (!job) {
    LOGE("news", "no memory for fetch request");
    nextAttemptMs_ = nowMs + kMinBackoffMs;
    return;
  }

  busy_ = true;
  if (xTaskCreatePinnedToCore(fetchTask, "news-fetch", 8192, job, 1, &task_, 0) != pdPASS) {
    busy_ = false;
    task_ = nullptr;
    delete job;
    LOGE("news", "failed to start fetch task");
    nextAttemptMs_ = nowMs + kMinBackoffMs;
  }
}

void NewsService::fetchTask(void* argument) {
  FetchJob* job = static_cast<FetchJob*>(argument);
  if (!job || !job->self) {
    delete job;
    vTaskDelete(nullptr);
    return;
  }

  NewsService* self = job->self;
  const uint32_t interval = refreshIntervalMs(job->config);
  self->runFetch(job->config);
  delete job;

  const uint32_t now = millis();
  const NewsFeed feed = self->copy();
  if (feed.valid && feed.error[0] == '\0') {
    self->backoffMs_ = kMinBackoffMs;
    self->nextAttemptMs_ = now + interval;
  } else {
    self->nextAttemptMs_ = now + self->backoffMs_;
    self->backoffMs_ =
        self->backoffMs_ * 2u > kMaxBackoffMs ? kMaxBackoffMs : self->backoffMs_ * 2u;
  }

  self->busy_ = false;
  self->task_ = nullptr;
  vTaskDelete(nullptr);
}

void NewsService::runFetch(const NewsConfig& config) {
  String payload;
  char error[40] = {0};
  NewsFeed next;

  if (!httpGetJson(config.url, payload, error, sizeof(error))) {
    next = copy();
    copyText(next.error, sizeof(next.error), error);
    if (next.valid) next.stale = true;
    publish(next);
    LOGW("news", "fetch failed: %s", error);
    return;
  }

  JsonDocument document;
  const DeserializationError parseError = deserializeJson(document, payload);
  if (parseError) {
    next = copy();
    snprintf(next.error, sizeof(next.error), "json: %s", parseError.c_str());
    if (next.valid) next.stale = true;
    publish(next);
    LOGE("news", "json parse failed: %s", parseError.c_str());
    return;
  }

  JsonVariantConst arrayValue;
  bool haveArray = false;
  if (config.itemsField[0]) {
    haveArray = resolveJsonPath(document.as<JsonVariantConst>(), config.itemsField, arrayValue);
  } else {
    arrayValue = document.as<JsonVariantConst>();
    haveArray = true;
  }

  if (!haveArray || !arrayValue.is<JsonArrayConst>()) {
    next = copy();
    copyText(next.error, sizeof(next.error), "items field not found");
    if (next.valid) next.stale = true;
    publish(next);
    LOGE("news", "items field '%s' not found", config.itemsField);
    return;
  }

  const uint8_t limit = config.limit >= 1 ? (config.limit > NewsFeed::kMaxItems ? NewsFeed::kMaxItems
                                                                            : config.limit)
                                          : 6;
  uint8_t count = 0;
  for (JsonVariantConst item : arrayValue.as<JsonArrayConst>()) {
    if (count >= limit) break;

    const char* title = nullptr;
    if (item.is<const char*>()) {
      title = item.as<const char*>();
    } else if (config.titleField[0]) {
      JsonVariantConst titleValue;
      if (resolveJsonPath(item, config.titleField, titleValue)) {
        title = titleValue.as<const char*>();
      }
    }
    if (!title) continue;

    copyHeadline(next.items[count], sizeof(next.items[count]), title);
    if (next.items[count][0] == '\0') continue;
    ++count;
  }

  if (count == 0) {
    next = copy();
    copyText(next.error, sizeof(next.error), "no headlines");
    if (next.valid) next.stale = true;
    publish(next);
    LOGW("news", "feed returned no usable headlines");
    return;
  }

  next.valid = true;
  next.stale = false;
  next.count = count;
  next.updatedMs = millis();
  next.error[0] = '\0';
  publish(next);
  LOGI("news", "%u headlines", static_cast<unsigned>(count));
}

void NewsService::publish(const NewsFeed& feed) {
  if (!mutex_) {
    feed_ = feed;
    return;
  }
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
    feed_ = feed;
    xSemaphoreGive(mutex_);
  }
}

void NewsService::toJson(JsonObject& out, uint32_t nowMs) const {
  const NewsFeed feed = snapshot();
  out["enabled"] = config_.enabled;
  out["configured"] = config_.url[0] != '\0';
  out["fetching"] = busy_;
  out["valid"] = feed.valid;
  out["stale"] = feed.stale;
  out["count"] = feed.count;
  out["ageSec"] = feed.valid ? feed.ageMs(nowMs) / 1000u : 0u;
  out["refreshMin"] = config_.refreshMin;
  out["error"] = feed.error;

  JsonArray items = out["items"].to<JsonArray>();
  if (!feed.valid) return;
  for (uint8_t i = 0; i < feed.count; ++i) {
    items.add(feed.items[i]);
  }
}

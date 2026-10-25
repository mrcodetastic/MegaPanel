#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "app/AppConfig.hpp"

// Headlines fetched from one JSON endpoint. The snapshot is copied, never
// shared, so the render loop reads it without contending with the network.
struct NewsFeed {
  static constexpr uint8_t kMaxItems = 8;

  bool valid;
  bool stale;
  uint32_t updatedMs;
  uint8_t count;
  char error[40];
  char items[kMaxItems][96];

  NewsFeed();

  uint32_t ageMs(uint32_t nowMs) const;
};

// Polls one news endpoint in a worker task. The response may be an array of
// strings or an array of objects with a title field, which covers the shape
// used by the RetroPanel ticker API and most news feeds.
class NewsService {
 public:
  NewsService();
  ~NewsService();

  void begin(const NewsConfig& config);
  void applyConfig(const NewsConfig& config);
  void loop(uint32_t nowMs);
  void requestRefresh();

  NewsFeed snapshot() const;
  bool fetching() const;

  void toJson(JsonObject& out, uint32_t nowMs) const;

 private:
  struct FetchJob {
    class NewsService* self;
    NewsConfig config;
  };

  static void fetchTask(void* argument);
  void runFetch(const NewsConfig& config);
  void publish(const NewsFeed& feed);
  NewsFeed copy() const;

  mutable SemaphoreHandle_t mutex_;
  NewsFeed feed_;
  NewsConfig config_;
  bool busy_;
  bool refreshRequested_;
  uint32_t nextAttemptMs_;
  uint32_t backoffMs_;
  TaskHandle_t task_;
};

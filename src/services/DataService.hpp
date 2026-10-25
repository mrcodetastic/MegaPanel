#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "app/AppConfig.hpp"

enum class QuoteTrend : uint8_t { Flat, Up, Down };

// Immutable snapshot of one configured data source. The render loop copies it;
// the fetch task owns the network.
struct DataQuote {
  bool valid;
  bool stale;
  uint32_t updatedMs;
  float price;
  float change;
  float changePercent;
  char name[20];
  char error[40];

  DataQuote();

  uint32_t ageMs(uint32_t nowMs) const;
  QuoteTrend trend() const;
};

// Fetches up to four arbitrary HTTP/JSON endpoints and maps their fields to a
// price, a change and a percentage. Works for crypto, equities, FX or any other
// asset class: the field names are dot-separated paths into the response.
//
// One worker task performs the requests, so the render loop and the web server
// never wait on DNS or TLS. Results are published under a mutex.
class DataService {
 public:
  DataService();
  ~DataService();

  void begin(const DataConfig& config);
  void applyConfig(const DataConfig& config);
  void loop(uint32_t nowMs);

  // 0xFF refreshes every source.
  void requestRefresh(uint8_t index = 0xFF);

  DataQuote quote(uint8_t index) const;
  uint8_t sourceCount() const;
  bool fetching() const;

  void toJson(JsonObject& out, uint32_t nowMs) const;

 private:
  struct FetchItem {
    uint8_t index;
    DataSourceConfig config;
  };

  struct FetchJob {
    class DataService* self;
    FetchItem* items;
    uint8_t count;
  };

  static void fetchTask(void* argument);
  void runFetch(const FetchItem* items, uint8_t count);
  void publish(uint8_t index, const DataQuote& quote);
  DataQuote copy(uint8_t index) const;

  mutable SemaphoreHandle_t mutex_;
  DataQuote quotes_[DataConfig::kMaxSources];
  DataConfig config_;
  bool busy_;
  bool refreshRequested_;
  uint32_t nextAttemptMs_[DataConfig::kMaxSources];
  uint32_t backoffMs_[DataConfig::kMaxSources];
  TaskHandle_t task_;
};

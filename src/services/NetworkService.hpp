#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "app/AppConfig.hpp"

// WiFi station connection and first-boot provisioning portal.
class NetworkService {
 public:
  NetworkService();

  void begin(const NetworkConfig& config);
  void loop();

  bool connected() const;
  int rssi() const;
  String ip() const;
  const char* hostname() const { return config_.hostname; }
  const NetworkConfig& config() const { return config_; }

 private:
  NetworkConfig config_;
  WiFiManager manager_;
  uint32_t lastRetryMs_;
};

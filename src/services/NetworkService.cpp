#include "NetworkService.hpp"

#include <IPAddress.h>
#include "util/Logger.hpp"

NetworkService::NetworkService() : lastRetryMs_(0) {}

void NetworkService::begin(const NetworkConfig& config) {
  config_ = config;

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(config_.hostname);

  if (config_.staticIp && config_.ip[0]) {
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns1;
    IPAddress dns2;
    if (ip.fromString(config_.ip)) {
      if (config_.gateway[0]) gateway.fromString(config_.gateway);
      if (config_.subnet[0]) subnet.fromString(config_.subnet);
      if (config_.dns1[0]) dns1.fromString(config_.dns1);
      if (config_.dns2[0]) dns2.fromString(config_.dns2);
      WiFi.config(ip, gateway, subnet, dns1, dns2);
      LOGI("net", "static IP %s", config_.ip);
    } else {
      LOGW("net", "invalid static IP '%s'; using DHCP", config_.ip);
    }
  }

  manager_.setConfigPortalTimeout(180);
  manager_.setBreakAfterConfig(true);
  LOGI("net", "connecting WiFi; AP fallback is MegaPanel-Setup");

  if (!manager_.autoConnect("MegaPanel-Setup")) {
    LOGW("net", "WiFi not configured; running offline");
  }

  WiFi.setSleep(false);
  lastRetryMs_ = millis();
}

void NetworkService::loop() {
  if (WiFi.status() == WL_CONNECTED) return;
  const uint32_t now = millis();
  if (now - lastRetryMs_ < 30000) return;
  lastRetryMs_ = now;
  LOGI("net", "reconnecting WiFi");
  WiFi.reconnect();
}

bool NetworkService::connected() const { return WiFi.status() == WL_CONNECTED; }

int NetworkService::rssi() const { return connected() ? WiFi.RSSI() : 0; }

String NetworkService::ip() const { return connected() ? WiFi.localIP().toString() : String(); }

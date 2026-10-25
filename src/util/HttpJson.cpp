#include "HttpJson.hpp"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cstdio>
#include <cstring>

namespace {

constexpr int kHttpTimeoutMs = 6000;
constexpr int kMaxPayloadBytes = 32768;

void copyText(char* destination, size_t capacity, const char* source) {
  if (!destination || capacity == 0) return;
  if (!source) {
    destination[0] = '\0';
    return;
  }
  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

}  // namespace

bool httpGetJson(const char* url, String& payload, char* error, size_t errorLength) {
  if (!url || !*url) {
    copyText(error, errorLength, "no url");
    return false;
  }

  const bool secure = strncmp(url, "https://", 8) == 0;
  HTTPClient http;
  http.setConnectTimeout(kHttpTimeoutMs);
  http.setTimeout(kHttpTimeoutMs);
  http.setReuse(false);

  auto finish = [&](HTTPClient& client) -> bool {
    const int status = client.GET();
    if (status != HTTP_CODE_OK) {
      snprintf(error, errorLength, "http %d", status);
      client.end();
      return false;
    }
    const int size = client.getSize();
    if (size > kMaxPayloadBytes) {
      copyText(error, errorLength, "response too large");
      client.end();
      return false;
    }
    payload = client.getString();
    client.end();
    if (payload.length() == 0) {
      copyText(error, errorLength, "empty response");
      return false;
    }
    return true;
  };

  if (secure) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(kHttpTimeoutMs / 1000);
    if (!http.begin(client, url)) {
      copyText(error, errorLength, "https begin failed");
      return false;
    }
    return finish(http);
  }

  WiFiClient client;
  client.setTimeout(kHttpTimeoutMs / 1000);
  if (!http.begin(client, url)) {
    copyText(error, errorLength, "http begin failed");
    return false;
  }
  return finish(http);
}

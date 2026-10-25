#pragma once

#include <Arduino.h>
#include <stdarg.h>

namespace logger {

inline void logf(char level, const char* tag, const char* format, ...) {
  char buffer[192];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  Serial.printf("[%c][%s] %s\n", level, tag ? tag : "panel", buffer);
}

}  // namespace logger

#define LOGI(tag, ...) logger::logf('I', tag, __VA_ARGS__)
#define LOGW(tag, ...) logger::logf('W', tag, __VA_ARGS__)
#define LOGE(tag, ...) logger::logf('E', tag, __VA_ARGS__)
#define LOGD(tag, ...) logger::logf('D', tag, __VA_ARGS__)

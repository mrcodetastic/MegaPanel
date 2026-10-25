#include "DiagnosticsService.hpp"

#include <esp_heap_caps.h>
#include <esp_system.h>

namespace {

const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt-watchdog";
    case ESP_RST_TASK_WDT: return "task-watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "unknown";
  }
}

}  // namespace

DiagnosticsService::DiagnosticsService()
    : loopStartUs_(0),
      loopMaxUs_(0),
      loopCount_(0),
      frameCount_(0),
      framesThisSecond_(0),
      lastFpsMs_(0),
      fps_(0),
      renderUs_(0),
      composeUs_(0),
      showUs_(0),
      targetFps_(0) {}

void DiagnosticsService::begin() {
  lastFpsMs_ = millis();
  loopStartUs_ = micros();
}

void DiagnosticsService::loopStart() { loopStartUs_ = micros(); }

void DiagnosticsService::loopEnd() {
  const uint32_t elapsed = micros() - loopStartUs_;
  if (elapsed > loopMaxUs_) loopMaxUs_ = elapsed;
  ++loopCount_;
}

void DiagnosticsService::noteFrame(uint32_t renderUs,
                                   uint32_t composeUs,
                                   uint32_t showUs,
                                   uint8_t targetFps) {
  ++frameCount_;
  ++framesThisSecond_;
  renderUs_ = renderUs;
  composeUs_ = composeUs;
  showUs_ = showUs;
  targetFps_ = targetFps;

  const uint32_t now = millis();
  if (now - lastFpsMs_ >= 1000) {
    fps_ = framesThisSecond_;
    framesThisSecond_ = 0;
    lastFpsMs_ = now;
  }
}

void DiagnosticsService::toJson(JsonObject out) const {
  out["uptimeMs"] = millis();
  out["loopCount"] = loopCount_;
  out["frameCount"] = frameCount_;
  out["loopMaxUs"] = loopMaxUs_;
  out["fps"] = fps_;
  out["targetFps"] = targetFps_;
  out["renderUs"] = renderUs_;
  out["composeUs"] = composeUs_;
  out["showUs"] = showUs_;
  out["freeHeap"] = ESP.getFreeHeap();
  out["minFreeHeap"] = ESP.getMinFreeHeap();
  out["freePsram"] = ESP.getFreePsram();
  out["psramSize"] = ESP.getPsramSize();
  out["resetReason"] = resetReasonName(esp_reset_reason());
}

#include "SceneManager.hpp"

#include <cstring>
#include <esp_heap_caps.h>
#include <new>
#include "FrameContext.hpp"
#include "IOutput.hpp"
#include "OutputContext.hpp"
#include "OutputRegistry.hpp"
#include "hal/IMatrixPanel.hpp"
#include "services/ConfigService.hpp"
#include "services/DiagnosticsService.hpp"
#include "services/LightSensorService.hpp"
#include "services/NetworkService.hpp"
#include "services/DataService.hpp"
#include "services/NewsService.hpp"
#include "services/PresenceService.hpp"
#include "services/TimeService.hpp"
#include "services/WeatherService.hpp"
#include "util/Logger.hpp"

namespace {

void copyId(char* destination, size_t capacity, const char* source) {
  if (!destination || capacity == 0) return;
  if (!source) {
    destination[0] = '\0';
    return;
  }
  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

}  // namespace

SceneManager::SceneManager()
    : panel_(nullptr),
      config_(nullptr),
      time_(nullptr),
      network_(nullptr),
      presence_(nullptr),
      light_(nullptr),
      diagnostics_(nullptr),
      weather_(nullptr),
      data_(nullptr),
      news_(nullptr),
      backgroundOutput_(nullptr),
      foregroundOutput_(nullptr),
      statusOutput_(nullptr),
      notifyOutput_(nullptr),
      screen_{0, 0},
      weatherSnapshot_(nullptr),
      foregroundAlphaBase_(255),
      playlistIndex_(0),
      playlistDeadlineMs_(0),
      lastActivityMs_(0),
      transitionStartMs_(0),
      transitionEndMs_(0),
      targetFps_(2),
      lastRenderUs_(0),
      lastComposeUs_(0),
      lastShowUs_(0) {
  backgroundId_[0] = '\0';
  foregroundId_[0] = '\0';
  backgroundStyle_.transparency = LayerTransparency::Opaque;
  foregroundStyle_.transparency = LayerTransparency::BlackIsTransparent;
  systemStyle_.transparency = LayerTransparency::ColorKey;
  systemStyle_.colorKey = CRGB(1, 0, 1);
}

SceneManager::~SceneManager() {
  stopAll();
  delete weatherSnapshot_;
  weatherSnapshot_ = nullptr;
}

bool SceneManager::begin(IMatrixPanel& panel,
                         ConfigService& config,
                         TimeService& time,
                         NetworkService& network,
                         PresenceService& presence,
                         LightSensorService& light,
                         DiagnosticsService& diagnostics,
                         WeatherService& weather,
                         DataService& data,
                         NewsService& news) {
  panel_ = &panel;
  config_ = &config;
  time_ = &time;
  network_ = &network;
  presence_ = &presence;
  light_ = &light;
  diagnostics_ = &diagnostics;
  weather_ = &weather;
  data_ = &data;
  news_ = &news;

  if (!allocateLayers(panel.size())) return false;

  context_.reset(new OutputContext(time, config, network, presence, light, diagnostics));
  context_->weather = &weather;
  context_->data = &data;
  context_->news = &news;
  if (!weatherSnapshot_) weatherSnapshot_ = new (std::nothrow) WeatherData();
  scheduler_.begin(millis(), 60);
  applyConfig(config.get());
  return true;
}

bool SceneManager::allocateLayers(const ScreenSpec& screen) {
  if (!screen.isValid()) {
    LOGE("scene", "invalid screen size");
    return false;
  }

  screen_ = screen;
  backgroundLayer_.reset(new PixelCanvas(screen_.width, screen_.height));
  foregroundLayer_.reset(new PixelCanvas(screen_.width, screen_.height));
  systemLayer_.reset(new PixelCanvas(screen_.width, screen_.height));

  if (!backgroundLayer_->isInitialized() || !foregroundLayer_->isInitialized() ||
      !systemLayer_->isInitialized()) {
    LOGE("scene", "failed to allocate three layers at %ux%u", screen_.width, screen_.height);
    backgroundLayer_.reset();
    foregroundLayer_.reset();
    systemLayer_.reset();
    return false;
  }

  backgroundLayer_->clearForStyle(backgroundStyle_);
  foregroundLayer_->clearForStyle(foregroundStyle_);
  systemLayer_->clearForStyle(systemStyle_);
  LOGI("scene", "layers ready at %ux%u", screen_.width, screen_.height);
  return true;
}

void SceneManager::applyConfig(const AppConfig& config) {
  const SceneConfig& scene = config.scene;

  backgroundStyle_.enabled = scene.backgroundId[0] != '\0';
  backgroundStyle_.alpha = scene.backgroundAlpha;
  backgroundStyle_.transparency = LayerTransparency::Opaque;

  foregroundStyle_.enabled = true;
  foregroundAlphaBase_ = scene.foregroundAlpha;
  foregroundStyle_.alpha = scene.foregroundAlpha;
  foregroundStyle_.transparency = LayerTransparency::BlackIsTransparent;

  systemStyle_.enabled = true;
  systemStyle_.alpha = scene.systemAlpha;
  systemStyle_.transparency = LayerTransparency::ColorKey;
  systemStyle_.colorKey = CRGB(1, 0, 1);

  // While the playlist owns the scene, a configuration save must not yank it
  // back to the configured pair. The first time the playlist starts it is not
  // active yet, so the configured scene is applied normally.
  const bool playlistActive = config.playlist.enabled && config.playlist.count > 0 &&
                              playlistDeadlineMs_ != 0;
  if (!playlistActive) {
    switchBackground(scene.backgroundId);
    switchForeground(scene.foregroundId);
  }
  switchStatusOverlay(scene.statusCorners);
  switchNotificationOverlay(scene.notifications);
  applyPlaylist(config, millis());

  if (backgroundOutput_ && backgroundId_[0]) {
    backgroundOutput_->onConfigChanged(config_->outputConfig(backgroundId_).as<JsonObjectConst>());
  }
  if (foregroundOutput_ && foregroundId_[0]) {
    foregroundOutput_->onConfigChanged(config_->outputConfig(foregroundId_).as<JsonObjectConst>());
  }
}

bool SceneManager::startOutput(IOutput* output, uint8_t layerBit, const char* id) {
  if (!output || !context_) return false;
  if ((output->meta().allowedLayers & layerBit) == 0) {
    LOGE("scene", "%s is not allowed on layer 0x%02x", id, layerBit);
    return false;
  }

  const JsonVariantConst variant = config_->outputConfig(id);
  const JsonObjectConst config = variant.as<JsonObjectConst>();

  if (!output->configure(screen_, config)) {
    LOGE("scene", "%s configure failed", id);
    output->stop();
    return false;
  }
  if (!output->start(*context_)) {
    LOGE("scene", "%s start failed", id);
    output->stop();
    return false;
  }
  return true;
}

bool SceneManager::switchBackground(const char* id) {
  const char* target = id ? id : "";
  if (backgroundOutput_ && strcmp(backgroundId_, target) == 0) return true;

  if (backgroundOutput_) {
    backgroundOutput_->stop();
    backgroundOutput_ = nullptr;
  }
  copyId(backgroundId_, sizeof(backgroundId_), target);

  if (!*target) {
    backgroundLayer_->clear(CRGB::Black);
    return true;
  }

  IOutput* output = OutputRegistry::instance().find(target);
  if (!output || !startOutput(output, LAYER_BG, target)) {
    LOGW("scene", "background %s unavailable; leaving background empty", target);
    backgroundId_[0] = '\0';
    backgroundLayer_->clear(CRGB::Black);
    return false;
  }

  backgroundOutput_ = output;
  LOGI("scene", "background started: %s", target);
  return true;
}

bool SceneManager::switchForeground(const char* id) {
  const char* target = (id && *id) ? id : "clock.standard";
  if (foregroundOutput_ && strcmp(foregroundId_, target) == 0) return true;

  if (foregroundOutput_) {
    foregroundOutput_->stop();
    foregroundOutput_ = nullptr;
  }
  foregroundId_[0] = '\0';

  IOutput* output = OutputRegistry::instance().find(target);
  if (output && startOutput(output, LAYER_FG, target)) {
    foregroundOutput_ = output;
    copyId(foregroundId_, sizeof(foregroundId_), target);
    LOGI("scene", "foreground started: %s", target);
    return true;
  }

  LOGW("scene", "foreground %s failed; falling back to clock.standard", target);
  if (strcmp(target, "clock.standard") != 0) {
    IOutput* fallback = OutputRegistry::instance().find("clock.standard");
    if (fallback && startOutput(fallback, LAYER_FG, "clock.standard")) {
      foregroundOutput_ = fallback;
      copyId(foregroundId_, sizeof(foregroundId_), "clock.standard");
      return true;
    }
  }
  return false;
}

void SceneManager::switchNotificationOverlay(bool enabled) {
  if (enabled) {
    if (notifyOutput_) return;
    IOutput* output = OutputRegistry::instance().find("sys.notify");
    if (output && startOutput(output, LAYER_SYS, "sys.notify")) {
      notifyOutput_ = output;
      LOGI("scene", "notification overlay started");
    } else {
      LOGW("scene", "notification overlay unavailable");
    }
  } else if (notifyOutput_) {
    notifyOutput_->stop();
    notifyOutput_ = nullptr;
  }
}

void SceneManager::switchStatusOverlay(bool enabled) {
  if (enabled) {
    if (statusOutput_) return;
    IOutput* output = OutputRegistry::instance().find("sys.status");
    if (output && startOutput(output, LAYER_SYS, "sys.status")) {
      statusOutput_ = output;
      LOGI("scene", "system status overlay started");
    } else {
      LOGW("scene", "system status overlay unavailable");
    }
  } else if (statusOutput_) {
    statusOutput_->stop();
    statusOutput_ = nullptr;
    systemLayer_->clearForStyle(systemStyle_);
  }
}

uint8_t SceneManager::computeTargetFps() const {
  uint8_t fps = 2;
  if (backgroundOutput_) {
    const uint8_t outputFps = backgroundOutput_->preferredFps();
    if (outputFps > fps) fps = outputFps;
  }
  if (foregroundOutput_) {
    const uint8_t outputFps = foregroundOutput_->preferredFps();
    if (outputFps > fps) fps = outputFps;
  }
  if (statusOutput_) {
    const uint8_t outputFps = statusOutput_->preferredFps();
    if (outputFps > fps) fps = outputFps;
  }
  if (notifyOutput_) {
    const uint8_t outputFps = notifyOutput_->preferredFps();
    if (outputFps > fps) fps = outputFps;
  }
  if (fps > 60) fps = 60;
  return fps;
}

void SceneManager::loop(uint32_t nowMs) {
  if (!panel_ || !panel_->ready()) return;

  if (presence_) {
    const uint32_t motion = presence_->lastMotionMs();
    if (motion > lastActivityMs_) lastActivityMs_ = motion;
  }
  advancePlaylist(nowMs);

  const uint8_t fps = computeTargetFps();
  targetFps_ = fps;
  if (!scheduler_.due(nowMs, fps)) return;
  renderFrame(nowMs, fps);
}

void SceneManager::renderFrame(uint32_t nowMs, uint8_t fps) {
  FrameContext frame;
  frame.nowMs = nowMs;
  frame.deltaMs = scheduler_.deltaMs();
  frame.deltaSec = static_cast<float>(frame.deltaMs) / 1000.0f;
  frame.frameIndex = scheduler_.frameIndex();
  frame.screen = screen_;
  if (time_) {
    frame.localTime = time_->localTime();
    frame.epoch = time_->epoch();
    frame.timeValid = time_->valid();
    frame.ntpSynced = time_->synced();
  }
  if (config_) {
    frame.use24Hour = config_->get().time.use24Hour;
    frame.dateFormat = config_->get().time.dateFormat;
  }
  frame.luxPercent = light_ ? light_->percent() : 100;
  frame.presence = presence_ ? presence_->present() : true;
  if (weather_ && weatherSnapshot_) {
    *weatherSnapshot_ = weather_->snapshot();
    frame.weather = weatherSnapshot_;
  }

  updateTransition(nowMs);

  const uint32_t renderStart = micros();

  backgroundLayer_->clearForStyle(backgroundStyle_);
  foregroundLayer_->clearForStyle(foregroundStyle_);
  systemLayer_->clearForStyle(systemStyle_);

  if (backgroundOutput_) backgroundOutput_->render(*backgroundLayer_, frame);
  if (foregroundOutput_) foregroundOutput_->render(*foregroundLayer_, frame);
  if (statusOutput_) statusOutput_->render(*systemLayer_, frame);
  if (notifyOutput_) notifyOutput_->render(*systemLayer_, frame);

  const uint32_t composeStart = micros();
  compositor_.compose(backgroundLayer_.get(),
                      foregroundLayer_.get(),
                      systemLayer_.get(),
                      backgroundStyle_,
                      foregroundStyle_,
                      systemStyle_,
                      panel_->framebuffer(),
                      screen_);
  const uint32_t showStart = micros();
  panel_->show();
  const uint32_t end = micros();

  lastRenderUs_ = composeStart - renderStart;
  lastComposeUs_ = showStart - composeStart;
  lastShowUs_ = end - showStart;
  if (diagnostics_) {
    diagnostics_->noteFrame(lastRenderUs_, lastComposeUs_, lastShowUs_, fps);
  }
}

void SceneManager::notifyUserActivity() {
  lastActivityMs_ = millis();
  // A manual scene change restarts the carousel from its first entry once the
  // idle period expires.
  playlistIndex_ = 0;
  playlistDeadlineMs_ = 0;
}

void SceneManager::applyPlaylist(const AppConfig& config, uint32_t nowMs) {
  const PlaylistConfig& playlist = config.playlist;
  if (!playlist.enabled || playlist.count == 0) {
    playlistDeadlineMs_ = 0;
    playlistIndex_ = 0;
    return;
  }
  if (playlistIndex_ >= playlist.count) playlistIndex_ = 0;
  if (playlistDeadlineMs_ == 0) {
    const uint16_t dwell = playlist.entries[playlistIndex_].dwellSec;
    playlistDeadlineMs_ =
        nowMs + static_cast<uint32_t>(dwell ? dwell : 20) * 1000u;
  }
}

void SceneManager::advancePlaylist(uint32_t nowMs) {
  if (!config_) return;
  const PlaylistConfig& playlist = config_->get().playlist;
  if (!playlist.enabled || playlist.count == 0) {
    playlistDeadlineMs_ = 0;
    return;
  }

  const uint32_t idleMs = static_cast<uint32_t>(playlist.idleStartSec) * 1000u;
  if (idleMs > 0 && static_cast<uint32_t>(nowMs - lastActivityMs_) < idleMs) {
    return;
  }

  if (playlistDeadlineMs_ == 0) {
    applyPlaylist(config_->get(), nowMs);
    return;
  }
  if (static_cast<int32_t>(nowMs - playlistDeadlineMs_) < 0) return;

  playlistIndex_ = static_cast<uint8_t>((playlistIndex_ + 1) % playlist.count);
  const PlaylistEntry& entry = playlist.entries[playlistIndex_];
  switchBackground(entry.backgroundId);
  switchForeground(entry.foregroundId);

  const uint16_t dwell = entry.dwellSec ? entry.dwellSec : 20;
  playlistDeadlineMs_ = nowMs + static_cast<uint32_t>(dwell) * 1000u;

  if (playlist.transition == 1) {
    transitionStartMs_ = nowMs;
    transitionEndMs_ = nowMs + 450u;
  }
  LOGI("scene", "playlist -> %s + %s", entry.foregroundId,
       entry.backgroundId[0] ? entry.backgroundId : "none");
}

void SceneManager::updateTransition(uint32_t nowMs) {
  if (transitionEndMs_ == 0) return;
  if (static_cast<int32_t>(nowMs - transitionEndMs_) >= 0) {
    foregroundStyle_.alpha = foregroundAlphaBase_;
    transitionEndMs_ = 0;
    return;
  }
  const uint32_t span = transitionEndMs_ > transitionStartMs_ ? transitionEndMs_ - transitionStartMs_ : 1;
  const uint32_t elapsed = nowMs - transitionStartMs_;
  const uint32_t ramp = elapsed * 255u / span;
  foregroundStyle_.alpha =
      static_cast<uint8_t>(static_cast<uint32_t>(foregroundAlphaBase_) * ramp / 255u);
}

void SceneManager::stopAll() {
  if (backgroundOutput_) {
    backgroundOutput_->stop();
    backgroundOutput_ = nullptr;
  }
  if (foregroundOutput_) {
    foregroundOutput_->stop();
    foregroundOutput_ = nullptr;
  }
  if (statusOutput_) {
    statusOutput_->stop();
    statusOutput_ = nullptr;
  }
  if (notifyOutput_) {
    notifyOutput_->stop();
    notifyOutput_ = nullptr;
  }
  backgroundId_[0] = '\0';
  foregroundId_[0] = '\0';
  context_.reset();
}

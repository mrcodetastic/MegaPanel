#pragma once

#include <memory>
#include "FrameScheduler.hpp"
#include "SceneCompositor.hpp"
#include "Types.hpp"
#include "app/AppConfig.hpp"

class IMatrixPanel;
class IOutput;
class OutputContext;
class ConfigService;
class TimeService;
class NetworkService;
class PresenceService;
class LightSensorService;
class DiagnosticsService;
class WeatherService;
class DataService;
class NewsService;
struct WeatherData;

// Owns the three drawable layers and the active output pointers. Outputs are
// pre-instantiated by the registry; this class only configures, starts and
// stops them.
class SceneManager {
 public:
  SceneManager();
  ~SceneManager();

  bool begin(IMatrixPanel& panel,
             ConfigService& config,
             TimeService& time,
             NetworkService& network,
             PresenceService& presence,
             LightSensorService& light,
             DiagnosticsService& diagnostics,
             WeatherService& weather,
             DataService& data,
             NewsService& news);

  void applyConfig(const AppConfig& config);
  void loop(uint32_t nowMs);
  void stopAll();

  // The playlist only advances after this long without a manual scene change
  // or PIR motion.
  void notifyUserActivity();
  uint8_t playlistIndex() const { return playlistIndex_; }

  const char* backgroundId() const { return backgroundId_; }
  const char* foregroundId() const { return foregroundId_; }
  uint8_t targetFps() const { return targetFps_; }
  ScreenSpec screen() const { return screen_; }

  uint32_t lastRenderUs() const { return lastRenderUs_; }
  uint32_t lastComposeUs() const { return lastComposeUs_; }
  uint32_t lastShowUs() const { return lastShowUs_; }

 private:
  bool allocateLayers(const ScreenSpec& screen);
  bool switchBackground(const char* id);
  bool switchForeground(const char* id);
  void switchStatusOverlay(bool enabled);
  void switchNotificationOverlay(bool enabled);
  bool startOutput(IOutput* output, uint8_t layerBit, const char* id);
  uint8_t computeTargetFps() const;
  void renderFrame(uint32_t nowMs, uint8_t fps);
  void applyPlaylist(const AppConfig& config, uint32_t nowMs);
  void advancePlaylist(uint32_t nowMs);
  void updateTransition(uint32_t nowMs);

  IMatrixPanel* panel_;
  ConfigService* config_;
  TimeService* time_;
  NetworkService* network_;
  PresenceService* presence_;
  LightSensorService* light_;
  DiagnosticsService* diagnostics_;
  WeatherService* weather_;
  DataService* data_;
  NewsService* news_;

  std::unique_ptr<OutputContext> context_;
  std::unique_ptr<PixelCanvas> backgroundLayer_;
  std::unique_ptr<PixelCanvas> foregroundLayer_;
  std::unique_ptr<PixelCanvas> systemLayer_;

  LayerStyle backgroundStyle_;
  LayerStyle foregroundStyle_;
  LayerStyle systemStyle_;

  IOutput* backgroundOutput_;
  IOutput* foregroundOutput_;
  IOutput* statusOutput_;
  IOutput* notifyOutput_;

  char backgroundId_[32];
  char foregroundId_[32];

  ScreenSpec screen_;
  FrameScheduler scheduler_;
  SceneCompositor compositor_;
  WeatherData* weatherSnapshot_;
  uint8_t foregroundAlphaBase_;
  uint8_t playlistIndex_;
  uint32_t playlistDeadlineMs_;
  uint32_t lastActivityMs_;
  uint32_t transitionStartMs_;
  uint32_t transitionEndMs_;
  uint8_t targetFps_;
  uint32_t lastRenderUs_;
  uint32_t lastComposeUs_;
  uint32_t lastShowUs_;
};

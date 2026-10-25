#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>

class IMatrixPanel;
class ConfigService;
class SceneManager;
class PresenceService;
class LightSensorService;
class BrightnessService;
class DiagnosticsService;
class NetworkService;
class TimeService;
class WeatherService;
class DataService;
class NewsService;

// Synchronous HTTP server. All routes run in loop(); none of them render.
class WebServerService {
 public:
  WebServerService();

  void begin(IMatrixPanel* panel,
             ConfigService* config,
             SceneManager* scene,
             PresenceService* presence,
             LightSensorService* light,
             BrightnessService* brightness,
             DiagnosticsService* diagnostics,
             NetworkService* network,
             TimeService* time,
             WeatherService* weather,
             DataService* data,
             NewsService* news);

  void loop();

 private:
  void registerRoutes();
  void handleRoot();
  void handleStatus();
  void handleConfigGet();
  void handleConfigPost();
  void handleOutputs();
  void handleSceneGet();
  void handleScenePost();
  void handlePreview();
  void handleDisplayPost();
  void handleNotifyPost();
  void handleSensors();
  void handleWeather();
  void handleWeatherRefresh();
  void handleData();
  void handleDataRefresh();
  void handleNews();
  void handleNewsRefresh();
  void handleReboot();
  void handleNotFound();

  bool serveFile(const String& path);
  String contentTypeFor(const String& path) const;
  String requestBody();
  void sendJson(const JsonDocument& document, int code = 200);
  void sendError(int code, const char* message);

  WebServer server_;
  IMatrixPanel* panel_;
  ConfigService* config_;
  SceneManager* scene_;
  PresenceService* presence_;
  LightSensorService* light_;
  BrightnessService* brightness_;
  DiagnosticsService* diagnostics_;
  NetworkService* network_;
  TimeService* time_;
  WeatherService* weather_;
  DataService* data_;
  NewsService* news_;
};

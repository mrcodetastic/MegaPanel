#pragma once

class TimeService;
class ConfigService;
class NetworkService;
class PresenceService;
class LightSensorService;
class DiagnosticsService;
class WeatherService;
class DataService;
class NewsService;
class StorageService;
class NotificationService;

// Services handed to an output while it is started. Core services are
// references; optional services are nullable until their phases are enabled.
struct OutputContext {
  TimeService& time;
  ConfigService& config;
  NetworkService& network;
  PresenceService& presence;
  LightSensorService& light;
  DiagnosticsService& diagnostics;

  WeatherService* weather;
  DataService* data;
  NewsService* news;
  StorageService* storage;
  NotificationService* notifications;

  OutputContext(TimeService& timeService,
                ConfigService& configService,
                NetworkService& networkService,
                PresenceService& presenceService,
                LightSensorService& lightService,
                DiagnosticsService& diagnosticsService)
      : time(timeService),
        config(configService),
        network(networkService),
        presence(presenceService),
        light(lightService),
        diagnostics(diagnosticsService),
        weather(nullptr),
        data(nullptr),
        news(nullptr),
        storage(nullptr),
        notifications(nullptr) {}
};

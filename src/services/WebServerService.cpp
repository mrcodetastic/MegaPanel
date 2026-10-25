#include "WebServerService.hpp"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <cstdlib>
#include <cstring>

#include "BrightnessService.hpp"
#include "ConfigService.hpp"
#include "DataService.hpp"
#include "DiagnosticsService.hpp"
#include "NewsService.hpp"
#include "LightSensorService.hpp"
#include "NetworkService.hpp"
#include "NotificationService.hpp"
#include "PresenceService.hpp"
#include "TimeService.hpp"
#include "WeatherService.hpp"
#include "core/OutputRegistry.hpp"
#include "core/SceneManager.hpp"
#include "hal/IMatrixPanel.hpp"
#include "util/Logger.hpp"

namespace {

CRGB parseColor(const char* text) {
  if (!text) return CRGB::White;
  if (*text == '#') ++text;
  if (strlen(text) != 6) return CRGB::White;
  char* end = nullptr;
  const uint32_t value = static_cast<uint32_t>(strtoul(text, &end, 16));
  if (!end || *end != '\0') return CRGB::White;
  return CRGB(static_cast<uint8_t>((value >> 16) & 0xFF),
              static_cast<uint8_t>((value >> 8) & 0xFF),
              static_cast<uint8_t>(value & 0xFF));
}

int8_t iconId(const char* name) {
  static const char* const names[] = {"bell", "mail", "alert", "heart", "check",
                                      "cross", "info", "home", "music", "star"};
  if (!name) return -1;
  for (int8_t i = 0; i < 10; ++i) {
    if (strcasecmp(name, names[i]) == 0) return i;
  }
  return -1;
}

const char kFallbackPage[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8"><title>MegaPanel</title>
<style>body{background:#0b0f14;color:#e8eef7;font-family:system-ui,sans-serif;padding:2rem}
code{color:#7ee787}</style></head><body>
<h1>MegaPanel</h1><p>The web UI was not found in LittleFS.</p>
<p>Upload the <code>data/</code> folder with <code>pio run -t uploadfs</code>.</p>
</body></html>
)HTML";

const char kJsonOk[] = "{\"ok\":true}";

}  // namespace

WebServerService::WebServerService()
    : server_(80),
      panel_(nullptr),
      config_(nullptr),
      scene_(nullptr),
      presence_(nullptr),
      light_(nullptr),
      brightness_(nullptr),
      diagnostics_(nullptr),
      network_(nullptr),
      time_(nullptr),
      weather_(nullptr),
      data_(nullptr),
      news_(nullptr) {}

void WebServerService::begin(IMatrixPanel* panel,
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
                             NewsService* news) {
  panel_ = panel;
  config_ = config;
  scene_ = scene;
  presence_ = presence;
  light_ = light;
  brightness_ = brightness;
  diagnostics_ = diagnostics;
  network_ = network;
  time_ = time;
  weather_ = weather;
  data_ = data;
  news_ = news;

  if (!LittleFS.begin(true)) {
    LOGW("web", "LittleFS mount failed; serving embedded fallback page");
  }
  registerRoutes();
  server_.begin();
  LOGI("web", "HTTP server started on port 80");
}

void WebServerService::registerRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/api/config", HTTP_GET, [this]() { handleConfigGet(); });
  server_.on("/api/config", HTTP_POST, [this]() { handleConfigPost(); });
  server_.on("/api/outputs", HTTP_GET, [this]() { handleOutputs(); });
  server_.on("/api/scene", HTTP_GET, [this]() { handleSceneGet(); });
  server_.on("/api/scene", HTTP_POST, [this]() { handleScenePost(); });
  server_.on("/api/preview", HTTP_GET, [this]() { handlePreview(); });
  server_.on("/api/display", HTTP_POST, [this]() { handleDisplayPost(); });
  server_.on("/api/notify", HTTP_POST, [this]() { handleNotifyPost(); });
  server_.on("/api/weather", HTTP_GET, [this]() { handleWeather(); });
  server_.on("/api/weather", HTTP_POST, [this]() { handleWeatherRefresh(); });
  server_.on("/api/data", HTTP_GET, [this]() { handleData(); });
  server_.on("/api/data", HTTP_POST, [this]() { handleDataRefresh(); });
  server_.on("/api/news", HTTP_GET, [this]() { handleNews(); });
  server_.on("/api/news", HTTP_POST, [this]() { handleNewsRefresh(); });
  server_.on("/api/sensors", HTTP_GET, [this]() { handleSensors(); });
  server_.on("/api/reboot", HTTP_POST, [this]() { handleReboot(); });
  server_.onNotFound([this]() { handleNotFound(); });
}

void WebServerService::loop() { server_.handleClient(); }

void WebServerService::handleRoot() {
  if (!serveFile("/www/index.html")) {
    server_.send(200, "text/html", kFallbackPage);
  }
}

void WebServerService::handleStatus() {
  JsonDocument document;
  JsonObject root = document.to<JsonObject>();
  root["ok"] = true;
  root["device"] = config_ ? config_->get().deviceName : "MegaPanel";
  root["version"] = PANEL_FW_VERSION;

  if (panel_) {
    root["panel"]["backend"] = panel_->backendName();
    root["panel"]["width"] = panel_->size().width;
    root["panel"]["height"] = panel_->size().height;
    root["panel"]["ready"] = panel_->ready();
    root["panel"]["brightness"] = panel_->brightness();
  }

  if (network_) {
    root["wifi"]["connected"] = network_->connected();
    root["wifi"]["rssi"] = network_->rssi();
    root["wifi"]["ip"] = network_->ip();
    root["wifi"]["hostname"] = network_->hostname();
  }

  if (scene_) {
    root["scene"]["backgroundId"] = scene_->backgroundId();
    root["scene"]["foregroundId"] = scene_->foregroundId();
    root["scene"]["targetFps"] = scene_->targetFps();
    root["scene"]["renderUs"] = scene_->lastRenderUs();
    root["scene"]["composeUs"] = scene_->lastComposeUs();
    root["scene"]["showUs"] = scene_->lastShowUs();
  }

  if (brightness_) {
    root["brightness"]["target"] = brightness_->target();
    root["brightness"]["reason"] = brightness_->reason();
    root["brightness"]["forcedOff"] = brightness_->forcedOff();
  }

  if (presence_) {
    root["presence"]["enabled"] = presence_->config().enabled;
    root["presence"]["present"] = presence_->present();
    root["presence"]["motion"] = presence_->motion();
    root["presence"]["raw"] = presence_->raw();
    root["presence"]["pin"] = presence_->config().pin;
  }

  if (light_) {
    root["light"]["enabled"] = light_->config().enabled;
    root["light"]["valid"] = light_->valid();
    root["light"]["raw"] = light_->raw();
    root["light"]["percent"] = light_->percent();
    root["light"]["pin"] = light_->config().pin;
  }

  if (time_) {
    root["time"]["valid"] = time_->valid();
    root["time"]["synced"] = time_->synced();
    root["time"]["epoch"] = time_->epoch();
    root["time"]["posix"] = time_->config().posix;
  }

  if (diagnostics_) {
    JsonObject diagnostics = root["diagnostics"].to<JsonObject>();
    diagnostics_->toJson(diagnostics);
  }

  sendJson(document);
}

void WebServerService::handleConfigGet() {
  if (!config_) {
    sendError(503, "configuration unavailable");
    return;
  }
  JsonDocument document;
  JsonObject root = document.to<JsonObject>();
  config_->toJson(root);
  sendJson(document);
}

void WebServerService::handleConfigPost() {
  if (!config_) {
    sendError(503, "configuration unavailable");
    return;
  }

  const String body = requestBody();
  if (body.isEmpty()) {
    sendError(400, "empty request body");
    return;
  }

  JsonDocument document;
  const DeserializationError error = deserializeJson(document, body);
  if (error) {
    sendError(400, error.c_str());
    return;
  }

  if (!config_->fromJson(document.as<JsonObjectConst>())) {
    sendError(400, config_->lastError());
    return;
  }

  config_->requestSave();
  if (weather_) weather_->applyConfig(config_->get().weather);
  if (data_) data_->applyConfig(config_->get().data);
  if (news_) news_->applyConfig(config_->get().news);
  if (scene_) scene_->applyConfig(config_->get());
  if (brightness_) brightness_->setManual(config_->get().brightness.manual);
  server_.send(200, "application/json", kJsonOk);
}

void WebServerService::handleOutputs() {
  JsonDocument document;
  JsonArray array = document.to<JsonArray>();
  const OutputRegistry& registry = OutputRegistry::instance();

  for (size_t i = 0; i < registry.count(); ++i) {
    const OutputDescriptor& descriptor = registry.at(i);
    const OutputMeta& meta = descriptor.meta();
    JsonObject output = array.add<JsonObject>();
    output["id"] = meta.id;
    output["name"] = meta.name;
    output["version"] = meta.version;
    output["author"] = meta.author;
    output["category"] = outputCategoryName(meta.category);
    output["allowedLayers"] = meta.allowedLayers;
    output["defaultLayer"] = static_cast<uint8_t>(meta.defaultLayer);
    output["designWidth"] = meta.designSize.width;
    output["designHeight"] = meta.designSize.height;
    output["minWidth"] = meta.minSize.width;
    output["minHeight"] = meta.minSize.height;
    output["defaultFps"] = meta.defaultFps;
    output["defaultDwellSec"] = meta.defaultDwellSec;
    output["animated"] = meta.animated;
    output["optionalFeature"] = meta.optionalFeature;

    JsonObject schema = output["schema"].to<JsonObject>();
    descriptor.schema(schema);
  }

  sendJson(document);
}

void WebServerService::handleSceneGet() {
  if (!config_) {
    sendError(503, "configuration unavailable");
    return;
  }

  JsonDocument document;
  JsonObject root = document.to<JsonObject>();
  const SceneConfig& scene = config_->get().scene;
  root["backgroundId"] = scene.backgroundId;
  root["foregroundId"] = scene.foregroundId;
  root["backgroundAlpha"] = scene.backgroundAlpha;
  root["foregroundAlpha"] = scene.foregroundAlpha;
  root["systemAlpha"] = scene.systemAlpha;
  root["statusCorners"] = scene.statusCorners;
  root["notifications"] = scene.notifications;

  const PlaylistConfig& playlist = config_->get().playlist;
  JsonObject playlistOut = root["playlist"].to<JsonObject>();
  playlistOut["enabled"] = playlist.enabled;
  playlistOut["idleStartSec"] = playlist.idleStartSec;
  playlistOut["transition"] = playlist.transition;
  playlistOut["count"] = playlist.count;
  JsonArray playlistEntries = playlistOut["entries"].to<JsonArray>();
  for (uint8_t i = 0; i < playlist.count && i < PlaylistConfig::kMaxEntries; ++i) {
    JsonObject entry = playlistEntries.add<JsonObject>();
    entry["foregroundId"] = playlist.entries[i].foregroundId;
    entry["backgroundId"] = playlist.entries[i].backgroundId;
    entry["dwellSec"] = playlist.entries[i].dwellSec;
  }

  if (scene_) {
    root["currentBackgroundId"] = scene_->backgroundId();
    root["currentForegroundId"] = scene_->foregroundId();
    root["targetFps"] = scene_->targetFps();
    playlistOut["currentIndex"] = scene_->playlistIndex();
  }
  sendJson(document);
}

void WebServerService::handleScenePost() {
  if (!config_ || !scene_) {
    sendError(503, "scene unavailable");
    return;
  }

  const String body = requestBody();
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, body);
  if (error) {
    sendError(400, error.c_str());
    return;
  }

  const JsonObjectConst root = document.as<JsonObjectConst>();
  const SceneConfig& current = config_->get().scene;
  const char* backgroundId = root["backgroundId"] | current.backgroundId;
  const char* foregroundId = root["foregroundId"] | current.foregroundId;

  if (!config_->setScene(backgroundId,
                         foregroundId,
                         root["backgroundAlpha"] | current.backgroundAlpha,
                         root["foregroundAlpha"] | current.foregroundAlpha,
                         root["systemAlpha"] | current.systemAlpha,
                         root["statusCorners"] | current.statusCorners,
                         root["notifications"] | current.notifications)) {
    sendError(400, "invalid scene");
    return;
  }

  scene_->notifyUserActivity();
  scene_->applyConfig(config_->get());
  server_.send(200, "application/json", kJsonOk);
}

void WebServerService::handlePreview() {
  if (!panel_ || !panel_->ready()) {
    sendError(503, "panel not ready");
    return;
  }

  const ScreenSpec size = panel_->size();
  const size_t length = static_cast<size_t>(size.width) * size.height * sizeof(CRGB);
  server_.sendHeader("X-Width", String(size.width));
  server_.sendHeader("X-Height", String(size.height));
  server_.sendHeader("X-Frame", String(millis()));
  server_.sendHeader("Cache-Control", "no-store");
  server_.setContentLength(length);
  server_.send(200, "application/octet-stream", "");
  server_.sendContent(reinterpret_cast<const char*>(panel_->framebuffer()), length);
}

void WebServerService::handleDisplayPost() {
  if (!brightness_ || !config_) {
    sendError(503, "display unavailable");
    return;
  }

  const String body = requestBody();
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, body);
  if (error) {
    sendError(400, error.c_str());
    return;
  }

  const JsonObjectConst root = document.as<JsonObjectConst>();
  if (root["on"].is<bool>()) {
    brightness_->setForcedOff(!root["on"].as<bool>());
  }
  if (root["brightness"].is<int>()) {
    brightness_->setManual(static_cast<uint8_t>(root["brightness"].as<int>()));
  }
  server_.send(200, "application/json", kJsonOk);
}

void WebServerService::handleNotifyPost() {
  const String body = requestBody();
  if (body.isEmpty()) {
    sendError(400, "empty request body");
    return;
  }

  JsonDocument document;
  const DeserializationError error = deserializeJson(document, body);
  if (error) {
    sendError(400, error.c_str());
    return;
  }

  const JsonObjectConst root = document.as<JsonObjectConst>();
  const char* text = root["text"] | "Notification";
  const char* color = root["color"] | "#FFFFFF";
  const char* icon = root["icon"] | "";
  const char* position = root["position"] | "bottom";
  const uint32_t duration = root["durationMs"] | 5000;

  NotificationService::instance().show(text, parseColor(color), iconId(icon), duration,
                                       strcmp(position, "top") == 0 ? 1 : 0);
  server_.send(200, "application/json", kJsonOk);
}

void WebServerService::handleWeatherRefresh() {
  if (!weather_) {
    sendError(503, "weather unavailable");
    return;
  }
  weather_->requestRefresh();
  server_.send(200, "application/json", kJsonOk);
}

void WebServerService::handleWeather() {
  JsonDocument document;
  JsonObject root = document.to<JsonObject>();
  if (!weather_) {
    root["available"] = false;
    sendJson(document);
    return;
  }
  root["available"] = true;
  weather_->toJson(root, millis());
  sendJson(document);
}

void WebServerService::handleData() {
  JsonDocument document;
  JsonObject root = document.to<JsonObject>();
  if (!data_) {
    root["available"] = false;
    sendJson(document);
    return;
  }
  root["available"] = true;
  data_->toJson(root, millis());
  sendJson(document);
}

void WebServerService::handleDataRefresh() {
  if (!data_) {
    sendError(503, "data unavailable");
    return;
  }
  data_->requestRefresh();
  server_.send(200, "application/json", kJsonOk);
}

void WebServerService::handleNews() {
  JsonDocument document;
  JsonObject root = document.to<JsonObject>();
  if (!news_) {
    root["available"] = false;
    sendJson(document);
    return;
  }
  root["available"] = true;
  news_->toJson(root, millis());
  sendJson(document);
}

void WebServerService::handleNewsRefresh() {
  if (!news_) {
    sendError(503, "news unavailable");
    return;
  }
  news_->requestRefresh();
  server_.send(200, "application/json", kJsonOk);
}

void WebServerService::handleSensors() {
  JsonDocument document;
  JsonObject root = document.to<JsonObject>();

  if (presence_) {
    JsonObject presence = root["presence"].to<JsonObject>();
    presence["enabled"] = presence_->config().enabled;
    presence["pin"] = presence_->config().pin;
    presence["present"] = presence_->present();
    presence["motion"] = presence_->motion();
    presence["raw"] = presence_->raw();
    presence["lastMotionMs"] = presence_->lastMotionMs();
  }

  if (light_) {
    JsonObject light = root["light"].to<JsonObject>();
    light["enabled"] = light_->config().enabled;
    light["pin"] = light_->config().pin;
    light["valid"] = light_->valid();
    light["raw"] = light_->raw();
    light["percent"] = light_->percent();
    light["rawDark"] = light_->config().rawDark;
    light["rawBright"] = light_->config().rawBright;
  }

  if (brightness_) {
    JsonObject brightness = root["brightness"].to<JsonObject>();
    brightness["target"] = brightness_->target();
    brightness["reason"] = brightness_->reason();
    brightness["forcedOff"] = brightness_->forcedOff();
  }

  sendJson(document);
}

void WebServerService::handleReboot() {
  server_.send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
  delay(150);
  ESP.restart();
}

void WebServerService::handleNotFound() {
  if (server_.uri().startsWith("/api/")) {
    sendError(404, "not found");
    return;
  }

  String path = server_.uri();
  if (path.endsWith("/")) path += "index.html";
  if (!serveFile("/www" + path)) {
    sendError(404, "not found");
  }
}

bool WebServerService::serveFile(const String& path) {
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  server_.streamFile(file, contentTypeFor(path));
  file.close();
  return true;
}

String WebServerService::contentTypeFor(const String& path) const {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".ico")) return "image/x-icon";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".gif")) return "image/gif";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  return "text/plain";
}

String WebServerService::requestBody() {
  String body = server_.arg("plain");
  if (body.isEmpty() && server_.hasArg("config")) body = server_.arg("config");
  return body;
}

void WebServerService::sendJson(const JsonDocument& document, int code) {
  String output;
  output.reserve(measureJson(document) + 1);
  serializeJson(document, output);
  server_.send(code, "application/json", output);
}

void WebServerService::sendError(int code, const char* message) {
  JsonDocument document;
  JsonObject root = document.to<JsonObject>();
  root["ok"] = false;
  root["error"] = message ? message : "error";
  sendJson(document, code);
}

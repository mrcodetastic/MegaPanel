#include "ConfigService.hpp"

#include <cstring>
#include <strings.h>
#include "util/Logger.hpp"

namespace {

constexpr const char* kNamespace = "panel";
constexpr const char* kConfigKey = "cfg";
constexpr uint32_t kSaveDelayMs = 1500;

void copyText(char* destination, size_t capacity, const char* source) {
  if (!destination || capacity == 0) return;
  if (!source) {
    destination[0] = '\0';
    return;
  }
  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

uint16_t clampWord(int value, uint16_t low, uint16_t high) {
  if (value < static_cast<int>(low)) return low;
  if (value > static_cast<int>(high)) return high;
  return static_cast<uint16_t>(value);
}

uint8_t clampByte(int value, uint8_t low, uint8_t high) {
  if (value < low) return low;
  if (value > high) return high;
  return static_cast<uint8_t>(value);
}

uint8_t driverFromName(const char* name) {
  if (!name) return 0;
  if (strcasecmp(name, "ICN2038S") == 0) return 1;
  return 0;
}

const char* driverName(uint8_t driver) {
  return driver == 1 ? "ICN2038S" : "FM6126A";
}

}  // namespace

ConfigService::ConfigService()
    : dirty_(false), saveAtMs_(0), lastError_{0} {
  outputsDocument_.clear();
}

bool ConfigService::begin() {
  if (!preferences_.begin(kNamespace, false)) {
    setError("failed to open NVS namespace");
    LOGW("config", "NVS namespace unavailable; using defaults");
    return false;
  }
  return load();
}

void ConfigService::loop(uint32_t nowMs) {
  if (dirty_ && static_cast<int32_t>(nowMs - saveAtMs_) >= 0) {
    saveNow();
  }
}

void ConfigService::requestSave() {
  dirty_ = true;
  saveAtMs_ = millis() + kSaveDelayMs;
}

void ConfigService::saveNow() {
  JsonDocument document;
  JsonObject root = document.to<JsonObject>();
  toJson(root);

  String serialized;
  serialized.reserve(2048);
  serializeJson(document, serialized);

  if (serialized.length() > 3500) {
    LOGW("config", "configuration is %u bytes; NVS values should stay below 4000",
         static_cast<unsigned>(serialized.length()));
  }

  const size_t written = preferences_.putString(kConfigKey, serialized);
  if (written == 0) {
    setError("failed to write configuration");
    LOGE("config", "failed to write configuration (%u bytes)",
         static_cast<unsigned>(serialized.length()));
    return;
  }

  dirty_ = false;
  lastError_[0] = '\0';
  LOGI("config", "saved %u bytes", static_cast<unsigned>(written));
}

void ConfigService::reset() {
  config_ = AppConfig();
  outputsDocument_.clear();
  requestSave();
  LOGI("config", "reset to defaults");
}

bool ConfigService::load() {
  const String serialized = preferences_.getString(kConfigKey, "");
  if (serialized.isEmpty()) {
    LOGI("config", "no saved configuration; using defaults");
    return true;
  }

  JsonDocument document;
  const DeserializationError error = deserializeJson(document, serialized);
  if (error) {
    setError(error.c_str());
    LOGW("config", "configuration parse failed: %s", error.c_str());
    return false;
  }

  const bool applied = fromJson(document.as<JsonObjectConst>());
  LOGI("config", "loaded %u bytes", static_cast<unsigned>(serialized.length()));
  return applied;
}

void ConfigService::toJson(JsonObject root) const {
  root["version"] = config_.version;
  root["device"]["name"] = config_.deviceName;

  JsonObject display = root["display"].to<JsonObject>();
  display["logicalWidth"] = config_.display.logicalWidth;
  display["logicalHeight"] = config_.display.logicalHeight;
  display["panelWidth"] = config_.display.panelWidth;
  display["panelHeight"] = config_.display.panelHeight;
  display["rows"] = config_.display.rows;
  display["cols"] = config_.display.cols;
  display["chainType"] = config_.display.chainType;
  display["rotation"] = config_.display.rotation;
  display["brightness"] = config_.display.brightness;
  display["driver"] = driverName(config_.display.driver);
  display["clkPhase"] = config_.display.clkPhase;
  display["doubleBuffer"] = config_.display.doubleBuffer;
  display["swapBlueGreen"] = config_.display.swapBlueGreen;
  display["gamma"] = config_.display.gamma;

  JsonObject time = root["time"].to<JsonObject>();
  time["posix"] = config_.time.posix;
  time["ntp1"] = config_.time.ntp1;
  time["ntp2"] = config_.time.ntp2;
  time["use24Hour"] = config_.time.use24Hour;
  time["dateFormat"] = config_.time.dateFormat;

  JsonObject brightness = root["brightness"].to<JsonObject>();
  brightness["manual"] = config_.brightness.manual;
  brightness["schedule"]["dimEnabled"] = config_.brightness.dimEnabled;
  brightness["schedule"]["dimStartMin"] = config_.brightness.dimStartMin;
  brightness["schedule"]["dimEndMin"] = config_.brightness.dimEndMin;
  brightness["schedule"]["dimLevel"] = config_.brightness.dimLevel;
  brightness["schedule"]["offEnabled"] = config_.brightness.offEnabled;
  brightness["schedule"]["offStartMin"] = config_.brightness.offStartMin;
  brightness["schedule"]["offEndMin"] = config_.brightness.offEndMin;

  JsonObject presence = root["presence"].to<JsonObject>();
  presence["enabled"] = config_.presence.enabled;
  presence["pin"] = config_.presence.pin;
  presence["activeHigh"] = config_.presence.activeHigh;
  presence["debounceMs"] = config_.presence.debounceMs;
  presence["timeoutSec"] = config_.presence.timeoutSec;
  presence["absentAction"] = config_.presence.absentAction;
  presence["absentDim"] = config_.presence.absentDim;

  JsonObject light = root["light"].to<JsonObject>();
  light["enabled"] = config_.light.enabled;
  light["pin"] = config_.light.pin;
  light["sampleIntervalMs"] = config_.light.sampleIntervalMs;
  light["rawDark"] = config_.light.rawDark;
  light["rawBright"] = config_.light.rawBright;
  light["invert"] = config_.light.invert;
  light["minBrightness"] = config_.light.minBrightness;
  light["maxBrightness"] = config_.light.maxBrightness;
  light["smoothing"] = config_.light.smoothing;

  JsonObject scene = root["scene"].to<JsonObject>();
  scene["backgroundId"] = config_.scene.backgroundId;
  scene["foregroundId"] = config_.scene.foregroundId;
  scene["backgroundAlpha"] = config_.scene.backgroundAlpha;
  scene["foregroundAlpha"] = config_.scene.foregroundAlpha;
  scene["systemAlpha"] = config_.scene.systemAlpha;
  scene["statusCorners"] = config_.scene.statusCorners;
  scene["notifications"] = config_.scene.notifications;

  JsonObject weather = root["weather"].to<JsonObject>();
  weather["enabled"] = config_.weather.enabled;
  weather["latitude"] = config_.weather.latitude;
  weather["longitude"] = config_.weather.longitude;
  weather["fahrenheit"] = config_.weather.fahrenheit;
  weather["refreshMin"] = config_.weather.refreshMin;

  JsonObject playlist = root["playlist"].to<JsonObject>();
  playlist["enabled"] = config_.playlist.enabled;
  playlist["idleStartSec"] = config_.playlist.idleStartSec;
  playlist["transition"] = config_.playlist.transition;
  JsonArray entries = playlist["entries"].to<JsonArray>();
  for (uint8_t i = 0; i < config_.playlist.count && i < PlaylistConfig::kMaxEntries; ++i) {
    JsonObject entry = entries.add<JsonObject>();
    entry["foregroundId"] = config_.playlist.entries[i].foregroundId;
    entry["backgroundId"] = config_.playlist.entries[i].backgroundId;
    entry["dwellSec"] = config_.playlist.entries[i].dwellSec;
  }

  JsonObject data = root["data"].to<JsonObject>();
  data["enabled"] = config_.data.enabled;
  JsonArray sources = data["sources"].to<JsonArray>();
  for (uint8_t i = 0; i < config_.data.count && i < DataConfig::kMaxSources; ++i) {
    const DataSourceConfig& source = config_.data.sources[i];
    JsonObject item = sources.add<JsonObject>();
    item["enabled"] = source.enabled;
    item["label"] = source.label;
    item["url"] = source.url;
    item["nameField"] = source.nameField;
    item["priceField"] = source.priceField;
    item["changeField"] = source.changeField;
    item["changePercentField"] = source.changePercentField;
    item["refreshSec"] = source.refreshSec;
  }

  JsonObject news = root["news"].to<JsonObject>();
  news["enabled"] = config_.news.enabled;
  news["url"] = config_.news.url;
  news["itemsField"] = config_.news.itemsField;
  news["titleField"] = config_.news.titleField;
  news["limit"] = config_.news.limit;
  news["refreshMin"] = config_.news.refreshMin;

  JsonObject network = root["network"].to<JsonObject>();
  network["hostname"] = config_.network.hostname;
  network["staticIp"] = config_.network.staticIp;
  network["ip"] = config_.network.ip;
  network["gateway"] = config_.network.gateway;
  network["subnet"] = config_.network.subnet;
  network["dns1"] = config_.network.dns1;
  network["dns2"] = config_.network.dns2;

  JsonObject outputs = root["outputs"].to<JsonObject>();
  if (!outputsDocument_.isNull()) {
    outputs.set(outputsDocument_.as<JsonObjectConst>());
  }
}

bool ConfigService::fromJson(JsonObjectConst root) {
  lastError_[0] = '\0';

  config_.version = root["version"] | config_.version;
  copyText(config_.deviceName, sizeof(config_.deviceName),
           root["device"]["name"] | config_.deviceName);

  JsonObjectConst display = root["display"];
  if (!display.isNull()) {
    config_.display.logicalWidth = display["logicalWidth"] | config_.display.logicalWidth;
    config_.display.logicalHeight = display["logicalHeight"] | config_.display.logicalHeight;
    config_.display.panelWidth = display["panelWidth"] | config_.display.panelWidth;
    config_.display.panelHeight = display["panelHeight"] | config_.display.panelHeight;
    config_.display.rows = display["rows"] | config_.display.rows;
    config_.display.cols = display["cols"] | config_.display.cols;
    config_.display.chainType = display["chainType"] | config_.display.chainType;
    config_.display.rotation = display["rotation"] | config_.display.rotation;
    config_.display.brightness = clampByte(display["brightness"] | config_.display.brightness, 1, 255);
    config_.display.driver = driverFromName(display["driver"] | "FM6126A");
    config_.display.clkPhase = display["clkPhase"] | config_.display.clkPhase;
    config_.display.doubleBuffer = display["doubleBuffer"] | config_.display.doubleBuffer;
    config_.display.swapBlueGreen = display["swapBlueGreen"] | config_.display.swapBlueGreen;
    config_.display.gamma = display["gamma"] | config_.display.gamma;
  }

  JsonObjectConst time = root["time"];
  if (!time.isNull()) {
    copyText(config_.time.posix, sizeof(config_.time.posix), time["posix"] | config_.time.posix);
    copyText(config_.time.ntp1, sizeof(config_.time.ntp1), time["ntp1"] | config_.time.ntp1);
    copyText(config_.time.ntp2, sizeof(config_.time.ntp2), time["ntp2"] | config_.time.ntp2);
    config_.time.use24Hour = time["use24Hour"] | config_.time.use24Hour;
    config_.time.dateFormat = time["dateFormat"] | config_.time.dateFormat;
  }

  JsonObjectConst brightness = root["brightness"];
  if (!brightness.isNull()) {
    config_.brightness.manual = clampByte(brightness["manual"] | config_.brightness.manual, 1, 255);
    JsonObjectConst schedule = brightness["schedule"];
    if (!schedule.isNull()) {
      config_.brightness.dimEnabled = schedule["dimEnabled"] | config_.brightness.dimEnabled;
      config_.brightness.dimStartMin = schedule["dimStartMin"] | config_.brightness.dimStartMin;
      config_.brightness.dimEndMin = schedule["dimEndMin"] | config_.brightness.dimEndMin;
      config_.brightness.dimLevel = clampByte(schedule["dimLevel"] | config_.brightness.dimLevel, 1, 255);
      config_.brightness.offEnabled = schedule["offEnabled"] | config_.brightness.offEnabled;
      config_.brightness.offStartMin = schedule["offStartMin"] | config_.brightness.offStartMin;
      config_.brightness.offEndMin = schedule["offEndMin"] | config_.brightness.offEndMin;
    }
  }

  JsonObjectConst presence = root["presence"];
  if (!presence.isNull()) {
    config_.presence.enabled = presence["enabled"] | config_.presence.enabled;
    config_.presence.pin = presence["pin"] | config_.presence.pin;
    config_.presence.activeHigh = presence["activeHigh"] | config_.presence.activeHigh;
    config_.presence.debounceMs = presence["debounceMs"] | config_.presence.debounceMs;
    config_.presence.timeoutSec = presence["timeoutSec"] | config_.presence.timeoutSec;
    config_.presence.absentAction = presence["absentAction"] | config_.presence.absentAction;
    config_.presence.absentDim = presence["absentDim"] | config_.presence.absentDim;
  }

  JsonObjectConst light = root["light"];
  if (!light.isNull()) {
    config_.light.enabled = light["enabled"] | config_.light.enabled;
    config_.light.pin = light["pin"] | config_.light.pin;
    config_.light.sampleIntervalMs = light["sampleIntervalMs"] | config_.light.sampleIntervalMs;
    config_.light.rawDark = light["rawDark"] | config_.light.rawDark;
    config_.light.rawBright = light["rawBright"] | config_.light.rawBright;
    config_.light.invert = light["invert"] | config_.light.invert;
    config_.light.minBrightness = clampByte(light["minBrightness"] | config_.light.minBrightness, 1, 255);
    config_.light.maxBrightness = clampByte(light["maxBrightness"] | config_.light.maxBrightness, 1, 255);
    config_.light.smoothing = light["smoothing"] | config_.light.smoothing;
  }

  JsonObjectConst scene = root["scene"];
  if (!scene.isNull()) {
    copyText(config_.scene.backgroundId, sizeof(config_.scene.backgroundId),
             scene["backgroundId"] | config_.scene.backgroundId);
    copyText(config_.scene.foregroundId, sizeof(config_.scene.foregroundId),
             scene["foregroundId"] | config_.scene.foregroundId);
    config_.scene.backgroundAlpha = clampByte(scene["backgroundAlpha"] | config_.scene.backgroundAlpha, 0, 255);
    config_.scene.foregroundAlpha = clampByte(scene["foregroundAlpha"] | config_.scene.foregroundAlpha, 0, 255);
    config_.scene.systemAlpha = clampByte(scene["systemAlpha"] | config_.scene.systemAlpha, 0, 255);
    config_.scene.statusCorners = scene["statusCorners"] | config_.scene.statusCorners;
    config_.scene.notifications = scene["notifications"] | config_.scene.notifications;
  }

  JsonObjectConst weather = root["weather"];
  if (!weather.isNull()) {
    config_.weather.enabled = weather["enabled"] | config_.weather.enabled;
    config_.weather.latitude = weather["latitude"] | config_.weather.latitude;
    config_.weather.longitude = weather["longitude"] | config_.weather.longitude;
    config_.weather.fahrenheit = weather["fahrenheit"] | config_.weather.fahrenheit;
    config_.weather.refreshMin =
        clampWord(weather["refreshMin"] | config_.weather.refreshMin, 1, 240);
  }

  JsonObjectConst playlist = root["playlist"];
  if (!playlist.isNull()) {
    config_.playlist.enabled = playlist["enabled"] | config_.playlist.enabled;
    config_.playlist.idleStartSec =
        clampWord(playlist["idleStartSec"] | config_.playlist.idleStartSec, 0, 3600);
    config_.playlist.transition =
        clampByte(playlist["transition"] | config_.playlist.transition, 0, 1);

    uint8_t count = 0;
    JsonArrayConst entries = playlist["entries"];
    if (!entries.isNull()) {
      for (JsonObjectConst entry : entries) {
        if (count >= PlaylistConfig::kMaxEntries) break;
        PlaylistEntry& target = config_.playlist.entries[count];
        copyText(target.foregroundId, sizeof(target.foregroundId), entry["foregroundId"] | "");
        copyText(target.backgroundId, sizeof(target.backgroundId), entry["backgroundId"] | "");
        target.dwellSec = clampWord(entry["dwellSec"] | 20, 3, 3600);
        ++count;
      }
    }
    config_.playlist.count = count;
  }

  JsonObjectConst data = root["data"];
  if (!data.isNull()) {
    config_.data.enabled = data["enabled"] | config_.data.enabled;

    uint8_t sourceCount = 0;
    JsonArrayConst sources = data["sources"];
    if (!sources.isNull()) {
      for (JsonObjectConst item : sources) {
        if (sourceCount >= DataConfig::kMaxSources) break;
        DataSourceConfig& source = config_.data.sources[sourceCount];
        source.enabled = item["enabled"] | true;
        copyText(source.label, sizeof(source.label), item["label"] | "");
        copyText(source.url, sizeof(source.url), item["url"] | "");
        copyText(source.nameField, sizeof(source.nameField), item["nameField"] | "");
        copyText(source.priceField, sizeof(source.priceField), item["priceField"] | "");
        copyText(source.changeField, sizeof(source.changeField), item["changeField"] | "");
        copyText(source.changePercentField, sizeof(source.changePercentField),
                 item["changePercentField"] | "");
        source.refreshSec = clampWord(item["refreshSec"] | 120, 10, 3600);
        ++sourceCount;
      }
    }
    config_.data.count = sourceCount;
  }

  JsonObjectConst news = root["news"];
  if (!news.isNull()) {
    config_.news.enabled = news["enabled"] | config_.news.enabled;
    copyText(config_.news.url, sizeof(config_.news.url), news["url"] | config_.news.url);
    copyText(config_.news.itemsField, sizeof(config_.news.itemsField),
             news["itemsField"] | config_.news.itemsField);
    copyText(config_.news.titleField, sizeof(config_.news.titleField),
             news["titleField"] | config_.news.titleField);
    config_.news.limit = clampByte(news["limit"] | config_.news.limit, 1, 8);
    config_.news.refreshMin = clampWord(news["refreshMin"] | config_.news.refreshMin, 1, 240);
  }

  JsonObjectConst network = root["network"];
  if (!network.isNull()) {
    copyText(config_.network.hostname, sizeof(config_.network.hostname),
             network["hostname"] | config_.network.hostname);
    config_.network.staticIp = network["staticIp"] | config_.network.staticIp;
    copyText(config_.network.ip, sizeof(config_.network.ip), network["ip"] | config_.network.ip);
    copyText(config_.network.gateway, sizeof(config_.network.gateway),
             network["gateway"] | config_.network.gateway);
    copyText(config_.network.subnet, sizeof(config_.network.subnet),
             network["subnet"] | config_.network.subnet);
    copyText(config_.network.dns1, sizeof(config_.network.dns1), network["dns1"] | config_.network.dns1);
    copyText(config_.network.dns2, sizeof(config_.network.dns2), network["dns2"] | config_.network.dns2);
  }

  if (root["outputs"].is<JsonObjectConst>()) {
    outputsDocument_.clear();
    outputsDocument_.set(root["outputs"]);
  }

  return true;
}

JsonVariantConst ConfigService::outputConfig(const char* id) const {
  if (!id || !*id) return JsonVariantConst();
  return outputsDocument_[id];
}

JsonObjectConst ConfigService::outputs() const {
  return outputsDocument_.as<JsonObjectConst>();
}

bool ConfigService::setScene(const char* backgroundId,
                             const char* foregroundId,
                             uint8_t backgroundAlpha,
                             uint8_t foregroundAlpha,
                             uint8_t systemAlpha,
                             bool statusCorners,
                             bool notifications) {
  copyText(config_.scene.backgroundId, sizeof(config_.scene.backgroundId), backgroundId);
  copyText(config_.scene.foregroundId, sizeof(config_.scene.foregroundId), foregroundId);
  config_.scene.backgroundAlpha = backgroundAlpha;
  config_.scene.foregroundAlpha = foregroundAlpha;
  config_.scene.systemAlpha = systemAlpha;
  config_.scene.statusCorners = statusCorners;
  config_.scene.notifications = notifications;
  requestSave();
  return true;
}

void ConfigService::setError(const char* message) {
  copyText(lastError_, sizeof(lastError_), message);
}

#pragma once

#include <stdint.h>

// Persisted application configuration. Core settings are typed; per-output
// settings live in a JSON document owned by ConfigService.
struct DisplayConfig {
  uint16_t logicalWidth;
  uint16_t logicalHeight;
  uint16_t panelWidth;
  uint16_t panelHeight;
  uint8_t rows;
  uint8_t cols;
  uint8_t chainType;
  uint8_t rotation;
  uint8_t brightness;
  uint8_t driver;  // 0 = FM6126A, 1 = ICN2038S
  bool clkPhase;
  bool doubleBuffer;
  bool swapBlueGreen;
  float gamma;

  DisplayConfig();
};

struct TimeConfig {
  char posix[64];
  char ntp1[64];
  char ntp2[64];
  bool use24Hour;
  uint8_t dateFormat;

  TimeConfig();
};

struct BrightnessConfig {
  uint8_t manual;
  bool dimEnabled;
  uint16_t dimStartMin;
  uint16_t dimEndMin;
  uint8_t dimLevel;
  bool offEnabled;
  uint16_t offStartMin;
  uint16_t offEndMin;

  BrightnessConfig();
};

struct PresenceConfig {
  bool enabled;
  int8_t pin;
  bool activeHigh;
  uint16_t debounceMs;
  uint16_t timeoutSec;
  uint8_t absentAction;  // 0 = blank, 1 = dim, 2 = ambient background
  uint8_t absentDim;

  PresenceConfig();
};

struct LightConfig {
  bool enabled;
  int8_t pin;
  uint16_t sampleIntervalMs;
  uint16_t rawDark;
  uint16_t rawBright;
  bool invert;
  uint8_t minBrightness;
  uint8_t maxBrightness;
  uint8_t smoothing;

  LightConfig();
};

struct SceneConfig {
  char backgroundId[32];
  char foregroundId[32];
  uint8_t backgroundAlpha;
  uint8_t foregroundAlpha;
  uint8_t systemAlpha;
  bool statusCorners;
  bool notifications;

  SceneConfig();
};

struct WeatherConfig {
  bool enabled;
  float latitude;
  float longitude;
  bool fahrenheit;
  uint16_t refreshMin;

  WeatherConfig();
};

// Playlist/carousel. Entries are fixed-size so the whole configuration stays a
// value type with no runtime allocation.
struct PlaylistEntry {
  char foregroundId[32];
  char backgroundId[32];
  uint16_t dwellSec;

  PlaylistEntry();
};

struct PlaylistConfig {
  bool enabled;
  uint8_t count;
  uint16_t idleStartSec;
  uint8_t transition;  // 0 = cut, 1 = fade
  static constexpr uint8_t kMaxEntries = 8;
  PlaylistEntry entries[kMaxEntries];

  PlaylistConfig();
};

struct NetworkConfig {
  char hostname[33];
  bool staticIp;
  char ip[16];
  char gateway[16];
  char subnet[16];
  char dns1[16];
  char dns2[16];

  NetworkConfig();
};

// One HTTP/JSON data source. Field names are dot-separated paths into the
// response, e.g. "data.0.price" or "bitcoin.usd". Empty means "not supplied".
struct DataSourceConfig {
  bool enabled;
  char label[20];
  char url[192];
  char nameField[40];
  char priceField[40];
  char changeField[40];
  char changePercentField[40];
  uint16_t refreshSec;

  DataSourceConfig();
};

struct DataConfig {
  bool enabled;
  uint8_t count;
  static constexpr uint8_t kMaxSources = 4;
  DataSourceConfig sources[kMaxSources];

  DataConfig();
};

// One news feed. `itemsField` is the path to the array of items (empty means
// the document root is the array); `titleField` is the path inside each item
// (empty means the item itself is a string).
struct NewsConfig {
  bool enabled;
  char url[192];
  char itemsField[40];
  char titleField[24];
  uint8_t limit;
  uint16_t refreshMin;

  NewsConfig();
};

struct AppConfig {
  uint16_t version;
  char deviceName[33];
  DisplayConfig display;
  TimeConfig time;
  BrightnessConfig brightness;
  PresenceConfig presence;
  LightConfig light;
  SceneConfig scene;
  WeatherConfig weather;
  PlaylistConfig playlist;
  DataConfig data;
  NewsConfig news;
  NetworkConfig network;

  AppConfig();
};

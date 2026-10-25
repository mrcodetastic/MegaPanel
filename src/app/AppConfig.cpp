#include "AppConfig.hpp"

#include <string.h>

namespace {

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

DisplayConfig::DisplayConfig()
    : logicalWidth(64),
      logicalHeight(64),
      panelWidth(64),
      panelHeight(64),
      rows(1),
      cols(1),
      chainType(0),
      rotation(0),
      brightness(128),
      driver(0),
      clkPhase(false),
      doubleBuffer(true),
      swapBlueGreen(false),
      gamma(2.2f) {}

TimeConfig::TimeConfig()
    : posix("GMT0BST,M3.5.0/01:00,M10.5.0/02:00"),
      ntp1("pool.ntp.org"),
      ntp2("time.nist.gov"),
      use24Hour(true),
      dateFormat(0) {}

BrightnessConfig::BrightnessConfig()
    : manual(128),
      dimEnabled(false),
      dimStartMin(22 * 60),
      dimEndMin(7 * 60),
      dimLevel(32),
      offEnabled(false),
      offStartMin(0),
      offEndMin(7 * 60) {}

PresenceConfig::PresenceConfig()
    : enabled(false),
      pin(-1),
      activeHigh(true),
      debounceMs(50),
      timeoutSec(300),
      absentAction(0),
      absentDim(8) {}

LightConfig::LightConfig()
    : enabled(false),
      pin(-1),
      sampleIntervalMs(500),
      rawDark(0),
      rawBright(4095),
      invert(false),
      minBrightness(8),
      maxBrightness(200),
      smoothing(80) {}

SceneConfig::SceneConfig()
    : backgroundId(""),
      foregroundId("clock.standard"),
      backgroundAlpha(255),
      foregroundAlpha(255),
      systemAlpha(255),
      statusCorners(true),
      notifications(true) {}

WeatherConfig::WeatherConfig()
    : enabled(false), latitude(0.0f), longitude(0.0f), fahrenheit(false), refreshMin(15) {}

PlaylistEntry::PlaylistEntry() : dwellSec(20) {
  foregroundId[0] = '\0';
  backgroundId[0] = '\0';
}

PlaylistConfig::PlaylistConfig() : enabled(false), count(0), idleStartSec(30), transition(1) {}

DataSourceConfig::DataSourceConfig() : enabled(true), refreshSec(120) {
  label[0] = '\0';
  url[0] = '\0';
  nameField[0] = '\0';
  priceField[0] = '\0';
  changeField[0] = '\0';
  changePercentField[0] = '\0';
}

DataConfig::DataConfig() : enabled(false), count(0) {}

NewsConfig::NewsConfig() : enabled(false), limit(6), refreshMin(10) {
  url[0] = '\0';
  itemsField[0] = '\0';
  titleField[0] = 't';
  titleField[1] = 'i';
  titleField[2] = 't';
  titleField[3] = 'l';
  titleField[4] = 'e';
  titleField[5] = '\0';
}

NetworkConfig::NetworkConfig()
    : hostname("megapanel"),
      staticIp(false),
      ip(""),
      gateway(""),
      subnet(""),
      dns1(""),
      dns2("") {}

AppConfig::AppConfig() : version(1) {
  copyText(deviceName, sizeof(deviceName), "MegaPanel");
}

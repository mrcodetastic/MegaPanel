#include <Arduino.h>

#include "app/AppConfig.hpp"
#include "core/SceneManager.hpp"
#include "hal/IMatrixPanel.hpp"
#include "hal/NullPanel.hpp"
#include "hal/PanelFactory.hpp"
#include "services/BrightnessService.hpp"
#include "services/ConfigService.hpp"
#include "services/DataService.hpp"
#include "services/DiagnosticsService.hpp"
#include "services/NewsService.hpp"
#include "services/LightSensorService.hpp"
#include "services/NetworkService.hpp"
#include "services/PresenceService.hpp"
#include "services/TimeService.hpp"
#include "services/WeatherService.hpp"
#include "services/WebServerService.hpp"
#include "util/Logger.hpp"

namespace {

ConfigService gConfig;
TimeService gTime;
NetworkService gNetwork;
DiagnosticsService gDiagnostics;
PresenceService gPresence;
LightSensorService gLight;
BrightnessService gBrightness;
WeatherService gWeather;
DataService gData;
NewsService gNews;
SceneManager gScene;
WebServerService gWeb;
std::unique_ptr<IMatrixPanel> gPanel;

void startPanel() {
  gPanel = createPanel(gConfig.get().display);
  PanelConfig panelConfig = makePanelConfig(gConfig.get().display);

  if (!gPanel->begin(panelConfig)) {
    LOGW("main", "primary panel backend failed; falling back to null backend");
    gPanel.reset(new NullPanel());
    panelConfig.logical = ScreenSpec{
        static_cast<uint16_t>(panelConfig.panelWidth * panelConfig.cols),
        static_cast<uint16_t>(panelConfig.panelHeight * panelConfig.rows)};
    if (!gPanel->begin(panelConfig)) {
      LOGE("main", "null panel failed to start");
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  LOGI("main", "MegaPanel %s booting", PANEL_FW_VERSION);

  gConfig.begin();
  gTime.begin(gConfig.get().time);
  gNetwork.begin(gConfig.get().network);

  startPanel();
  if (gPanel && gPanel->ready()) {
    LOGI("main", "panel %s %ux%u", gPanel->backendName(), gPanel->size().width,
         gPanel->size().height);
  }

  gDiagnostics.begin();
  gPresence.begin(gConfig.get().presence);
  gLight.begin(gConfig.get().light);
  gWeather.begin(gConfig.get().weather);
  gData.begin(gConfig.get().data);
  gNews.begin(gConfig.get().news);

  if (!gScene.begin(*gPanel, gConfig, gTime, gNetwork, gPresence, gLight, gDiagnostics,
                    gWeather, gData, gNews)) {
    LOGE("main", "scene begin failed");
  }

  gBrightness.begin(gPanel.get(), &gConfig, &gLight, &gPresence, &gTime);
  gWeb.begin(gPanel.get(), &gConfig, &gScene, &gPresence, &gLight, &gBrightness,
             &gDiagnostics, &gNetwork, &gTime, &gWeather, &gData, &gNews);

  if (gNetwork.connected()) {
    LOGI("main", "ready at http://%s/", gNetwork.ip().c_str());
  } else {
    LOGW("main", "WiFi not connected; AP provisioning portal may be active");
  }
}

void loop() {
  const uint32_t now = millis();
  gDiagnostics.loopStart();

  gTime.loop();
  gNetwork.loop();
  gPresence.loop(now);
  gLight.loop(now);
  gBrightness.loop(now);
  gWeather.loop(now);
  gData.loop(now);
  gNews.loop(now);
  gScene.loop(now);
  gWeb.loop();
  gConfig.loop(now);

  gDiagnostics.loopEnd();
  delay(1);
}

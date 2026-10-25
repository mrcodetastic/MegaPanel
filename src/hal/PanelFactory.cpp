#include "PanelFactory.hpp"

#include "NullPanel.hpp"
#if defined(PANEL_USE_HUB75) && PANEL_USE_HUB75
#include "Hub75DmaPanel.hpp"
#endif

PanelConfig makePanelConfig(const DisplayConfig& display) {
  PanelConfig config;
  config.logical = ScreenSpec{display.logicalWidth, display.logicalHeight};
  config.panelWidth = display.panelWidth;
  config.panelHeight = display.panelHeight;
  config.rows = display.rows;
  config.cols = display.cols;
  config.chainType = display.chainType;
  config.rotation = display.rotation;
  config.driver = display.driver;
  config.clkPhase = display.clkPhase;
  config.doubleBuffer = display.doubleBuffer;
  config.swapBlueGreen = display.swapBlueGreen;
  config.brightness = display.brightness;
  return config;
}

std::unique_ptr<IMatrixPanel> createPanel(const DisplayConfig& config) {
  (void)config;
#if defined(PANEL_USE_HUB75) && PANEL_USE_HUB75
  return std::unique_ptr<IMatrixPanel>(new Hub75DmaPanel());
#else
  return std::unique_ptr<IMatrixPanel>(new NullPanel());
#endif
}

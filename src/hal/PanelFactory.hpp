#pragma once

#include <memory>
#include "IMatrixPanel.hpp"
#include "app/AppConfig.hpp"

PanelConfig makePanelConfig(const DisplayConfig& config);
std::unique_ptr<IMatrixPanel> createPanel(const DisplayConfig& config);

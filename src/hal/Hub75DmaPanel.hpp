#pragma once

#include <memory>
#include "IMatrixPanel.hpp"

class MatrixPanel_I2S_DMA;

// Reference HUB75 backend using ESP32-HUB75-MatrixPanel-I2S-DMA.
// The first release uses drawPixelRGB888 per pixel for portability. A later
// optimisation can build a logical-to-physical index table and copy rows.
class Hub75DmaPanel : public IMatrixPanel {
 public:
  Hub75DmaPanel();
  ~Hub75DmaPanel() override;

  bool begin(const PanelConfig& config) override;
  void end() override;
  bool ready() const override { return ready_; }
  ScreenSpec size() const override { return size_; }
  CRGB* framebuffer() override { return framebuffer_.get(); }
  const CRGB* framebuffer() const override { return framebuffer_.get(); }
  void show() override;
  void waitForScan() override;
  void setBrightness(uint8_t value) override;
  uint8_t brightness() const override { return brightness_; }
  const char* backendName() const override { return "HUB75 DMA"; }

 private:
  MatrixPanel_I2S_DMA* dma_;
  ScreenSpec size_;
  std::unique_ptr<CRGB[]> framebuffer_;
  PanelConfig config_;
  uint8_t brightness_;
  bool ready_;
  bool flippedOnce_;
};

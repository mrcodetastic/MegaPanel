#pragma once

#include <memory>
#include "IMatrixPanel.hpp"

// In-memory backend for fallback and host-style testing.
class NullPanel : public IMatrixPanel {
 public:
  NullPanel();
  ~NullPanel() override;

  bool begin(const PanelConfig& config) override;
  void end() override;
  bool ready() const override { return ready_; }
  ScreenSpec size() const override { return size_; }
  CRGB* framebuffer() override { return framebuffer_.get(); }
  const CRGB* framebuffer() const override { return framebuffer_.get(); }
  void show() override {}
  void setBrightness(uint8_t value) override { brightness_ = value; }
  uint8_t brightness() const override { return brightness_; }
  const char* backendName() const override { return "null"; }

 private:
  ScreenSpec size_;
  std::unique_ptr<CRGB[]> framebuffer_;
  uint8_t brightness_;
  bool ready_;
};

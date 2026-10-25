#pragma once

#include "PanelConfig.hpp"
#include "core/Types.hpp"

// Hardware abstraction for the LED panel. Outputs never see this interface.
class IMatrixPanel {
 public:
  virtual ~IMatrixPanel() = default;

  virtual bool begin(const PanelConfig& config) = 0;
  virtual void end() = 0;
  virtual bool ready() const = 0;

  virtual ScreenSpec size() const = 0;

  // Logical W*H framebuffer owned by the backend. The compositor writes here.
  virtual CRGB* framebuffer() = 0;
  virtual const CRGB* framebuffer() const = 0;

  // Push or flip the framebuffer to the hardware.
  virtual void show() = 0;
  virtual void waitForScan() {}

  virtual void setBrightness(uint8_t value) = 0;
  virtual uint8_t brightness() const = 0;

  virtual const char* backendName() const = 0;
};

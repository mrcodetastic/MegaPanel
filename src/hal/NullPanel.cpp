#include "NullPanel.hpp"

#include <new>

NullPanel::NullPanel() : size_{0, 0}, brightness_(128), ready_(false) {}

NullPanel::~NullPanel() { end(); }

bool NullPanel::begin(const PanelConfig& config) {
  end();
  if (!config.logical.isValid()) return false;

  size_ = config.logical;
  const size_t count = static_cast<size_t>(size_.width) * size_.height;
  framebuffer_.reset(new (std::nothrow) CRGB[count]);
  if (!framebuffer_) {
    size_ = ScreenSpec{0, 0};
    return false;
  }

  for (size_t i = 0; i < count; ++i) framebuffer_[i] = CRGB::Black;
  brightness_ = config.brightness;
  ready_ = true;
  return true;
}

void NullPanel::end() {
  framebuffer_.reset();
  size_ = ScreenSpec{0, 0};
  ready_ = false;
}

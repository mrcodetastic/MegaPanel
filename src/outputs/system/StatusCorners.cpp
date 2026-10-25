#include "StatusCorners.hpp"

#include "core/FrameContext.hpp"
#include "core/OutputContext.hpp"
#include "services/NetworkService.hpp"

StatusCorners::StatusCorners() : screen_{0, 0}, context_(nullptr), started_(false) {}

const OutputMeta& StatusCorners::metaStatic() {
  static const OutputMeta meta = {
      "sys.status",
      "Status corners",
      "1.0",
      "MegaPanel",
      OutputCategory::SystemOverlay,
      LAYER_SYS,
      LayerId::System,
      ScreenSpec{0, 0},
      ScreenSpec{16, 16},
      0,
      0,
      1,
      0,
      false,
      true,
      true,
      false,
      false,
      false};
  return meta;
}

bool StatusCorners::configure(const ScreenSpec& screen, JsonObjectConst config) {
  (void)config;
  screen_ = screen;
  return screen_.isValid();
}

bool StatusCorners::start(OutputContext& ctx) {
  context_ = &ctx;
  started_ = true;
  return true;
}

void StatusCorners::stop() {
  context_ = nullptr;
  started_ = false;
}

void StatusCorners::render(PixelCanvas& target, const FrameContext& frame) {
  (void)frame;
  if (!started_ || !context_ || !context_->network.connected()) return;

  const int16_t x = static_cast<int16_t>(screen_.width - 8);
  const int16_t y = 1;
  const CRGB color(0, 180, 80);

  // Three arcs and a base dot, kept 7x6 so it fits beside any clock.
  target.drawPixel(x + 3, y + 5, color);
  target.drawPixel(x + 2, y + 4, color);
  target.drawPixel(x + 4, y + 4, color);
  target.drawPixel(x + 1, y + 3, color);
  target.drawPixel(x + 5, y + 3, color);
  target.drawPixel(x + 0, y + 2, color);
  target.drawPixel(x + 6, y + 2, color);
}

#include "NotifyOverlay.hpp"

#include "core/FrameContext.hpp"
#include "services/NotificationService.hpp"

NotifyOverlay::NotifyOverlay() : screen_{0, 0}, started_(false) {}

const OutputMeta& NotifyOverlay::metaStatic() {
  static const OutputMeta meta = {
      "sys.notify",
      "Notification banner",
      "1.0",
      "MegaPanel",
      OutputCategory::SystemOverlay,
      LAYER_SYS,
      LayerId::System,
      ScreenSpec{0, 0},
      ScreenSpec{16, 16},
      0,
      0,
      30,
      0,
      true,
      false,
      false,
      false,
      false,
      false};
  return meta;
}

bool NotifyOverlay::configure(const ScreenSpec& screen, JsonObjectConst config) {
  (void)config;
  screen_ = screen;
  return screen_.isValid();
}

bool NotifyOverlay::start(OutputContext& ctx) {
  (void)ctx;
  started_ = true;
  return true;
}

void NotifyOverlay::stop() { started_ = false; }

void NotifyOverlay::render(PixelCanvas& target, const FrameContext& frame) {
  if (!started_) return;
  NotificationService& service = NotificationService::instance();
  if (!service.active()) return;

  const int16_t bandHeight = static_cast<int16_t>(
      screen_.height < 14 ? screen_.height : 14);
  const int16_t bandY = service.position() == 1
                            ? 0
                            : static_cast<int16_t>(screen_.height - bandHeight);
  const int16_t accentY = service.position() == 1
                              ? static_cast<int16_t>(bandHeight - 1)
                              : bandY;
  const int16_t contentY = service.position() == 1 ? 0 : static_cast<int16_t>(bandY + 1);

  target.fillRect(0, bandY, screen_.width, bandHeight, CRGB(0, 0, 0));
  target.drawFastHLine(0, accentY, screen_.width, service.color());

  const int16_t iconWidth = service.icon() >= 0 ? 10 : 0;
  const int16_t textX = static_cast<int16_t>(iconWidth + 3);
  const int16_t available = static_cast<int16_t>(screen_.width - textX - 2);
  const char* text = service.text();

  target.setTextSize(1);
  target.setTextWrap(false);
  target.setTextColor(service.color());
  const int16_t textWidth = target.textWidth(text);
  const int16_t textY = static_cast<int16_t>(contentY + 4);

  if (textWidth <= available) {
    target.setCursor(static_cast<int16_t>(textX + (available - textWidth) / 2), textY);
    target.print(text);
  } else {
    const int16_t gap = 18;
    const int16_t span = static_cast<int16_t>(textWidth + gap);
    const int16_t offset = static_cast<int16_t>((frame.nowMs / 25u) % span);
    const int16_t x = static_cast<int16_t>(textX - offset);
    target.setCursor(x, textY);
    target.print(text);
    target.setCursor(static_cast<int16_t>(x + span), textY);
    target.print(text);
    target.fillRect(0, contentY, textX, static_cast<int16_t>(bandHeight - 2), CRGB(0, 0, 0));
  }

  if (service.icon() >= 0) {
    const int16_t x = 3;
    const int16_t y = static_cast<int16_t>(contentY + 3);
    target.drawRect(x, y, 6, 6, service.color());
    target.fillRect(static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 6), 2, 1, service.color());
  }
}

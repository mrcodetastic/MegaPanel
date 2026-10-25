#include "Hub75DmaPanel.hpp"

#include <new>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "util/Logger.hpp"

Hub75DmaPanel::Hub75DmaPanel()
    : dma_(nullptr),
      size_{0, 0},
      brightness_(128),
      ready_(false),
      flippedOnce_(false) {}

Hub75DmaPanel::~Hub75DmaPanel() { end(); }

bool Hub75DmaPanel::begin(const PanelConfig& config) {
  end();
  config_ = config;

  if (config_.panelWidth == 0 || config_.panelHeight == 0 ||
      config_.rows == 0 || config_.cols == 0) {
    LOGE("panel", "invalid panel geometry");
    return false;
  }

  HUB75_I2S_CFG::i2s_pins pins = {
      config_.pins.r1, config_.pins.g1, config_.pins.b1,
      config_.pins.r2, config_.pins.g2, config_.pins.b2,
      config_.pins.a, config_.pins.b, config_.pins.c, config_.pins.d, config_.pins.e,
      config_.pins.lat, config_.pins.oe, config_.pins.clk};

  const uint8_t chain = static_cast<uint8_t>(config_.rows * config_.cols);
  HUB75_I2S_CFG matrixConfig(config_.panelWidth, config_.panelHeight, chain, pins);
  matrixConfig.driver = config_.driver == 1 ? HUB75_I2S_CFG::ICN2038S
                                            : HUB75_I2S_CFG::FM6126A;
  matrixConfig.clkphase = config_.clkPhase;
  matrixConfig.double_buff = config_.doubleBuffer;

  dma_ = new (std::nothrow) MatrixPanel_I2S_DMA(matrixConfig);
  if (!dma_) {
    LOGE("panel", "failed to allocate HUB75 driver");
    return false;
  }
  if (!dma_->begin()) {
    LOGE("panel", "HUB75 begin failed");
    delete dma_;
    dma_ = nullptr;
    return false;
  }

  dma_->setBrightness8(config_.brightness);
  dma_->clearScreen();
  brightness_ = config_.brightness;

  size_.width = static_cast<uint16_t>(config_.panelWidth * config_.cols);
  size_.height = static_cast<uint16_t>(config_.panelHeight * config_.rows);
  if (size_ != config_.logical) {
    LOGW("panel", "configured logical %ux%u differs from physical %ux%u; using physical",
         config_.logical.width, config_.logical.height, size_.width, size_.height);
  }

  const size_t count = static_cast<size_t>(size_.width) * size_.height;
  framebuffer_.reset(new (std::nothrow) CRGB[count]);
  if (!framebuffer_) {
    LOGE("panel", "failed to allocate %u-byte framebuffer", static_cast<unsigned>(count * 3));
    end();
    return false;
  }
  for (size_t i = 0; i < count; ++i) framebuffer_[i] = CRGB::Black;

  ready_ = true;
  flippedOnce_ = false;
  LOGI("panel", "HUB75 ready: %ux%u, %u panel(s), driver=%s",
       size_.width, size_.height, chain, config_.driver == 1 ? "ICN2038S" : "FM6126A");
  return true;
}

void Hub75DmaPanel::end() {
  framebuffer_.reset();
  if (dma_) {
    dma_->clearScreen();
    delete dma_;
    dma_ = nullptr;
  }
  size_ = ScreenSpec{0, 0};
  ready_ = false;
  flippedOnce_ = false;
}

void Hub75DmaPanel::show() {
  if (!ready_ || !dma_ || !framebuffer_) return;

  for (uint16_t y = 0; y < size_.height; ++y) {
    const uint32_t row = static_cast<uint32_t>(y) * size_.width;
    for (uint16_t x = 0; x < size_.width; ++x) {
      const CRGB& color = framebuffer_[row + x];
      dma_->drawPixelRGB888(x, y, color.r, color.g, color.b);
    }
  }

  if (config_.doubleBuffer) {
    dma_->flipDMABuffer();
    flippedOnce_ = true;
  }
}

void Hub75DmaPanel::waitForScan() {
  if (ready_ && flippedOnce_) {
    delay(1);
  }
}

void Hub75DmaPanel::setBrightness(uint8_t value) {
  brightness_ = value;
  if (dma_) dma_->setBrightness8(value);
}

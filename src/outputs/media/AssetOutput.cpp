#include "AssetOutput.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/FrameContext.hpp"
#include "core/OutputContext.hpp"
#include "services/DataService.hpp"
#include "util/QuoteFormat.hpp"

namespace {

CRGB parseHexColor(const char* text, CRGB fallback) {
  if (!text) return fallback;
  if (*text == '#') ++text;
  if (strlen(text) != 6) return fallback;
  char* end = nullptr;
  const uint32_t value = static_cast<uint32_t>(strtoul(text, &end, 16));
  if (!end || *end != '\0') return fallback;
  return CRGB(static_cast<uint8_t>((value >> 16) & 0xFF),
              static_cast<uint8_t>((value >> 8) & 0xFF),
              static_cast<uint8_t>(value & 0xFF));
}

void colorToHex(CRGB color, char* out, size_t capacity) {
  snprintf(out, capacity, "#%02X%02X%02X", color.r, color.g, color.b);
}

void copyText(char* destination, size_t capacity, const char* source) {
  if (!destination || capacity == 0) return;
  if (!source) {
    destination[0] = '\0';
    return;
  }
  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

}  // namespace

AssetOutput::AssetOutput()
    : screen_{0, 0},
      data_(nullptr),
      source_(0),
      decimals_(-1),
      showPercent_(true),
      thousands_(true),
      upColor_(CRGB(60, 220, 130)),
      downColor_(CRGB(255, 90, 90)),
      flatColor_(CRGB(190, 190, 190)),
      nameColor_(CRGB(150, 165, 180)),
      fps_(5),
      started_(false) {
  label_[0] = '\0';
  copyText(prefix_, sizeof(prefix_), "$");
}

const OutputMeta& AssetOutput::metaStatic() {
  static const OutputMeta meta = {
      "media.asset",
      "Asset quote",
      "1.0",
      "MegaPanel data display, reusable across asset classes",
      OutputCategory::DisplayOutput,
      LAYER_FG,
      LayerId::Foreground,
      ScreenSpec{0, 0},
      ScreenSpec{32, 24},
      0,
      0,
      5,
      15,
      false,
      false,
      true,
      false,
      false,
      false};
  return meta;
}

void AssetOutput::schemaStatic(JsonObject& out) {
  JsonObject source = out["source"].to<JsonObject>();
  source["type"] = "number";
  source["min"] = 0;
  source["max"] = 3;
  source["default"] = 0;
  source["hint"] = "Index of the data source in the Data card";

  JsonObject label = out["label"].to<JsonObject>();
  label["type"] = "string";
  label["default"] = "";
  label["hint"] = "Overrides the name returned by the source";

  JsonObject prefix = out["prefix"].to<JsonObject>();
  prefix["type"] = "string";
  prefix["default"] = "$";

  JsonObject decimals = out["decimals"].to<JsonObject>();
  decimals["type"] = "number";
  decimals["min"] = -1;
  decimals["max"] = 8;
  decimals["default"] = -1;
  decimals["hint"] = "-1 selects decimals from the price magnitude";

  JsonObject showPercent = out["showPercent"].to<JsonObject>();
  showPercent["type"] = "bool";
  showPercent["default"] = true;

  JsonObject thousands = out["thousands"].to<JsonObject>();
  thousands["type"] = "bool";
  thousands["default"] = true;

  JsonObject up = out["upColor"].to<JsonObject>();
  up["type"] = "color";
  up["default"] = "#3CDC82";

  JsonObject down = out["downColor"].to<JsonObject>();
  down["type"] = "color";
  down["default"] = "#FF5A5A";

  JsonObject flat = out["flatColor"].to<JsonObject>();
  flat["type"] = "color";
  flat["default"] = "#BEBEBE";

  JsonObject nameColor = out["nameColor"].to<JsonObject>();
  nameColor["type"] = "color";
  nameColor["default"] = "#96A5B4";

  JsonObject fps = out["fps"].to<JsonObject>();
  fps["type"] = "range";
  fps["min"] = 2;
  fps["max"] = 20;
  fps["default"] = 5;
}

void AssetOutput::applyConfig(JsonObjectConst config) {
  source_ = static_cast<uint8_t>(config["source"] | 0);
  if (source_ > 3) source_ = 3;

  copyText(label_, sizeof(label_), config["label"] | "");
  copyText(prefix_, sizeof(prefix_), config["prefix"] | "$");

  int decimals = config["decimals"] | -1;
  if (decimals < -1) decimals = -1;
  if (decimals > 8) decimals = 8;
  decimals_ = static_cast<int8_t>(decimals);

  showPercent_ = config["showPercent"] | true;
  thousands_ = config["thousands"] | true;
  upColor_ = parseHexColor(config["upColor"] | "#3CDC82", upColor_);
  downColor_ = parseHexColor(config["downColor"] | "#FF5A5A", downColor_);
  flatColor_ = parseHexColor(config["flatColor"] | "#BEBEBE", flatColor_);
  nameColor_ = parseHexColor(config["nameColor"] | "#96A5B4", nameColor_);

  const uint8_t fps = static_cast<uint8_t>(config["fps"] | 5);
  fps_ = fps < 2 ? 2 : (fps > 20 ? 20 : fps);
}

bool AssetOutput::configure(const ScreenSpec& screen, JsonObjectConst config) {
  screen_ = screen;
  applyConfig(config);
  return screen_.isValid();
}

void AssetOutput::onConfigChanged(JsonObjectConst config) { applyConfig(config); }

bool AssetOutput::start(OutputContext& ctx) {
  data_ = ctx.data;
  started_ = true;
  return true;
}

void AssetOutput::stop() {
  started_ = false;
  data_ = nullptr;
}

void AssetOutput::render(PixelCanvas& target, const FrameContext& frame) {
  if (!started_) return;
  target.clear(CRGB(0, 0, 0));

  const int16_t width = static_cast<int16_t>(screen_.width);
  const int16_t height = static_cast<int16_t>(screen_.height);
  const DataQuote quote = data_ ? data_->quote(source_) : DataQuote();

  char name[24];
  if (label_[0]) {
    copyText(name, sizeof(name), label_);
  } else if (quote.name[0]) {
    copyText(name, sizeof(name), quote.name);
  } else {
    copyText(name, sizeof(name), "ASSET");
  }

  target.setTextSize(1);
  target.setTextColor(nameColor_);
  char header[28];
  if (quote.stale) {
    snprintf(header, sizeof(header), "~%s", name);
  } else {
    snprintf(header, sizeof(header), "%s", name);
  }
  const int16_t headerWidth = target.textWidth(header);
  target.setCursor(static_cast<int16_t>((width - headerWidth) / 2), 0);
  target.print(header);

  const int16_t bandTop = 9;
  const int16_t bandBottom = static_cast<int16_t>(height - 10);

  if (!quote.valid) {
    const char* message = "NO DATA";
    if (data_ && data_->fetching()) message = "FETCHING";
    target.setTextColor(flatColor_);
    const int16_t messageWidth = target.textWidth(message);
    target.setCursor(static_cast<int16_t>((width - messageWidth) / 2),
                     static_cast<int16_t>(bandTop + (bandBottom - bandTop - 8) / 2));
    target.print(message);

    if (quote.error[0]) {
      target.setTextColor(CRGB(110, 120, 130));
      char error[24];
      copyText(error, sizeof(error), quote.error);
      const int16_t errorWidth = target.textWidth(error);
      target.setCursor(static_cast<int16_t>((width - errorWidth) / 2),
                       static_cast<int16_t>(height - 9));
      target.print(error);
    }
    return;
  }

  char grouped[24];
  char plain[24];
  char withPrefix[40];
  char prefixed[40];
  formatQuotePrice(grouped, sizeof(grouped), quote.price, decimals_, thousands_);
  formatQuotePrice(plain, sizeof(plain), quote.price, decimals_, false);
  snprintf(prefixed, sizeof(prefixed), "%s%s", prefix_, grouped);
  snprintf(withPrefix, sizeof(withPrefix), "%s%s", prefix_, plain);

  const int16_t maxSize = static_cast<int16_t>(((bandBottom - bandTop) / 8) > 4 ? 4 : ((bandBottom - bandTop) / 8));
  const char* chosen = prefixed;
  int16_t size = maxSize < 1 ? 1 : maxSize;

  // Prefer the largest size, then grouped without the currency prefix, then
  // the plain number, so a 64 px panel still gets a readable price.
  const char* candidates[3] = {prefixed, withPrefix, plain};
  bool found = false;
  // Size first, then the richest text that fits: a panel shows 5 digits at
  // size 2 rather than "$64,321" at size 1.
  for (int16_t trial = (maxSize < 1 ? 1 : maxSize); trial >= 1 && !found; --trial) {
    for (int16_t candidate = 0; candidate < 3; ++candidate) {
      target.setTextSize(static_cast<uint8_t>(trial));
      if (target.textWidth(candidates[candidate]) <= width - 2) {
        chosen = candidates[candidate];
        size = trial;
        found = true;
        break;
      }
    }
  }
  if (!found) {
    chosen = plain;
    size = 1;
  }

  const int16_t priceHeight = static_cast<int16_t>(8 * size);
  const int16_t priceY = static_cast<int16_t>(bandTop + (bandBottom - bandTop - priceHeight) / 2);
  target.setTextSize(static_cast<uint8_t>(size));
  target.setTextColor(quote.stale ? CRGB(170, 175, 180) : CRGB(255, 255, 255));
  const int16_t priceWidth = target.textWidth(chosen);
  target.setCursor(static_cast<int16_t>((width - priceWidth) / 2), priceY);
  target.print(chosen);

  const QuoteTrend trend = quote.trend();
  const CRGB movementColor =
      trend == QuoteTrend::Up ? upColor_ : (trend == QuoteTrend::Down ? downColor_ : flatColor_);

  char percent[16];
  char change[24];
  formatQuotePercent(percent, sizeof(percent), quote.changePercent);
  formatQuoteChange(change, sizeof(change), quote.change, quote.price, decimals_);
  const char* movement = showPercent_ ? percent : change;

  target.setTextSize(1);
  const int16_t movementWidth = target.textWidth(movement);
  const int16_t totalWidth = static_cast<int16_t>(movementWidth + 7);
  const int16_t movementX = static_cast<int16_t>((width - totalWidth) / 2);
  const int16_t movementY = static_cast<int16_t>(height - 9);

  if (trend == QuoteTrend::Up) {
    target.fillTriangle(movementX, static_cast<int16_t>(movementY + 5),
                        static_cast<int16_t>(movementX + 5), static_cast<int16_t>(movementY + 5),
                        static_cast<int16_t>(movementX + 2), static_cast<int16_t>(movementY + 1),
                        movementColor);
  } else if (trend == QuoteTrend::Down) {
    target.fillTriangle(movementX, static_cast<int16_t>(movementY + 1),
                        static_cast<int16_t>(movementX + 5), static_cast<int16_t>(movementY + 1),
                        static_cast<int16_t>(movementX + 2), static_cast<int16_t>(movementY + 5),
                        movementColor);
  } else {
    target.fillRect(static_cast<int16_t>(movementX + 1), static_cast<int16_t>(movementY + 3), 4, 1,
                    movementColor);
  }

  target.setTextColor(movementColor);
  target.setCursor(static_cast<int16_t>(movementX + 7), movementY);
  target.print(movement);

  (void)frame;
}

void AssetOutput::writeConfig(JsonObject& out) const {
  out["source"] = source_;
  out["label"] = label_;
  out["prefix"] = prefix_;
  out["decimals"] = decimals_;
  out["showPercent"] = showPercent_;
  out["thousands"] = thousands_;
  char hex[8];
  colorToHex(upColor_, hex, sizeof(hex));
  out["upColor"] = hex;
  colorToHex(downColor_, hex, sizeof(hex));
  out["downColor"] = hex;
  colorToHex(flatColor_, hex, sizeof(hex));
  out["flatColor"] = hex;
  colorToHex(nameColor_, hex, sizeof(hex));
  out["nameColor"] = hex;
  out["fps"] = fps_;
}

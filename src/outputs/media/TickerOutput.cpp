#include "TickerOutput.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "core/FrameContext.hpp"
#include "core/OutputContext.hpp"
#include "services/DataService.hpp"
#include "services/NewsService.hpp"
#include "util/QuoteFormat.hpp"

namespace {

void copyTextFallback(char* destination, size_t capacity, const char* source, const char* fallback) {
  if (!destination || capacity == 0) return;
  const char* text = (source && *source) ? source : fallback;
  strncpy(destination, text, capacity - 1);
  destination[capacity - 1] = '\0';
}

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

}  // namespace

TickerOutput::TickerOutput()
    : screen_{0, 0},
      color_(CRGB::White),
      speed_(40),
      started_(false),
      source_(-1),
      newsMode_(false),
      data_(nullptr),
      news_(nullptr),
      scrollX_(0) {
  strncpy(text_, "MegaPanel", sizeof(text_) - 1);
  strncpy(template_, "{name} {price} ({percent})", sizeof(template_) - 1);
}

const OutputMeta& TickerOutput::metaStatic() {
  static const OutputMeta meta = {
      "media.ticker",
      "Scrolling ticker",
      "1.0",
      "MegaPanel",
      OutputCategory::DisplayOutput,
      LAYER_FG,
      LayerId::Foreground,
      ScreenSpec{0, 0},
      ScreenSpec{16, 16},
      0,
      0,
      30,
      20,
      true,
      false,
      false,
      false,
      false,
      false};
  return meta;
}

void TickerOutput::schemaStatic(JsonObject& out) {
  JsonObject text = out["text"].to<JsonObject>();
  text["type"] = "string";
  text["default"] = "MegaPanel";

  JsonObject color = out["color"].to<JsonObject>();
  color["type"] = "color";
  color["default"] = "#FFFFFF";

  JsonObject speed = out["speed"].to<JsonObject>();
  speed["type"] = "range";
  speed["min"] = 5;
  speed["max"] = 120;
  speed["step"] = 1;
  speed["default"] = 40;

  JsonObject source = out["source"].to<JsonObject>();
  source["type"] = "number";
  source["min"] = -1;
  source["max"] = 3;
  source["default"] = -1;
  source["hint"] = "-1 uses the fixed text; 0-3 reads a Data card source";

  JsonObject tmpl = out["template"].to<JsonObject>();
  tmpl["type"] = "string";
  tmpl["default"] = "{name} {price} ({percent})";
  tmpl["hint"] = "Tokens: {name} {price} {change} {percent}";

  JsonObject news = out["news"].to<JsonObject>();
  news["type"] = "bool";
  news["default"] = false;
  news["hint"] = "Scroll the News feed headlines instead of the text";
}

void TickerOutput::applyConfig(JsonObjectConst config) {
  const char* text = config["text"] | "MegaPanel";
  strncpy(text_, text, sizeof(text_) - 1);
  text_[sizeof(text_) - 1] = '\0';

  const char* tmpl = config["template"] | "{name} {price} ({percent})";
  strncpy(template_, tmpl, sizeof(template_) - 1);
  template_[sizeof(template_) - 1] = '\0';

  int source = config["source"] | -1;
  if (source < -1) source = -1;
  if (source > 3) source = 3;
  source_ = static_cast<int8_t>(source);

  newsMode_ = config["news"] | false;

  color_ = parseHexColor(config["color"] | "#FFFFFF", CRGB::White);
  speed_ = static_cast<uint8_t>(config["speed"] | 40);
  if (speed_ < 5) speed_ = 5;
}

void TickerOutput::buildText(char* out, size_t capacity) const {
  if (!out || capacity == 0) return;
  out[0] = '\0';

  const DataQuote quote = data_ ? data_->quote(static_cast<uint8_t>(source_)) : DataQuote();
  char name[24] = "--";
  char price[24] = "--";
  char change[24] = "--";
  char percent[16] = "--";
  if (quote.valid) {
    copyTextFallback(name, sizeof(name), quote.name, "ASSET");
    formatQuotePrice(price, sizeof(price), quote.price, -1, true);
    formatQuoteChange(change, sizeof(change), quote.change, quote.price, -1);
    formatQuotePercent(percent, sizeof(percent), quote.changePercent);
  }

  size_t written = 0;
  const char* cursor = template_;
  while (*cursor && written + 1 < capacity) {
    if (*cursor == '{') {
      const char* close = strchr(cursor, '}');
      if (close) {
        const size_t tokenLength = static_cast<size_t>(close - cursor - 1);
        const char* replacement = nullptr;
        if (tokenLength == 4 && strncmp(cursor + 1, "name", 4) == 0) replacement = name;
        else if (tokenLength == 5 && strncmp(cursor + 1, "price", 5) == 0) replacement = price;
        else if (tokenLength == 6 && strncmp(cursor + 1, "change", 6) == 0) replacement = change;
        else if (tokenLength == 7 && strncmp(cursor + 1, "percent", 7) == 0) replacement = percent;

        if (replacement) {
          const size_t length = strlen(replacement);
          const size_t room = capacity - written - 1;
          const size_t take = length < room ? length : room;
          memcpy(out + written, replacement, take);
          written += take;
          out[written] = '\0';
          cursor = close + 1;
          continue;
        }
      }
    }
    out[written++] = *cursor++;
    out[written] = '\0';
  }
}

bool TickerOutput::configure(const ScreenSpec& screen, JsonObjectConst config) {
  screen_ = screen;
  applyConfig(config);
  return screen_.isValid();
}

void TickerOutput::onConfigChanged(JsonObjectConst config) { applyConfig(config); }

void TickerOutput::buildNewsText(char* out, size_t capacity) const {
  if (!out || capacity == 0) return;
  out[0] = '\0';
  if (!news_) return;

  const NewsFeed feed = news_->snapshot();
  if (!feed.valid) {
    copyTextFallback(out, capacity, nullptr, "NEWS UNAVAILABLE");
    return;
  }

  size_t written = 0;
  for (uint8_t i = 0; i < feed.count; ++i) {
    const char* separator = written > 0 ? "  |  " : "";
    const int result = snprintf(out + written, capacity - written, "%s%s", separator,
                                feed.items[i]);
    if (result < 0) break;
    const size_t added = static_cast<size_t>(result);
    if (added >= capacity - written) {
      written = capacity - 1;
      break;
    }
    written += added;
  }
  out[written] = '\0';
}

bool TickerOutput::start(OutputContext& ctx) {
  data_ = ctx.data;
  news_ = ctx.news;
  started_ = true;
  scrollX_ = 0;
  return true;
}

void TickerOutput::stop() {
  started_ = false;
  data_ = nullptr;
  news_ = nullptr;
}

void TickerOutput::render(PixelCanvas& target, const FrameContext& frame) {
  if (!started_) return;
  target.clear(CRGB(0, 0, 0));
  target.setTextSize(1);
  target.setTextWrap(false);
  target.setTextColor(color_);

  // Large enough for several joined headlines; local to one render call.
  char buffer[384];
  const char* text = text_;
  if (newsMode_ && news_) {
    buildNewsText(buffer, sizeof(buffer));
    if (buffer[0]) text = buffer;
  } else if (source_ >= 0 && data_) {
    buildText(buffer, sizeof(buffer));
    if (buffer[0]) text = buffer;
  }

  const int16_t width = target.textWidth(text);
  if (width <= screen_.width - 2) {
    const int16_t x = static_cast<int16_t>((screen_.width - width) / 2);
    const int16_t y = static_cast<int16_t>((screen_.height - 8) / 2);
    target.setCursor(x, y);
    target.print(text);
    return;
  }

  scrollX_ += static_cast<float>(speed_) * static_cast<float>(frame.deltaMs) / 1000.0f;
  const int16_t gap = 20;
  const int16_t span = static_cast<int16_t>(width + gap);
  if (scrollX_ >= span) scrollX_ -= span;

  const int16_t x = static_cast<int16_t>(screen_.width - scrollX_);
  const int16_t y = static_cast<int16_t>((screen_.height - 8) / 2);
  target.setCursor(x, y);
  target.print(text);
  target.setCursor(static_cast<int16_t>(x + span), y);
  target.print(text);
}

void TickerOutput::writeConfig(JsonObject& out) const {
  out["text"] = text_;
  out["source"] = source_;
  out["template"] = template_;
  out["news"] = newsMode_;
  char hex[8];
  colorToHex(color_, hex, sizeof(hex));
  out["color"] = hex;
  out["speed"] = speed_;
}

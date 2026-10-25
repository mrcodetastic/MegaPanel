#include "StaticClocks.hpp"

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include "core/FrameContext.hpp"
#include "worldmap.h"

namespace {

void formatTimeText(const FrameContext& frame, char* out, size_t capacity) {
  if (!frame.timeValid) {
    snprintf(out, capacity, "--:--");
    return;
  }
  int hour = frame.localTime.tm_hour;
  if (!frame.use24Hour) {
    hour %= 12;
    if (hour == 0) hour = 12;
  }
  snprintf(out, capacity, "%02d:%02d", hour, frame.localTime.tm_min);
}

void formatDateText(const FrameContext& frame, char* out, size_t capacity) {
  if (!frame.timeValid) {
    snprintf(out, capacity, "--/--/----");
    return;
  }
  const int day = frame.localTime.tm_mday;
  const int month = frame.localTime.tm_mon + 1;
  const int year = frame.localTime.tm_year + 1900;
  switch (frame.dateFormat) {
    case 1: snprintf(out, capacity, "%02d/%02d/%04d", month, day, year); break;
    case 2: snprintf(out, capacity, "%04d-%02d-%02d", year, month, day); break;
    case 3: snprintf(out, capacity, "%02d.%02d.%04d", day, month, year); break;
    default: snprintf(out, capacity, "%02d/%02d/%04d", day, month, year); break;
  }
}

uint8_t fitTextSize(const ScreenSpec& screen, uint8_t chars, uint8_t charWidth, uint8_t charHeight,
                    int16_t reservedHeight) {
  int16_t available = static_cast<int16_t>(screen.height - reservedHeight - 2);
  if (available < charHeight) available = charHeight;
  int size = (screen.width - 4) / (chars * charWidth);
  const int heightSize = available / charHeight;
  if (heightSize < size) size = heightSize;
  if (size < 1) size = 1;
  if (size > 6) size = 6;
  return static_cast<uint8_t>(size);
}

void drawSmallDate(PixelCanvas& target, const FrameContext& frame, CRGB color) {
  char dateText[16];
  formatDateText(frame, dateText, sizeof(dateText));
  target.setTextSize(1);
  target.setTextColor(color);
  const int16_t w = target.textWidth(dateText);
  target.setCursor(static_cast<int16_t>((target.width() - w) / 2),
                   static_cast<int16_t>(target.height() - 9));
  target.print(dateText);
}

}  // namespace

// ---------------------------------------------------------------------------
// Large clock
// ---------------------------------------------------------------------------
void ClockLarge::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!frame.timeValid) {
    target.drawTextCentered("SYNC", static_cast<int16_t>(screen_.height / 2 - 4), color_, 1);
    return;
  }

  char timeText[8];
  formatTimeText(frame, timeText, sizeof(timeText));
  const int16_t reserved = showDate_ ? 10 : 2;
  const uint8_t size = fitTextSize(screen_, 5, 6, 8, reserved);
  const int16_t textW = static_cast<int16_t>(5 * 6 * size);
  const int16_t textH = static_cast<int16_t>(8 * size);
  const int16_t x = static_cast<int16_t>((screen_.width - textW) / 2);
  const int16_t available = static_cast<int16_t>(screen_.height - reserved - 2);
  const int16_t y = static_cast<int16_t>((available - textH) / 2 + 1);

  target.setTextSize(size);
  target.setTextColor(color_);
  target.setCursor(x, y);
  target.print(timeText);

  if (showDate_) drawSmallDate(target, frame, CRGB(150, 150, 150));
}

// ---------------------------------------------------------------------------
// Time in words
// ---------------------------------------------------------------------------
void ClockWords::drawClock(PixelCanvas& target, const FrameContext& frame) {
  if (!frame.timeValid) {
    target.drawTextCentered("SYNC", static_cast<int16_t>(screen_.height / 2 - 4), color_, 1);
    return;
  }

  static const char* const hours[12] = {
      "TWELVE", "ONE", "TWO", "THREE", "FOUR", "FIVE",
      "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN"};
  static const char* const phrases[12] = {
      "OCLOCK", "FIVE PAST", "TEN PAST", "QUARTER PAST",
      "TWENTY PAST", "TWENTY FIVE PAST", "HALF PAST",
      "TWENTY FIVE TO", "TWENTY TO", "QUARTER TO", "TEN TO", "FIVE TO"};

  const int hour24 = frame.localTime.tm_hour;
  const int minute = frame.localTime.tm_min;
  int bucket = ((minute + 2) / 5) % 12;
  int hourIndex = hour24 % 12;
  if (bucket >= 7) hourIndex = (hourIndex + 1) % 12;

  char line[32];
  snprintf(line, sizeof(line), "%s", phrases[bucket]);

  const int maxChars = (screen_.width - 4) / 6;
  char lines[4][24];
  uint8_t lineCount = 0;
  char current[24] = {0};
  const char* cursor = line;
  while (*cursor && lineCount < 3) {
    while (*cursor == ' ') ++cursor;
    if (!*cursor) break;
    const char* end = cursor;
    while (*end && *end != ' ') ++end;
    const size_t wordLen = static_cast<size_t>(end - cursor);
    const size_t currentLen = strlen(current);
    const size_t needed = currentLen + (currentLen ? 1 : 0) + wordLen;
    if (currentLen && static_cast<int>(needed) > maxChars) {
      snprintf(lines[lineCount++], sizeof(lines[0]), "%s", current);
      current[0] = '\0';
    }
    if (current[0]) strncat(current, " ", sizeof(current) - strlen(current) - 1);
    strncat(current, cursor, sizeof(current) - strlen(current) - 1);
    cursor = end;
  }
  if (current[0] && lineCount < 3) {
    snprintf(lines[lineCount++], sizeof(lines[0]), "%s", current);
  }

  if (lineCount < 3) {
    snprintf(lines[lineCount++], sizeof(lines[0]), "%s", hours[hourIndex]);
  }

  const int16_t lineHeight = 9;
  int16_t y = 2;
  target.setTextSize(1);
  target.setTextColor(color_);
  for (uint8_t i = 0; i < lineCount; ++i) {
    const int16_t w = target.textWidth(lines[i]);
    target.setCursor(static_cast<int16_t>((screen_.width - w) / 2), y);
    target.print(lines[i]);
    y = static_cast<int16_t>(y + lineHeight);
  }

  if (showDate_ && y < screen_.height - 8) {
    char dateText[16];
    formatDateText(frame, dateText, sizeof(dateText));
    target.setTextColor(CRGB(140, 140, 140));
    const int16_t w = target.textWidth(dateText);
    target.setCursor(static_cast<int16_t>((screen_.width - w) / 2),
                     static_cast<int16_t>(screen_.height - 9));
    target.print(dateText);
  }
}

// ---------------------------------------------------------------------------
// Clockwise world map
// ---------------------------------------------------------------------------
void ClockWorldMapCw::drawClock(PixelCanvas& target, const FrameContext& frame) {
  constexpr int16_t kMapW = 108;
  constexpr int16_t kMapH = 54;
  const int16_t visibleW = screen_.width < kMapW ? screen_.width : kMapW;
  const int16_t visibleH = screen_.height < kMapH ? screen_.height : kMapH;
  const int16_t yOffset = static_cast<int16_t>((screen_.height - visibleH) / 2);
  const int16_t xOffset = visibleW < kMapW
                              ? static_cast<int16_t>((frame.localTime.tm_hour * 5) % (kMapW - visibleW))
                              : 0;

  for (int16_t y = 0; y < visibleH; ++y) {
    for (int16_t x = 0; x < visibleW; ++x) {
      const int16_t mapX = static_cast<int16_t>(x + xOffset);
      const int16_t mapY = static_cast<int16_t>(y + (kMapH - visibleH) / 2);
      const uint16_t pixel = pgm_read_word(&_WORLD_MAP[static_cast<uint32_t>(mapY) * kMapW + mapX]);
      if (pixel != 0xF81F) {
        target.drawPixel(x, static_cast<int16_t>(y + yOffset), target.color565_to_CRGB(pixel));
      }
    }
  }

  char timeText[8];
  formatTimeText(frame, timeText, sizeof(timeText));
  target.fillRect(0, static_cast<int16_t>(screen_.height - 10), screen_.width, 10, CRGB(0, 0, 0));
  target.setTextSize(1);
  target.setTextColor(color_);
  const int16_t w = target.textWidth(timeText);
  target.setCursor(static_cast<int16_t>((screen_.width - w) / 2),
                   static_cast<int16_t>(screen_.height - 9));
  target.print(timeText);
}

// ---------------------------------------------------------------------------
// Compact Mario clock
// ---------------------------------------------------------------------------
void ClockMarioCw::drawClock(PixelCanvas& target, const FrameContext& frame) {
  target.clear(CRGB(70, 160, 245));
  target.fillRect(0, static_cast<int16_t>(screen_.height - 5), screen_.width, 5, CRGB(0, 140, 0));
  target.fillRect(0, static_cast<int16_t>(screen_.height - 18), 10, 13, CRGB(55, 140, 55));
  target.fillRect(static_cast<int16_t>(screen_.width - 12), static_cast<int16_t>(screen_.height - 22),
                  12, 17, CRGB(55, 140, 55));

  const int16_t cloudX = static_cast<int16_t>((frame.nowMs / 80u) % (screen_.width + 20)) - 20;
  target.fillRect(cloudX, 8, 10, 3, CRGB(235, 245, 255));
  target.fillRect(static_cast<int16_t>(cloudX + 3), 6, 5, 2, CRGB(235, 245, 255));

  char timeText[8];
  formatTimeText(frame, timeText, sizeof(timeText));
  const uint8_t size = fitTextSize(screen_, 5, 6, 8, 22);
  const int16_t textW = static_cast<int16_t>(5 * 6 * size);
  const int16_t x = static_cast<int16_t>((screen_.width - textW) / 2);
  target.setTextSize(size);
  target.setTextColor(color_);
  target.setCursor(x, 8);
  target.print(timeText);

  const int16_t marioX = static_cast<int16_t>((frame.localTime.tm_sec * (screen_.width - 8)) / 60);
  const int16_t marioY = static_cast<int16_t>(screen_.height - 16);
  target.fillRect(marioX, marioY, 6, 2, CRGB(220, 30, 30));           // hat
  target.fillRect(marioX, static_cast<int16_t>(marioY + 2), 6, 4, CRGB(245, 190, 150));  // face
  target.fillRect(marioX, static_cast<int16_t>(marioY + 6), 6, 4, CRGB(35, 70, 210));    // overalls
  target.fillRect(marioX, static_cast<int16_t>(marioY + 10), 6, 1, CRGB(100, 60, 20));   // shoes

  if (showDate_) {
    char dateText[16];
    formatDateText(frame, dateText, sizeof(dateText));
    target.setTextSize(1);
    target.setTextColor(CRGB(255, 255, 255));
    const int16_t w = target.textWidth(dateText);
    target.setCursor(static_cast<int16_t>((screen_.width - w) / 2),
                     static_cast<int16_t>(screen_.height - 18));
    target.print(dateText);
  }
}

// ---------------------------------------------------------------------------
// Compact Pac-Man clock
// ---------------------------------------------------------------------------
void ClockPacmanCw::drawClock(PixelCanvas& target, const FrameContext& frame) {
  target.clear(CRGB::Black);
  const CRGB wall(30, 30, 170);
  target.drawRect(0, 0, screen_.width, screen_.height, wall);
  target.drawRect(1, 1, static_cast<int16_t>(screen_.width - 2), static_cast<int16_t>(screen_.height - 2), wall);
  target.fillRect(0, static_cast<int16_t>(screen_.height / 2), 3, 2, wall);
  target.fillRect(static_cast<int16_t>(screen_.width - 3), static_cast<int16_t>(screen_.height / 2), 3, 2, wall);

  for (int16_t x = 5; x < screen_.width - 4; x += 8) {
    target.drawPixel(x, static_cast<int16_t>(screen_.height / 2), CRGB(220, 170, 40));
  }

  char timeText[8];
  formatTimeText(frame, timeText, sizeof(timeText));
  const uint8_t size = fitTextSize(screen_, 5, 6, 8, 24);
  const int16_t textW = static_cast<int16_t>(5 * 6 * size);
  target.setTextSize(size);
  target.setTextColor(color_);
  target.setCursor(static_cast<int16_t>((screen_.width - textW) / 2), 5);
  target.print(timeText);

  const int16_t pacX = static_cast<int16_t>((frame.localTime.tm_sec * (screen_.width - 10)) / 60);
  const int16_t pacY = static_cast<int16_t>(screen_.height - 12);
  target.fillCircle(pacX, pacY, 4, CRGB(250, 220, 20));
  const uint8_t mouth = static_cast<uint8_t>((frame.nowMs / 120u) % 2u);
  if (mouth) {
    target.fillTriangle(pacX, pacY, static_cast<int16_t>(pacX + 6), static_cast<int16_t>(pacY - 4),
                        static_cast<int16_t>(pacX + 6), static_cast<int16_t>(pacY + 4), CRGB(0, 0, 0));
  } else {
    target.fillTriangle(pacX, pacY, static_cast<int16_t>(pacX + 6), pacY,
                        static_cast<int16_t>(pacX + 4), static_cast<int16_t>(pacY + 3), CRGB(0, 0, 0));
  }

  const int16_t ghostX = static_cast<int16_t>(screen_.width - 8);
  target.fillRect(ghostX, static_cast<int16_t>(pacY - 3), 5, 5, CRGB(250, 60, 60));
  target.fillRect(ghostX, pacY, 5, 2, CRGB(250, 60, 60));

  if (showDate_) {
    char dateText[16];
    formatDateText(frame, dateText, sizeof(dateText));
    target.setTextSize(1);
    target.setTextColor(CRGB(150, 150, 150));
    const int16_t w = target.textWidth(dateText);
    target.setCursor(static_cast<int16_t>((screen_.width - w) / 2),
                     static_cast<int16_t>(screen_.height - 9));
    target.print(dateText);
  }
}

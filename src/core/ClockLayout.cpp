#include "ClockLayout.hpp"

ClockLayout::ClockLayout()
    : arrangement(ClockArrangement::Custom),
      time{0, 0, 0, 0},
      digits{},
      date{0, 0, 0, 0},
      playArea{0, 0, 0, 0},
      groundY(0),
      patrolY(0),
      digitW(0),
      digitH(0),
      gap(0),
      textSize(1) {}

ClockLayout ClockLayout::choose(const ScreenSpec& screen,
                                uint8_t requestedDigitW,
                                uint8_t requestedDigitH,
                                uint8_t requestedGap,
                                bool withDate) {
  ClockLayout out;
  out.digitW = requestedDigitW;
  out.digitH = requestedDigitH;
  out.gap = requestedGap;
  out.groundY = static_cast<int16_t>(screen.height > 4 ? screen.height - 4 : 0);
  out.patrolY = static_cast<int16_t>(screen.height > 8 ? screen.height - 8 : 0);

  if (screen.width == 0 || screen.height == 0 || requestedDigitW == 0 || requestedDigitH == 0) {
    return out;
  }

  Layout layout(screen, 1);
  const int16_t dateH = withDate ? 10 : 0;
  int16_t availableH = static_cast<int16_t>(screen.height - dateH - 4);
  if (availableH < 8) availableH = static_cast<int16_t>(screen.height);

  const int16_t colonW = requestedGap < 2 ? 2 : requestedGap;
  const int16_t maxW = static_cast<int16_t>(screen.width - 4);

  // 1. Five slots on one row: H H : M M
  int16_t totalW = static_cast<int16_t>(4 * requestedDigitW + colonW + 4 * requestedGap);
  if (totalW <= maxW && requestedDigitH <= availableH) {
    const int16_t x = static_cast<int16_t>((screen.width - totalW) / 2);
    const int16_t y = static_cast<int16_t>((availableH - requestedDigitH) / 2 + 2);
    out.arrangement = ClockArrangement::FiveAcross;
    out.digits[0] = Rect{x, y, requestedDigitW, requestedDigitH};
    out.digits[1] = Rect{static_cast<int16_t>(x + requestedDigitW + requestedGap), y,
                         requestedDigitW, requestedDigitH};
    out.digits[2] = Rect{static_cast<int16_t>(x + 2 * (requestedDigitW + requestedGap)),
                         static_cast<int16_t>(y + requestedDigitH / 2 - 1), colonW, 2};
    out.digits[3] = Rect{static_cast<int16_t>(x + 2 * (requestedDigitW + requestedGap) + colonW + requestedGap),
                         y, requestedDigitW, requestedDigitH};
    out.digits[4] = Rect{static_cast<int16_t>(x + 3 * (requestedDigitW + requestedGap) + colonW + requestedGap),
                         y, requestedDigitW, requestedDigitH};
    out.time = Rect{x, y, totalW, requestedDigitH};
    out.playArea = Rect{0, 0, screen.width, static_cast<int16_t>(screen.height - dateH)};
    out.date = withDate
                   ? Rect{0, static_cast<int16_t>(screen.height - dateH), screen.width, dateH}
                   : Rect{0, 0, 0, 0};
    return out;
  }

  // 2. Two rows: HH over MM
  const int16_t rowW = static_cast<int16_t>(2 * requestedDigitW + requestedGap);
  const int16_t rowH = static_cast<int16_t>(2 * requestedDigitH + requestedGap);
  if (rowW <= maxW && rowH <= availableH) {
    const int16_t x = static_cast<int16_t>((screen.width - rowW) / 2);
    const int16_t y = static_cast<int16_t>((availableH - rowH) / 2 + 2);
    out.arrangement = ClockArrangement::TwoRows;
    out.digits[0] = Rect{x, y, requestedDigitW, requestedDigitH};
    out.digits[1] = Rect{static_cast<int16_t>(x + requestedDigitW + requestedGap), y,
                         requestedDigitW, requestedDigitH};
    out.digits[2] = Rect{static_cast<int16_t>(screen.width / 2 - 1),
                         static_cast<int16_t>(y + requestedDigitH - 1), 2, 2};
    out.digits[3] = Rect{x, static_cast<int16_t>(y + requestedDigitH + requestedGap),
                         requestedDigitW, requestedDigitH};
    out.digits[4] = Rect{static_cast<int16_t>(x + requestedDigitW + requestedGap),
                         static_cast<int16_t>(y + requestedDigitH + requestedGap),
                         requestedDigitW, requestedDigitH};
    out.time = Rect{x, y, rowW, rowH};
    out.playArea = Rect{0, 0, screen.width, static_cast<int16_t>(screen.height - dateH)};
    out.date = withDate
                   ? Rect{0, static_cast<int16_t>(screen.height - dateH), screen.width, dateH}
                   : Rect{0, 0, 0, 0};
    return out;
  }

  // 3. Single row with reduced digit width
  int16_t fitW = static_cast<int16_t>((screen.width - 4 - 4 * requestedGap - colonW) / 4);
  if (fitW >= 3) {
    totalW = static_cast<int16_t>(4 * fitW + colonW + 4 * requestedGap);
    if (totalW <= maxW && requestedDigitH <= availableH) {
      const int16_t x = static_cast<int16_t>((screen.width - totalW) / 2);
      const int16_t y = static_cast<int16_t>((availableH - requestedDigitH) / 2 + 2);
      out.arrangement = ClockArrangement::SingleRow;
      out.digitW = static_cast<uint8_t>(fitW);
      out.digits[0] = Rect{x, y, fitW, requestedDigitH};
      out.digits[1] = Rect{static_cast<int16_t>(x + fitW + requestedGap), y, fitW, requestedDigitH};
      out.digits[2] = Rect{static_cast<int16_t>(x + 2 * (fitW + requestedGap)),
                           static_cast<int16_t>(y + requestedDigitH / 2 - 1), colonW, 2};
      out.digits[3] = Rect{static_cast<int16_t>(x + 2 * (fitW + requestedGap) + colonW + requestedGap),
                           y, fitW, requestedDigitH};
      out.digits[4] = Rect{static_cast<int16_t>(x + 3 * (fitW + requestedGap) + colonW + requestedGap),
                           y, fitW, requestedDigitH};
      out.time = Rect{x, y, totalW, requestedDigitH};
      out.playArea = Rect{0, 0, screen.width, static_cast<int16_t>(screen.height - dateH)};
      out.date = withDate
                     ? Rect{0, static_cast<int16_t>(screen.height - dateH), screen.width, dateH}
                     : Rect{0, 0, 0, 0};
      return out;
    }
  }

  // 4. Fallback: use the safe area. The output is responsible for a compact layout.
  out.arrangement = ClockArrangement::Custom;
  out.time = layout.safe;
  out.playArea = layout.safe;
  out.date = withDate ? Rect{0, static_cast<int16_t>(screen.height - dateH), screen.width, dateH}
                      : Rect{0, 0, 0, 0};
  out.textSize = 1;
  return out;
}

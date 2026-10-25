#include "WorldTime.hpp"

#include <string.h>

namespace {

bool isDigitChar(char c) { return c >= '0' && c <= '9'; }
bool isAlphaChar(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

const char* parseName(const char* p) {
  if (*p == '<') {
    ++p;
    const char* begin = p;
    while (isAlphaChar(*p) || isDigitChar(*p) || *p == '+' || *p == '-') ++p;
    if (*p != '>' || p - begin < 3) return nullptr;
    return p + 1;
  }
  const char* begin = p;
  while (isAlphaChar(*p)) ++p;
  return (p - begin) >= 3 ? p : nullptr;
}

const char* parseClock(const char* p, int32_t maxHours, int32_t* out) {
  int32_t sign = 1;
  if (*p == '+' || *p == '-') {
    if (*p == '-') sign = -1;
    ++p;
  }
  if (!isDigitChar(*p)) return nullptr;
  int32_t hours = 0;
  for (int digits = 0; isDigitChar(*p); ++digits) {
    if (digits == 3) return nullptr;
    hours = hours * 10 + (*p++ - '0');
  }
  if (hours > maxHours) return nullptr;
  int32_t value = hours * 3600;
  for (int part = 0; part < 2 && *p == ':'; ++part) {
    if (!isDigitChar(p[1]) || !isDigitChar(p[2])) return nullptr;
    const int32_t minutes = (p[1] - '0') * 10 + (p[2] - '0');
    if (minutes > 59) return nullptr;
    value += part == 0 ? minutes * 60 : minutes;
    p += 3;
  }
  *out = sign * value;
  return p;
}

const char* parseNumber(const char* p, int low, int high, int* out) {
  if (!isDigitChar(*p)) return nullptr;
  int value = 0;
  for (int digits = 0; isDigitChar(*p); ++digits) {
    if (digits == 3) return nullptr;
    value = value * 10 + (*p++ - '0');
  }
  if (value < low || value > high) return nullptr;
  *out = value;
  return p;
}

const char* parseRule(const char* p, PosixTzRule* rule) {
  memset(rule, 0, sizeof(*rule));
  if (*p != 'M') return nullptr;
  int month = 0;
  int week = 0;
  int wday = 0;
  p = parseNumber(p + 1, 1, 12, &month);
  if (!p || *p != '.') return nullptr;
  p = parseNumber(p + 1, 1, 5, &week);
  if (!p || *p != '.') return nullptr;
  p = parseNumber(p + 1, 0, 6, &wday);
  if (!p) return nullptr;
  rule->month = static_cast<uint8_t>(month);
  rule->week = static_cast<uint8_t>(week);
  rule->wday = static_cast<uint8_t>(wday);
  rule->secs = 2 * 3600;
  if (*p == '/') {
    const char* next = parseClock(p + 1, 167, &rule->secs);
    if (!next) return nullptr;
    p = next;
  }
  return p;
}

int64_t daysFromCivil(int64_t year, unsigned month, unsigned day) {
  year -= month <= 2 ? 1 : 0;
  const int64_t era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153u * (month + (month > 2 ? -3 : 9)) + 2u) / 5u + day - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

void civilFromDays(int64_t days, int64_t* year, unsigned* month, unsigned* day) {
  days += 719468;
  const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(days - era * 146097);
  const unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
  int64_t y = static_cast<int64_t>(yoe) + era * 400;
  const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
  const unsigned mp = (5u * doy + 2u) / 153u;
  const unsigned d = doy - (153u * mp + 2u) / 5u + 1u;
  const unsigned m = mp + (mp < 10 ? 3u : -9u);
  y += m <= 2 ? 1 : 0;
  *year = y;
  *month = m;
  *day = d;
}

unsigned daysInMonth(int64_t year, unsigned month) {
  static const uint8_t kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2) {
    const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    return leap ? 29u : 28u;
  }
  return kDays[month - 1];
}

int64_t ruleDay(int64_t year, const PosixTzRule& rule) {
  const int64_t firstDays = daysFromCivil(year, rule.month, 1);
  const int firstWeekday = static_cast<int>((firstDays + 4) % 7 + 7) % 7;
  int day = 1 + ((rule.wday - firstWeekday + 7) % 7) + (rule.week - 1) * 7;
  const int last = static_cast<int>(daysInMonth(year, rule.month));
  if (day > last) day -= 7;
  if (day < 1) day = 1;
  return daysFromCivil(year, rule.month, static_cast<unsigned>(day));
}

}  // namespace

bool posixTzParse(const char* text, PosixTz* out) {
  if (!text || !out) return false;
  PosixTz result;
  result.stdOff = 0;
  result.dstOff = 0;
  result.hasDst = false;
  memset(&result.start, 0, sizeof(result.start));
  memset(&result.end, 0, sizeof(result.end));

  const char* p = parseName(text);
  if (!p) return false;

  int32_t west = 0;
  p = parseClock(p, 24, &west);
  if (!p) return false;
  result.stdOff = -west;

  if (*p) {
    const char* afterName = parseName(p);
    if (!afterName) return false;
    p = afterName;
    if (*p && *p != ',') {
      int32_t dstWest = 0;
      const char* afterOffset = parseClock(p, 24, &dstWest);
      if (!afterOffset) return false;
      result.dstOff = -dstWest;
      p = afterOffset;
    } else {
      result.dstOff = result.stdOff + 3600;
    }
    result.hasDst = true;
    if (*p != ',') return false;
    p = parseRule(p + 1, &result.start);
    if (!p || *p != ',') return false;
    p = parseRule(p + 1, &result.end);
    if (!p || *p) return false;
  }

  *out = result;
  return true;
}

int32_t posixTzOffset(const PosixTz& tz, int64_t utcSeconds) {
  if (!tz.hasDst) return tz.stdOff;

  const int64_t days = utcSeconds >= 0 ? utcSeconds / 86400 : (utcSeconds - 86399) / 86400;
  int64_t year = 1970;
  unsigned month = 1;
  unsigned day = 1;
  civilFromDays(days, &year, &month, &day);

  const int64_t startUtc = ruleDay(year, tz.start) * 86400 + tz.start.secs - tz.stdOff;
  const int64_t endUtc = ruleDay(year, tz.end) * 86400 + tz.end.secs - tz.dstOff;

  if (startUtc <= endUtc) {
    return (utcSeconds >= startUtc && utcSeconds < endUtc) ? tz.dstOff : tz.stdOff;
  }
  return (utcSeconds >= startUtc || utcSeconds < endUtc) ? tz.dstOff : tz.stdOff;
}

#include "QuoteFormat.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

int8_t resolveDecimals(float price, int8_t decimals) {
  if (decimals >= 0) return decimals > 8 ? 8 : decimals;
  const float magnitude = fabsf(price);
  if (magnitude >= 10000.0f) return 0;
  if (magnitude >= 1.0f) return 2;
  if (magnitude >= 0.01f) return 4;
  return 6;
}

// Insert thousands separators into the integer part, keeping any sign.
void groupThousands(const char* input, char* output, size_t capacity) {
  if (capacity == 0) return;
  const size_t length = strlen(input);
  if (length + (length / 3) + 1 > capacity) {
    strncpy(output, input, capacity - 1);
    output[capacity - 1] = '\0';
    return;
  }

  size_t start = 0;
  if (input[0] == '-' || input[0] == '+') {
    output[0] = input[0];
    start = 1;
  }
  const char* dot = strchr(input, '.');
  const size_t integerEnd = dot ? static_cast<size_t>(dot - input) : length;
  const size_t integerLength = integerEnd > start ? integerEnd - start : 0;

  size_t cursor = start;
  for (size_t i = 0; i < integerLength; ++i) {
    if (i > 0 && (integerLength - i) % 3 == 0) output[cursor++] = ',';
    output[cursor++] = input[start + i];
  }
  if (dot) {
    const size_t remaining = length - integerEnd;
    if (cursor + remaining >= capacity) return;
    memcpy(output + cursor, dot, remaining);
    cursor += remaining;
  }
  output[cursor] = '\0';
}

}  // namespace

const char* quoteSign(float value) {
  if (value > 0.0f) return "+";
  if (value < 0.0f) return "-";
  return "";
}

void formatQuotePrice(char* out, size_t capacity, float price, int8_t decimals, bool thousands) {
  if (!out || capacity == 0) return;
  const int8_t places = resolveDecimals(price, decimals);
  char raw[40];
  snprintf(raw, sizeof(raw), "%.*f", static_cast<int>(places), static_cast<double>(price));
  if (thousands) {
    groupThousands(raw, out, capacity);
  } else {
    strncpy(out, raw, capacity - 1);
    out[capacity - 1] = '\0';
  }
}

void formatQuotePercent(char* out, size_t capacity, float percent) {
  if (!out || capacity == 0) return;
  if (fabsf(percent) < 0.005f) {
    snprintf(out, capacity, "0.00%%");
    return;
  }
  snprintf(out, capacity, "%s%.2f%%", quoteSign(percent), static_cast<double>(fabsf(percent)));
}

void formatQuoteChange(char* out, size_t capacity, float change, float price, int8_t decimals) {
  if (!out || capacity == 0) return;
  const int8_t places = resolveDecimals(price, decimals);
  if (fabsf(change) < 0.5f * powf(10.0f, -static_cast<float>(places))) {
    snprintf(out, capacity, "%.*f", static_cast<int>(places), 0.0);
    return;
  }
  char magnitude[40];
  snprintf(magnitude, sizeof(magnitude), "%.*f", static_cast<int>(places),
           static_cast<double>(fabsf(change)));
  char grouped[44];
  groupThousands(magnitude, grouped, sizeof(grouped));
  snprintf(out, capacity, "%s%s", quoteSign(change), grouped);
}

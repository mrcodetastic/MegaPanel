#pragma once

#include <stddef.h>
#include <stdint.h>

// Formatting shared by the asset display and the ticker so both present the
// same numbers identically.
//
// `decimals < 0` selects automatically: 0 decimals at 10 000 and above, 2 at 1
// and above, 4 at 0.01 and above, otherwise 6. This keeps "64,321" and
// "0.000412" both readable on a 64 px panel.
void formatQuotePrice(char* out, size_t capacity, float price, int8_t decimals, bool thousands);

// "+2.35%", "-0.45%" or "0.00%".
void formatQuotePercent(char* out, size_t capacity, float percent);

// "+123.45" / "-12.00" / "0.00", using the same adaptive decimals as the price.
void formatQuoteChange(char* out, size_t capacity, float change, float price, int8_t decimals);

// "-" for a negative value, "+" for a positive one, "" for zero.
const char* quoteSign(float value);

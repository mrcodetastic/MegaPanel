#pragma once

#include <stdint.h>

#include "core/PixelCanvas.hpp"
#include "services/WeatherService.hpp"

// Draws a WMO condition glyph inside a size x size box (8, 16, 24 or 32 px).
// The shapes are procedural so they scale without bitmap tables, and the
// rain/snow/thunder details animate from nowMs.
void drawWeatherIcon(PixelCanvas& canvas,
                     int16_t x,
                     int16_t y,
                     uint8_t size,
                     WeatherIcon icon,
                     CRGB color,
                     uint32_t nowMs);

const char* weatherShortLabel(WeatherIcon icon);

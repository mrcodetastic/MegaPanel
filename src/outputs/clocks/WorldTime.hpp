#pragma once

#include <stdint.h>

// Minimal POSIX TZ evaluator for the world clock. It supports the shape every
// zone in WorldCities uses:
//
//   std offset [dst [offset] , Mmonth.week.wday[/time] , Mmonth.week.wday[/time] ]
//
// Offsets are written west-positive in the string, as POSIX does, and exposed
// here as seconds EAST of UTC. A dst name without an explicit offset is one
// hour ahead of standard time. Rules must use the M form; the J and D forms
// are rejected so a bad string can never silently show the wrong time.
struct PosixTzRule {
  uint8_t month;  // 1..12
  uint8_t week;   // 1..5, 5 = last
  uint8_t wday;   // 0 = Sunday
  int32_t secs;   // local time of day of the change
};

struct PosixTz {
  int32_t stdOff;  // seconds east of UTC
  int32_t dstOff;
  bool hasDst;
  PosixTzRule start;
  PosixTzRule end;
};

bool posixTzParse(const char* text, PosixTz* out);

// Seconds east of UTC in force at the given UTC instant.
int32_t posixTzOffset(const PosixTz& tz, int64_t utcSeconds);

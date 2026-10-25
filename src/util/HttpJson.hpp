#pragma once

#include <Arduino.h>

// Performs a GET and returns the body in `payload`. Handles http and https
// (https without certificate validation, as no CA bundle is vendored), caps
// the response size, and reports a short reason in `error` on failure.
bool httpGetJson(const char* url, String& payload, char* error, size_t errorLength);

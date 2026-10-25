#pragma once

#include <ArduinoJson.h>

// Resolves dotted paths such as "data.0.price" or "bitcoin.usd" against a JSON
// document. Objects are traversed by key, arrays by numeric index. This is the
// mapping syntax exposed by the web UI for data and news sources.
bool resolveJsonPath(JsonVariantConst root, const char* path, JsonVariantConst& out);

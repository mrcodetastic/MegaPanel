#include "JsonPath.hpp"

#include <cstdlib>
#include <cstring>

bool resolveJsonPath(JsonVariantConst root, const char* path, JsonVariantConst& out) {
  if (!path || !*path) return false;
  JsonVariantConst current = root;
  const char* cursor = path;
  char key[32];

  while (*cursor) {
    const char* dot = strchr(cursor, '.');
    const size_t length = dot ? static_cast<size_t>(dot - cursor) : strlen(cursor);
    if (length == 0 || length >= sizeof(key)) return false;
    memcpy(key, cursor, length);
    key[length] = '\0';

    if (current.is<JsonArrayConst>()) {
      char* end = nullptr;
      const long index = strtol(key, &end, 10);
      if (!end || *end != '\0' || index < 0) return false;
      JsonArrayConst array = current.as<JsonArrayConst>();
      if (index >= static_cast<long>(array.size())) return false;
      current = array[static_cast<size_t>(index)];
    } else {
      current = current[key];
    }

    if (current.isNull()) return false;
    if (!dot) break;
    cursor = dot + 1;
  }

  out = current;
  return true;
}

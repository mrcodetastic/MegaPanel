#pragma once

#include <ArduinoJson.h>
#include <stddef.h>
#include "core/OutputMeta.hpp"

class IOutput;

struct OutputDescriptor {
  IOutput* instance;
  const OutputMeta& (*meta)();
  void (*schema)(JsonObject&);
};

// Static catalog of pre-instantiated output objects. It never constructs an
// output at runtime.
class OutputRegistry {
 public:
  static const OutputRegistry& instance();

  IOutput* find(const char* id) const;
  size_t count() const;
  const OutputDescriptor& at(size_t index) const;

 private:
  OutputRegistry() = default;
};

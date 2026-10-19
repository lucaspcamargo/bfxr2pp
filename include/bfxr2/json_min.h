#pragma once

#include "bfxr2/param_reader.h"
#include "bfxr2/params.h"

#include <cstdint>
#include <string>

namespace bfxr {

// Built-in JSON parameter reader implementing IParamReader
class JsonParamReader : public IParamReader {
 public:
  bool parse(const std::string& input, ParamMap& out, std::string* err = nullptr) override;
};

// Convenience free function using the built-in JsonParamReader
bool parse_params_json(const std::string& text, ParamMap& out, std::string* err = nullptr);

}  // namespace bfxr

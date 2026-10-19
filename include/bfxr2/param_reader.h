#pragma once

#include "bfxr2/params.h"

#include <istream>
#include <memory>
#include <string>

namespace bfxr {

// Abstract interface for loading sound parameters from various formats/sources
class IParamReader {
 public:
  virtual ~IParamReader() = default;

  // Parse parameters from a string buffer
  virtual bool parse(const std::string& input, ParamMap& out, std::string* err = nullptr) = 0;

  // Parse parameters from an input stream
  virtual bool parse(std::istream& input, ParamMap& out, std::string* err = nullptr) {
    std::string content((std::istreambuf_iterator<char>(input)),
                        std::istreambuf_iterator<char>());
    return parse(content, out, err);
  }
};

}  // namespace bfxr

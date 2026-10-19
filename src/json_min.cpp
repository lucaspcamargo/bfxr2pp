#include "bfxr2/json_min.h"

#include <cctype>
#include <cstdlib>

namespace bfxr {
namespace {

void skip_ws(const std::string& s, size_t& i) {
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

bool match(const std::string& s, size_t& i, char c) {
  skip_ws(s, i);
  if (i < s.size() && s[i] == c) {
    ++i;
    return true;
  }
  return false;
}

bool parse_string(const std::string& s, size_t& i, std::string& out) {
  skip_ws(s, i);
  if (i >= s.size() || s[i] != '"') return false;
  ++i;
  out.clear();
  while (i < s.size()) {
    char c = s[i++];
    if (c == '"') return true;
    if (c == '\\' && i < s.size()) {
      char e = s[i++];
      if (e == '"' || e == '\\' || e == '/')
        out.push_back(e);
      else if (e == 'n')
        out.push_back('\n');
      else if (e == 't')
        out.push_back('\t');
      else if (e == 'r')
        out.push_back('\r');
      else
        out.push_back(e);
    } else {
      out.push_back(c);
    }
  }
  return false;
}

bool parse_number(const std::string& s, size_t& i, double& out) {
  skip_ws(s, i);
  size_t start = i;
  if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
  bool any = false;
  while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
    ++i;
    any = true;
  }
  if (i < s.size() && s[i] == '.') {
    ++i;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
      ++i;
      any = true;
    }
  }
  if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
    ++i;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
      ++i;
      any = true;
    }
  }
  if (!any) return false;
  char* end = nullptr;
  out = std::strtod(s.c_str() + start, &end);
  return end != s.c_str() + start;
}

// Skip any JSON value (used for unknown keys).
bool skip_value(const std::string& s, size_t& i);

bool skip_object(const std::string& s, size_t& i) {
  if (!match(s, i, '{')) return false;
  skip_ws(s, i);
  if (match(s, i, '}')) return true;
  for (;;) {
    std::string key;
    if (!parse_string(s, i, key)) return false;
    if (!match(s, i, ':')) return false;
    if (!skip_value(s, i)) return false;
    skip_ws(s, i);
    if (match(s, i, '}')) return true;
    if (!match(s, i, ',')) return false;
  }
}

bool skip_array(const std::string& s, size_t& i) {
  if (!match(s, i, '[')) return false;
  skip_ws(s, i);
  if (match(s, i, ']')) return true;
  for (;;) {
    if (!skip_value(s, i)) return false;
    skip_ws(s, i);
    if (match(s, i, ']')) return true;
    if (!match(s, i, ',')) return false;
  }
}

bool skip_value(const std::string& s, size_t& i) {
  skip_ws(s, i);
  if (i >= s.size()) return false;
  if (s[i] == '{') return skip_object(s, i);
  if (s[i] == '[') return skip_array(s, i);
  if (s[i] == '"') {
    std::string tmp;
    return parse_string(s, i, tmp);
  }
  if (s.compare(i, 4, "true") == 0) {
    i += 4;
    return true;
  }
  if (s.compare(i, 5, "false") == 0) {
    i += 5;
    return true;
  }
  if (s.compare(i, 4, "null") == 0) {
    i += 4;
    return true;
  }
  double d;
  return parse_number(s, i, d);
}

bool parse_param_object(const std::string& s, size_t& i, ParamMap& out) {
  if (!match(s, i, '{')) return false;
  skip_ws(s, i);
  if (match(s, i, '}')) return true;
  for (;;) {
    std::string key;
    if (!parse_string(s, i, key)) return false;
    if (!match(s, i, ':')) return false;
    skip_ws(s, i);
    double num = 0;
    if (parse_number(s, i, num)) {
      out[key] = num;
    } else if (!skip_value(s, i)) {
      return false;
    }
    skip_ws(s, i);
    if (match(s, i, '}')) return true;
    if (!match(s, i, ',')) return false;
  }
}

}  // namespace

bool parse_params_json(const std::string& text, ParamMap& out, std::string* err) {
  out.clear();
  size_t i = 0;
  skip_ws(text, i);
  if (i >= text.size() || text[i] != '{') {
    if (err) *err = "expected JSON object";
    return false;
  }

  // First pass: look for a nested "params" object.
  size_t j = i;
  ParamMap top;
  ParamMap nested;
  bool has_nested = false;

  if (!match(text, j, '{')) return false;
  skip_ws(text, j);
  if (match(text, j, '}')) {
    out = top;
    return true;
  }
  for (;;) {
    std::string key;
    if (!parse_string(text, j, key)) {
      if (err) *err = "bad key";
      return false;
    }
    if (!match(text, j, ':')) {
      if (err) *err = "expected ':'";
      return false;
    }
    skip_ws(text, j);
    if (key == "params" && j < text.size() && text[j] == '{') {
      if (!parse_param_object(text, j, nested)) {
        if (err) *err = "bad params object";
        return false;
      }
      has_nested = true;
    } else {
      double num = 0;
      size_t before = j;
      if (parse_number(text, j, num)) {
        top[key] = num;
      } else {
        j = before;
        if (!skip_value(text, j)) {
          if (err) *err = "bad value";
          return false;
        }
      }
    }
    skip_ws(text, j);
    if (match(text, j, '}')) break;
    if (!match(text, j, ',')) {
      if (err) *err = "expected ',' or '}'";
      return false;
    }
  }

  out = has_nested ? nested : top;
  return true;
}

bool JsonParamReader::parse(const std::string& input, ParamMap& out, std::string* err) {
  return parse_params_json(input, out, err);
}

}  // namespace bfxr

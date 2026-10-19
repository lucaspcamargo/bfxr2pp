#include "bfxr2/bfxr_dsp.h"
#include "bfxr2/json_min.h"
#include "bfxr2/params.h"
#include "bfxr2/presets.h"
#include "bfxr2/wav.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

struct Args {
  std::string in_path;
  std::string out_path;
  std::string preset;
  uint32_t seed = 1;
  bool dump_info = false;
  bool list_presets = false;
};

void usage() {
  std::cerr << "Usage:\n"
            << "  bfxr_render --in sound.bfxr --out out.wav [--seed N]\n"
            << "  bfxr_render --preset <name> --out out.wav [--seed N]\n"
            << "  bfxr_render --list-presets\n"
            << "  bfxr_render --dump-info\n";
}

Args parse_args(int argc, char** argv) {
  Args args;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--in") {
      if (++i >= argc) throw std::runtime_error("missing --in value");
      args.in_path = argv[i];
    } else if (a == "--out") {
      if (++i >= argc) throw std::runtime_error("missing --out value");
      args.out_path = argv[i];
    } else if (a == "--preset") {
      if (++i >= argc) throw std::runtime_error("missing --preset value");
      args.preset = argv[i];
    } else if (a == "--seed") {
      if (++i >= argc) throw std::runtime_error("missing --seed value");
      args.seed = static_cast<uint32_t>(std::stoul(argv[i]));
    } else if (a == "--dump-info") {
      args.dump_info = true;
    } else if (a == "--list-presets") {
      args.list_presets = true;
    } else {
      throw std::runtime_error("Unknown argument: " + a);
    }
  }
  return args;
}

std::string read_file(const std::string& path) {
  std::ifstream f(path);
  if (!f) throw std::runtime_error("cannot open " + path);
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

}  // namespace

int main(int argc, char** argv) {
  try {
    Args args = parse_args(argc, argv);
    if (args.dump_info) {
      std::cout << bfxr::dump_info_json();
      return 0;
    }

    if (args.list_presets) {
      std::cout << "Available presets:\n";
      for (const auto& meta : bfxr::get_preset_list()) {
        std::cout << "  - " << meta.short_name << " (" << meta.name << "): " << meta.description << "\n";
      }
      return 0;
    }

    if (args.out_path.empty() || (args.in_path.empty() && args.preset.empty())) {
      usage();
      return 2;
    }

    bfxr::ParamMap params;
    bfxr::Mulberry32 rng(args.seed);

    if (!args.preset.empty()) {
      std::string target = args.preset;
      std::transform(target.begin(), target.end(), target.begin(), ::tolower);
      bool found = false;

      for (const auto& meta : bfxr::get_preset_list()) {
        std::string sname = meta.short_name;
        std::string fname = meta.name;
        std::transform(sname.begin(), sname.end(), sname.begin(), ::tolower);
        std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);

        if (target == sname || target == fname) {
          params = bfxr::generate_preset(meta.category, rng);
          found = true;
          break;
        }
      }

      if (!found) {
        std::cerr << "Unknown preset: '" << args.preset << "'. Use --list-presets to view options.\n";
        return 1;
      }
    } else {
      bfxr::JsonParamReader reader;
      std::string err;
      if (!reader.parse(read_file(args.in_path), params, &err)) {
        std::cerr << "Failed to parse input: " << err << "\n";
        return 1;
      }
    }

    auto result = bfxr::render_bfxr(params, rng);
    if (!result.ok || result.samples.empty()) {
      std::cerr << "Render failed (DSP produced no buffer)\n";
      return 1;
    }

    std::string wav =
        bfxr::encode_wav16(result.samples.data(), result.samples.size(), 44100);
    if (!bfxr::write_file(args.out_path, wav)) {
      std::cerr << "Failed to write " << args.out_path << "\n";
      return 1;
    }
    std::cerr << "Wrote " << args.out_path << ": " << result.samples.size() << " samples ("
              << (result.samples.size() / 44100.0) << "s)\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
    return 2;
  }
}

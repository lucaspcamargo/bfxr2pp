#include "bfxr2/bfxr_dsp.h"
#include "bfxr2/presets.h"
#include "bfxr2/prng.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <thread>

namespace {

void print_usage() {
  std::cout << "Usage: bfxr2pp_sdl_play [preset_name] [--seed N]\n\n"
            << "Available presets:\n";
  for (const auto& meta : bfxr::get_preset_list()) {
    std::cout << "  " << meta.short_name << " (" << meta.name << "): " << meta.description << "\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::string preset_name = "shoot";
  uint32_t seed = 0;
  bool seed_specified = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      print_usage();
      return 0;
    } else if (arg == "--seed") {
      if (++i >= argc) {
        std::cerr << "Error: --seed requires a numeric value\n";
        return 1;
      }
      seed = static_cast<uint32_t>(std::stoul(argv[i]));
      seed_specified = true;
    } else if (arg.rfind("-", 0) == 0) {
      std::cerr << "Unknown option: " << arg << "\n";
      print_usage();
      return 1;
    } else {
      preset_name = arg;
    }
  }

  if (!seed_specified) {
    std::random_device rd;
    seed = rd();
  }

  std::string target = preset_name;
  std::transform(target.begin(), target.end(), target.begin(), ::tolower);

  const bfxr::PresetMeta* chosen_meta = nullptr;
  for (const auto& meta : bfxr::get_preset_list()) {
    std::string sname = meta.short_name;
    std::string fname = meta.name;
    std::transform(sname.begin(), sname.end(), sname.begin(), ::tolower);
    std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);

    if (target == sname || target == fname) {
      chosen_meta = &meta;
      break;
    }
  }

  if (!chosen_meta) {
    std::cerr << "Unknown preset: '" << preset_name << "'\n\n";
    print_usage();
    return 1;
  }

  std::cout << "Generating sound preset '" << chosen_meta->name << "' with seed " << seed << "...\n";

  bfxr::Mulberry32 rng(seed);
  bfxr::ParamMap params = bfxr::generate_preset(chosen_meta->category, rng);
  bfxr::RenderResult render = bfxr::render_bfxr(params, rng);

  if (!render.ok || render.samples.empty()) {
    std::cerr << "Render failed (no audio buffer generated)\n";
    return 1;
  }

  std::cout << "Rendered " << render.samples.size() << " samples (~"
            << (render.samples.size() / 44100.0) << "s).\n";

  if (!SDL_Init(SDL_INIT_AUDIO)) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
    return 1;
  }

  SDL_AudioSpec spec;
  SDL_zero(spec);
  spec.format = SDL_AUDIO_F32;
  spec.channels = 1;
  spec.freq = 44100;

  SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(
      SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
      &spec,
      nullptr,
      nullptr
  );

  if (!stream) {
    std::cerr << "Failed to open SDL audio stream: " << SDL_GetError() << "\n";
    SDL_Quit();
    return 1;
  }

  if (!SDL_PutAudioStreamData(stream, render.samples.data(), static_cast<int>(render.samples.size() * sizeof(float)))) {
    std::cerr << "Failed to queue audio data: " << SDL_GetError() << "\n";
    SDL_DestroyAudioStream(stream);
    SDL_Quit();
    return 1;
  }

  if (!SDL_FlushAudioStream(stream)) {
    std::cerr << "Failed to flush audio stream: " << SDL_GetError() << "\n";
  }

  if (!SDL_ResumeAudioStreamDevice(stream)) {
    std::cerr << "Failed to start audio playback: " << SDL_GetError() << "\n";
    SDL_DestroyAudioStream(stream);
    SDL_Quit();
    return 1;
  }

  std::cout << "Playing back audio...\n";

  // Wait until playback finishes (stream buffer clears)
  while (SDL_GetAudioStreamQueued(stream) > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Brief grace period for hardware buffer to drain
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  SDL_DestroyAudioStream(stream);
  SDL_Quit();

  std::cout << "Playback completed.\n";
  return 0;
}

#include "bfxr2/bfxr_dsp.h"
#include "bfxr2/params.h"
#include "bfxr2/presets.h"
#include "bfxr2/prng.h"
#include "bfxr2/wav.h"

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace {

// Finds a system TrueType font (.ttf) without any link dependencies.
std::string find_system_ui_font() {
#if defined(_WIN32)
  const char* windir = getenv("WINDIR");
  std::string font_dir = windir ? (std::string(windir) + "\\Fonts\\") : "C:\\Windows\\Fonts\\";
  const char* candidates[] = {
      "segoeui.ttf", "arial.ttf", "tahoma.ttf", "calibri.ttf"
  };
  for (const char* font : candidates) {
    std::string path = font_dir + font;
    std::ifstream f(path, std::ios::binary);
    if (f.good()) return path;
  }
#elif defined(__linux__)
  // 1. Try querying fontconfig via `fc-match` (standard system utility)
  struct PcloseDeleter {
    void operator()(FILE* f) const { if (f) pclose(f); }
  };
  std::unique_ptr<FILE, PcloseDeleter> pipe(
      popen("fc-match --format=\"%{file}\" :fontformat=TrueType 2>/dev/null", "r")
  );
  if (pipe) {
    std::array<char, 512> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      result += buffer.data();
    }
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
      result.pop_back();
    }
    if (!result.empty()) {
      std::ifstream f(result, std::ios::binary);
      if (f.good()) return result;
    }
  }

  // 2. Common fallback font locations on Linux distributions
  const char* fallbacks[] = {
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
      "/usr/share/fonts/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
      "/usr/share/fonts/noto/NotoSans-Regular.ttf",
      "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
      "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
      "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
      "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
  };
  for (const char* path : fallbacks) {
    std::ifstream f(path, std::ios::binary);
    if (f.good()) return path;
  }
#endif
  return "";
}

// Detects whether the system prefers a dark theme without any link dependencies.
bool detect_system_dark_theme() {
#if defined(_WIN32)
  // Check Windows AppsUseLightTheme registry setting via reg.exe
  struct PcloseDeleter {
    void operator()(FILE* f) const { if (f) pclose(f); }
  };
  std::unique_ptr<FILE, PcloseDeleter> pipe(
      popen("reg query \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize\" /v AppsUseLightTheme 2>nul", "r")
  );
  if (pipe) {
    std::array<char, 256> buffer;
    std::string out;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      out += buffer.data();
    }
    // 0x0 indicates dark theme, 0x1 indicates light theme
    if (out.find("0x0") != std::string::npos) return true;
    if (out.find("0x1") != std::string::npos) return false;
  }
#elif defined(__linux__)
  struct PcloseDeleter {
    void operator()(FILE* f) const { if (f) pclose(f); }
  };
  // 1. Try querying Freedesktop portal / GNOME color-scheme via gsettings
  std::unique_ptr<FILE, PcloseDeleter> pipe(
      popen("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null", "r")
  );
  if (pipe) {
    std::array<char, 128> buffer;
    std::string out;
    if (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      out = buffer.data();
    }
    if (out.find("dark") != std::string::npos || out.find("Dark") != std::string::npos) {
      return true;
    }
    if (out.find("light") != std::string::npos || out.find("Light") != std::string::npos) {
      return false;
    }
  }

  // 2. Fallback: inspect gtk-theme name
  std::unique_ptr<FILE, PcloseDeleter> pipe_theme(
      popen("gsettings get org.gnome.desktop.interface gtk-theme 2>/dev/null", "r")
  );
  if (pipe_theme) {
    std::array<char, 128> buffer;
    std::string out;
    if (fgets(buffer.data(), buffer.size(), pipe_theme.get()) != nullptr) {
      out = buffer.data();
    }
    if (out.find("dark") != std::string::npos || out.find("Dark") != std::string::npos) {
      return true;
    }
  }
#endif
  // Default to dark theme for audio workstations / DAWs if uncertain
  return true;
}

struct AppState {
  bfxr::ParamMap params;
  bfxr::ParamInfo param_info;
  uint32_t seed = 42;
  bool dirty = true;

  std::vector<float> audio_buffer;
  std::vector<float> waveform_display;

  SDL_AudioStream* audio_stream = nullptr;

  char export_filename[128] = "sound.wav";
  std::string export_status;
  std::chrono::steady_clock::time_point export_status_time;
};

struct FullAppData {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  AppState app;
  bool is_dark_theme = true;
};

void resynthesize(AppState& state) {
  bfxr::Mulberry32 rng(state.seed);
  bfxr::RenderResult res = bfxr::render_bfxr(state.params, rng);
  if (res.ok && !res.samples.empty()) {
    state.audio_buffer = std::move(res.samples);
  } else {
    state.audio_buffer.clear();
  }

  // Downsample audio buffer to 512 points for display
  state.waveform_display.clear();
  if (!state.audio_buffer.empty()) {
    constexpr size_t kPoints = 512;
    state.waveform_display.resize(kPoints);
    double step = static_cast<double>(state.audio_buffer.size()) / kPoints;
    for (size_t i = 0; i < kPoints; ++i) {
      size_t idx = static_cast<size_t>(i * step);
      if (idx >= state.audio_buffer.size()) idx = state.audio_buffer.size() - 1;
      state.waveform_display[i] = state.audio_buffer[idx];
    }
  }
  state.dirty = false;
}

void play_audio(AppState& state) {
  if (!state.audio_stream || state.audio_buffer.empty()) return;

  SDL_ClearAudioStream(state.audio_stream);
  SDL_PutAudioStreamData(
      state.audio_stream,
      state.audio_buffer.data(),
      static_cast<int>(state.audio_buffer.size() * sizeof(float))
  );
  SDL_FlushAudioStream(state.audio_stream);
  SDL_ResumeAudioStreamDevice(state.audio_stream);
}

}  // namespace


SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    std::cerr << "SDL_Init error: " << SDL_GetError() << "\n";
    return SDL_APP_FAILURE;
  }

  SDL_Window* window = SDL_CreateWindow(
      "bfxr2pp - Sound Synthesizer & Waveform Preview",
      1100, 760,
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
  );
  if (!window) {
    std::cerr << "SDL_CreateWindow error: " << SDL_GetError() << "\n";
    SDL_Quit();
    return SDL_APP_FAILURE;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
  if (!renderer) {
    std::cerr << "SDL_CreateRenderer error: " << SDL_GetError() << "\n";
    SDL_DestroyWindow(window);
    SDL_Quit();
    return SDL_APP_FAILURE;
  }

  // Audio setup
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

  auto* data = new FullAppData();
  data->window = window;
  data->renderer = renderer;
  data->app.audio_stream = stream;
  data->app.param_info = bfxr::make_param_info();
  data->app.params = bfxr::default_params();
  resynthesize(data->app);

  // Setup ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;

  data->is_dark_theme = detect_system_dark_theme();
  if (data->is_dark_theme) {
    ImGui::StyleColorsDark();
  } else {
    ImGui::StyleColorsLight();
  }

  std::string font_path = find_system_ui_font();
  float pixel_density = SDL_GetWindowPixelDensity(window);
  if (pixel_density <= 0.0f) pixel_density = 1.0f;

  ImFontConfig font_cfg;
  font_cfg.RasterizerDensity = pixel_density;

  if (!font_path.empty()) {
    io.Fonts->AddFontFromFileTTF(font_path.c_str(), 16.0f, &font_cfg);
    std::cout << "Loaded system font: " << font_path << " (RasterizerDensity: " << pixel_density << ")\n";
  } else {
    io.Fonts->AddFontDefault(&font_cfg);
  }

  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  auto get_window_content_scale = [window]() -> float {
    SDL_DisplayID display_id = SDL_GetDisplayForWindow(window);
    if (display_id == 0) return 1.0f;
    float scale = SDL_GetDisplayContentScale(display_id);
    return scale > 0.0f ? scale : 1.0f;
  };

  float content_scale = get_window_content_scale();
  if (content_scale > 1.0f) {
    ImGui::GetStyle().ScaleAllSizes(content_scale);
    io.FontGlobalScale = content_scale;
  }

  *appstate = data;
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  auto* data = static_cast<FullAppData*>(appstate);
  ImGui_ImplSDL3_ProcessEvent(event);
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  }
  if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
      event->window.windowID == SDL_GetWindowID(data->window)) {
    return SDL_APP_SUCCESS;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  auto* data = static_cast<FullAppData*>(appstate);
  AppState& app = data->app;
  SDL_Window* window = data->window;
  SDL_Renderer* renderer = data->renderer;

  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  int w = 0, h = 0;
  SDL_GetWindowSize(window, &w, &h);
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w), static_cast<float>(h)));
  ImGuiWindowFlags win_flags = ImGuiWindowFlags_NoTitleBar |
                               ImGuiWindowFlags_NoResize |
                               ImGuiWindowFlags_NoMove |
                               ImGuiWindowFlags_NoCollapse;

  ImGui::Begin("bfxr2pp Studio", nullptr, win_flags);

  ImGui::Text("Presets & Generation:");
  ImGui::SameLine();
  if (ImGui::Button("PLAY SOUND (Space)")) {
    play_audio(app);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
    play_audio(app);
  }

  ImGui::Separator();

  for (const auto& meta : bfxr::get_preset_list()) {
    if (ImGui::Button(meta.short_name.c_str())) {
      std::random_device rd;
      app.seed = rd();
      bfxr::Mulberry32 rng(app.seed);
      if (meta.category == bfxr::PresetCategory::Mutate) {
        app.params = bfxr::mutate_params(app.params, rng);
      } else {
        app.params = bfxr::generate_preset(meta.category, rng);
      }
      resynthesize(app);
      play_audio(app);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s: %s (Generates new variation)", meta.name.c_str(), meta.description.c_str());
    }
    ImGui::SameLine();
  }

  if (ImGui::Button("New Seed")) {
    std::random_device rd;
    app.seed = rd();
    resynthesize(app);
    play_audio(app);
  }

  ImGui::SameLine();
  ImGui::Spacing();
  ImGui::SameLine();

  ImGui::SetNextItemWidth(140.0f);
  ImGui::InputText("##ExportFile", app.export_filename, sizeof(app.export_filename));
  ImGui::SameLine();
  if (ImGui::Button("Export WAV")) {
    if (app.audio_buffer.empty()) {
      app.export_status = "Error: empty buffer!";
    } else {
      std::string wav_data = bfxr::encode_wav16(
          app.audio_buffer.data(),
          app.audio_buffer.size(),
          44100
      );
      if (bfxr::write_file(app.export_filename, wav_data)) {
        app.export_status = std::string("Saved ") + app.export_filename;
      } else {
        app.export_status = "Error writing file!";
      }
    }
    app.export_status_time = std::chrono::steady_clock::now();
  }

  if (!app.export_status.empty()) {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - app.export_status_time).count();
    if (elapsed < 4) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", app.export_status.c_str());
    } else {
      app.export_status.clear();
    }
  }

  ImGui::NewLine();
  ImGui::Separator();

  ImGui::Text("Waveform Preview (Duration: %.2f s, Samples: %zu)",
              app.audio_buffer.size() / 44100.0f, app.audio_buffer.size());
  if (!app.waveform_display.empty()) {
    ImGui::PlotLines("##Waveform", app.waveform_display.data(),
                     static_cast<int>(app.waveform_display.size()), 0,
                     nullptr, -1.0f, 1.0f, ImVec2(-1.0f, 130.0f));
  } else {
    ImGui::TextDisabled("No waveform generated.");
  }

  ImGui::Separator();

  ImGui::BeginChild("ParamsRegion", ImVec2(0, 0), true);

  int current_wave = static_cast<int>(app.params["waveType"]);
  const char* wave_names[] = {
      "Square (0)", "Saw (1)", "Sin (2)", "White Noise (3)", "Triangle (4)",
      "Rasp (5)", "Tan (6)", "Whistle (7)", "Breaker (8)", "Bitnoise (9)",
      "FMSyn (10)", "Voice (11)"
  };
  int wave_indices[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  int combo_idx = 0;
  for (int i = 0; i < 12; ++i) {
    if (wave_indices[i] == current_wave) {
      combo_idx = i;
      break;
    }
  }

  ImGui::Text("Waveform Type:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(250.0f);
  if (ImGui::Combo("##WaveType", &combo_idx, wave_names, 12)) {
    app.params["waveType"] = wave_indices[combo_idx];
    app.dirty = true;
  }

  ImGui::Columns(2, "ParamColumns", true);

  size_t count = 0;
  for (const auto& meta : app.param_info.params) {
    if (meta.name == "waveType") continue;

    float val = static_cast<float>(app.params[meta.name]);
    float min_v = static_cast<float>(meta.min_value);
    float max_v = static_cast<float>(meta.max_value);

    ImGui::PushID(meta.name.c_str());
    ImGui::Text("%s", meta.name.c_str());
    ImGui::SameLine(180.0f);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("##val", &val, min_v, max_v, "%.3f")) {
      app.params[meta.name] = static_cast<double>(val);
      app.dirty = true;
    }
    ImGui::PopID();

    count++;
    if (count == (app.param_info.params.size() / 2)) {
      ImGui::NextColumn();
    }
  }

  ImGui::Columns(1);
  ImGui::EndChild();
  ImGui::End();

  if (app.dirty) {
    resynthesize(app);
  }

  ImGui::Render();
  ImGuiIO& io = ImGui::GetIO();
  SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
  if (data->is_dark_theme) {
    SDL_SetRenderDrawColor(renderer, 25, 25, 28, 255);
  } else {
    SDL_SetRenderDrawColor(renderer, 240, 240, 245, 255);
  }
  SDL_RenderClear(renderer);
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
  SDL_RenderPresent(renderer);

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  if (appstate) {
    auto* data = static_cast<FullAppData*>(appstate);

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (data->app.audio_stream) {
      SDL_DestroyAudioStream(data->app.audio_stream);
    }
    if (data->renderer) {
      SDL_DestroyRenderer(data->renderer);
    }
    if (data->window) {
      SDL_DestroyWindow(data->window);
    }
    delete data;
  }
}
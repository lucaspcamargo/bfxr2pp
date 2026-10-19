#include "bfxr2/presets.h"

#include <algorithm>
#include <cmath>

namespace bfxr {
namespace {

// Helpers for RNG operations matching JS Math.random()
inline double rand_double(Mulberry32& rng) {
  return rng.next();
}

inline double rand_range(Mulberry32& rng, double min, double max) {
  return min + rand_double(rng) * (max - min);
}

inline int rand_choice(Mulberry32& rng, const std::vector<int>& choices) {
  size_t idx = static_cast<size_t>(rand_double(rng) * choices.size());
  if (idx >= choices.size()) idx = choices.size() - 1;
  return choices[idx];
}

inline double rand_centered_around_x(Mulberry32& rng, double min, double max, double centre) {
  if (rand_double(rng) < 0.5) {
    double r = rand_double(rng);
    r = r * r;
    return centre + r * (max - centre);
  } else {
    double r = rand_double(rng);
    r = r * r;
    return centre - r * (centre - min);
  }
}

// WaveType indices:
// Triangle:4, Sin:2, Square:0, Saw:1, Breaker:8, Tan:6, Whistle:7, White:3, Voice:11, Bitnoise:9, Rasp:5, FMSyn:10

void rectify_params(ParamMap& p, Mulberry32& rng) {
  double frequency_default = 0.3;
  if (static_cast<int>(p["waveType"]) == 11) {
    frequency_default = 0.22;
  }
  p["frequency_start"] = rand_centered_around_x(rng, 0, 0.6, frequency_default);

  if (p["attackTime"] + p["sustainTime"] + p["decayTime"] < 0.2) {
    p["sustainTime"] = 0.2 + rand_double(rng) * 0.3;
    p["decayTime"] = 0.2 + rand_double(rng) * 0.3;
  }

  double r = rand_double(rng) * rand_double(rng);
  p["sustainPunch"] = r * r;

  if ((p["frequency_start"] > 0.7 && p["frequency_slide"] > 0.2) ||
      (p["frequency_start"] < 0.2 && p["frequency_slide"] < -0.05)) {
    p["frequency_slide"] = -p["frequency_slide"];
  }

  if (p["lpFilterCutoff"] < 0.1 && p["lpFilterCutoffSweep"] < 0) {
    p["lpFilterCutoffSweep"] = -p["lpFilterCutoffSweep"] + 0.2;
  }

  if (static_cast<int>(p["waveType"]) != 0) {
    p["squareDuty"] = 0.0;
    p["dutySweep"] = 0.0;
  } else {
    double duty = p["squareDuty"];
    if (duty > 0.7 && rand_double(rng) < 0.5) {
      p["dutySweep"] = -rand_double(rng) * 0.5;
    }
  }
}

}  // namespace

const std::vector<PresetMeta>& get_preset_list() {
  static const std::vector<PresetMeta> kPresets = {
      {PresetCategory::PickupCoin, "Pickup/Coin", "Pickup",
       "Blips and baleeps."},
      {PresetCategory::LaserShoot, "Laser/Shoot", "Shoot",
       "Pew pew frequency slides."},
      {PresetCategory::Explosion, "Explosion", "Boom",
       "Noisy explosions and impacts."},
      {PresetCategory::Powerup, "Powerup", "PowerUp",
       "Rising pitch glides and sweeps."},
      {PresetCategory::HitHurt, "Hit/Hurt", "Hit",
       "Sudden impacts with steep pitch drops."},
      {PresetCategory::Jump, "Jump", "Jump",
       "Upward frequency spring sound."},
      {PresetCategory::BlipSelect, "Blip/Select", "Blip",
       "UI clicks, blips, and tone cues."},
      {PresetCategory::Randomize, "Randomize", "Random",
       "Full random sound effect generation."},
      {PresetCategory::Mutate, "Mutate", "Mutant",
       "Slight mutation of current sound effect."}
  };
  return kPresets;
}

ParamMap generate_pickup_coin(Mulberry32& rng) {
  ParamMap p = default_params();
  p["frequency_start"] = 0.4 + rand_double(rng) * 0.5;
  p["sustainTime"] = rand_double(rng) * 0.1;
  p["decayTime"] = 0.1 + rand_double(rng) * 0.4;
  p["sustainPunch"] = 0.3 + rand_double(rng) * 0.3;

  if (rand_double(rng) < 0.5) {
    p["pitch_jump_onset_percent"] = 0.5 + rand_double(rng) * 0.2;
    int cnum = static_cast<int>(rand_double(rng) * 7) + 1;
    int cden = static_cast<int>(rand_double(rng) * 7) + cnum + 2;
    p["pitch_jump_amount"] = static_cast<double>(cnum) / static_cast<double>(cden);
  }
  return p;
}

ParamMap generate_laser_shoot(Mulberry32& rng) {
  ParamMap p = default_params();
  p["waveType"] = static_cast<int>(rand_double(rng) * 3);
  if (static_cast<int>(p["waveType"]) == 2 && rand_double(rng) < 0.5) {
    p["waveType"] = static_cast<int>(rand_double(rng) * 2);
  }

  if (rand_double(rng) < 0.33) {
    p["frequency_start"] = 0.1 + rand_double(rng) * 0.5;
    p["min_frequency_relative_to_starting_frequency"] = rand_double(rng) * 0.1;
    p["frequency_slide"] = -0.35 - rand_double(rng) * 0.3;
  } else {
    p["frequency_start"] = 0.5 + rand_double(rng) * 0.5;
    p["min_frequency_relative_to_starting_frequency"] =
        p["frequency_start"] - 0.2 - rand_double(rng) * 0.6;
    if (p["min_frequency_relative_to_starting_frequency"] < 0.2) {
      p["min_frequency_relative_to_starting_frequency"] = 0.2;
    }
    p["frequency_slide"] = -0.15 - rand_double(rng) * 0.2;
  }

  if (p["frequency_start"] < 0.15) {
    p["min_frequency_relative_to_starting_frequency"] = 0;
    p["frequency_slide"] = -0.1 - rand_double(rng) * 0.1;
  }

  if (rand_double(rng) < 0.5) {
    p["squareDuty"] = rand_double(rng) * 0.5;
    p["dutySweep"] = rand_double(rng) * 0.2;
  } else {
    p["squareDuty"] = 0.4 + rand_double(rng) * 0.5;
    p["dutySweep"] = -rand_double(rng) * 0.7;
  }

  p["sustainTime"] = 0.1 + rand_double(rng) * 0.2;
  p["decayTime"] = rand_double(rng) * 0.4;
  if (rand_double(rng) < 0.5) p["sustainPunch"] = rand_double(rng) * 0.3;

  if (rand_double(rng) < 0.33) {
    p["flangerOffset"] = rand_double(rng) * 0.2;
    p["flangerSweep"] = -rand_double(rng) * 0.2;
  }

  if (rand_double(rng) < 0.5) p["hpFilterCutoff"] = rand_double(rng) * 0.3;
  return p;
}

ParamMap generate_explosion(Mulberry32& rng) {
  ParamMap p = default_params();
  p["waveType"] = (rand_double(rng) < 0.5) ? 3 : 9;  // White or Bitnoise

  if (rand_double(rng) < 0.5) {
    p["frequency_start"] = 0.1 + rand_double(rng) * 0.4;
    p["frequency_slide"] = -0.1 + rand_double(rng) * 0.4;
  } else {
    p["frequency_start"] = 0.2 + rand_double(rng) * 0.7;
    p["frequency_slide"] = -0.2 - rand_double(rng) * 0.2;
  }
  p["frequency_start"] = p["frequency_start"] * p["frequency_start"];

  if (rand_double(rng) < 0.2) p["frequency_slide"] = 0.0;
  if (rand_double(rng) < 0.33) p["repeatSpeed"] = 0.3 + rand_double(rng) * 0.5;

  p["sustainTime"] = 0.1 + rand_double(rng) * 0.3;
  p["decayTime"] = rand_double(rng) * 0.5;
  p["sustainPunch"] = 0.2 + rand_double(rng) * 0.6;

  if (rand_double(rng) < 0.5) {
    p["flangerOffset"] = -0.3 + rand_double(rng) * 0.9;
    p["flangerSweep"] = -rand_double(rng) * 0.3;
  }

  if (rand_double(rng) < 0.33) {
    p["pitch_jump_onset_percent"] = 0.6 + rand_double(rng) * 0.3;
    p["pitch_jump_amount"] = 0.8 - rand_double(rng) * 1.6;
  }
  return p;
}

ParamMap generate_powerup(Mulberry32& rng) {
  ParamMap p = default_params();
  if (rand_double(rng) < 0.5) {
    p["waveType"] = 1;  // Saw
  } else {
    p["squareDuty"] = rand_double(rng) * 0.6;
  }

  if (rand_double(rng) < 0.5) {
    p["frequency_start"] = 0.2 + rand_double(rng) * 0.3;
    p["frequency_slide"] = 0.1 + rand_double(rng) * 0.4;
    p["repeatSpeed"] = 0.4 + rand_double(rng) * 0.4;
  } else {
    p["frequency_start"] = 0.2 + rand_double(rng) * 0.3;
    p["frequency_slide"] = 0.05 + rand_double(rng) * 0.2;
    if (rand_double(rng) < 0.5) {
      p["vibratoDepth"] = rand_double(rng) * 0.7;
      p["vibratoSpeed"] = rand_double(rng) * 0.6;
    }
  }

  p["sustainTime"] = rand_double(rng) * 0.4;
  p["decayTime"] = 0.1 + rand_double(rng) * 0.4;
  return p;
}

ParamMap generate_hit_hurt(Mulberry32& rng) {
  ParamMap p = default_params();
  // Choices: White(3), Bitnoise(9), Saw(1), Square(0), Voice(11)
  p["waveType"] = rand_choice(rng, {3, 9, 1, 0, 11});
  if (static_cast<int>(p["waveType"]) == 0) {
    p["squareDuty"] = rand_double(rng) * 0.6;
  }

  p["frequency_start"] = 0.2 + rand_double(rng) * 0.6;
  p["frequency_slide"] = -0.3 - rand_double(rng) * 0.4;

  p["sustainTime"] = rand_double(rng) * 0.1;
  p["decayTime"] = 0.1 + rand_double(rng) * 0.2;

  if (rand_double(rng) < 0.5) {
    p["hpFilterCutoff"] = rand_double(rng) * 0.3;
  }
  return p;
}

ParamMap generate_jump(Mulberry32& rng) {
  ParamMap p = default_params();
  // Choices: Square(0), Saw(1), FMSyn(10)
  p["waveType"] = rand_choice(rng, {0, 1, 10});
  p["squareDuty"] = rand_double(rng) * 0.6;
  p["frequency_start"] = 0.3 + rand_double(rng) * 0.3;
  p["frequency_slide"] = 0.1 + rand_double(rng) * 0.2;

  p["sustainTime"] = 0.1 + rand_double(rng) * 0.3;
  p["decayTime"] = 0.1 + rand_double(rng) * 0.2;

  if (rand_double(rng) < 0.5) p["hpFilterCutoff"] = rand_double(rng) * 0.3;
  if (rand_double(rng) < 0.5) p["lpFilterCutoff"] = 1.0 - rand_double(rng) * 0.6;
  return p;
}

ParamMap generate_blip_select(Mulberry32& rng) {
  ParamMap p = default_params();
  // Choices: Square(0), Saw(1), FMSyn(10), Whistle(7)
  p["waveType"] = rand_choice(rng, {0, 1, 10, 7});
  if (static_cast<int>(p["waveType"]) == 0) {
    p["squareDuty"] = rand_double(rng) * 0.6;
  }

  p["frequency_start"] = 0.2 + rand_double(rng) * 0.4;
  p["sustainTime"] = 0.1 + rand_double(rng) * 0.1;
  p["decayTime"] = rand_double(rng) * 0.2;
  p["hpFilterCutoff"] = 0.1;
  return p;
}

ParamMap generate_random(Mulberry32& rng) {
  ParamMap p = default_params();

  struct PowerRule {
    const char* name;
    int power;
  };
  static const PowerRule kPowers[] = {
      {"attackTime", 4}, {"sustainTime", 2}, {"sustainPunch", 2},
      {"overtones", 3}, {"overtoneFalloff", 2}, {"vibratoDepth", 3},
      {"dutySweep", 3}, {"flangerOffset", 3}, {"flangerSweep", 3},
      {"lpFilterCutoff", 3}, {"lpFilterCutoffSweep", 3}, {"hpFilterCutoff", 5},
      {"hpFilterCutoffSweep", 5}, {"bitCrush", 4}, {"bitCrushSweep", 5},
      {"frequency_slide", 4}, {"frequency_acceleration", 7}, {"frequency_start", 4}
  };

  for (auto& kv : p) {
    if (kv.first == "masterVolume" || kv.first == "waveType") continue;
    double min_val = param_min(kv.first);
    double max_val = param_max(kv.first);
    double def_val = kv.second;

    double r = rand_double(rng);
    for (const auto& rule : kPowers) {
      if (kv.first == rule.name) {
        r = std::pow(r, rule.power);
        break;
      }
    }

    bool above = rand_double(rng) < 0.5;
    if (min_val == def_val) above = true;
    if (max_val == def_val) above = false;

    if (above) {
      kv.second = def_val + (max_val - def_val) * r;
    } else {
      kv.second = def_val - (def_val - min_val) * r;
    }
  }

  // WaveType selection
  p["waveType"] = static_cast<int>(rand_double(rng) * 11);

  if (rand_double(rng) < 0.5) p["repeatSpeed"] = 0;
  p["min_frequency_relative_to_starting_frequency"] = 0;
  p["compressionAmount"] = 0;

  rectify_params(p, rng);
  return p;
}

ParamMap mutate_params(const ParamMap& current, Mulberry32& rng) {
  ParamMap p = current;

  // Small probability to mutate waveType
  if (rand_double(rng) < 0.1) {
    int wave_count = 12;
    int offset = static_cast<int>(rand_double(rng) * (wave_count - 1));
    p["waveType"] = (static_cast<int>(p["waveType"]) + offset) % wave_count;
    rectify_params(p, rng);
    return p;
  }

  for (auto& kv : p) {
    if (kv.first == "masterVolume" || kv.first == "waveType") continue;
    if (rand_double(rng) < 0.5) continue;

    double min_val = param_min(kv.first);
    double max_val = param_max(kv.first);
    double range = max_val - min_val;
    double mutated_diff = (rand_double(rng) - 0.5) * 0.1 * range;
    kv.second = std::max(min_val, std::min(max_val, kv.second + mutated_diff));
  }

  rectify_params(p, rng);
  return p;
}

ParamMap generate_preset(PresetCategory category, Mulberry32& rng, const ParamMap* base_for_mutate) {
  switch (category) {
    case PresetCategory::PickupCoin: return generate_pickup_coin(rng);
    case PresetCategory::LaserShoot: return generate_laser_shoot(rng);
    case PresetCategory::Explosion: return generate_explosion(rng);
    case PresetCategory::Powerup: return generate_powerup(rng);
    case PresetCategory::HitHurt: return generate_hit_hurt(rng);
    case PresetCategory::Jump: return generate_jump(rng);
    case PresetCategory::BlipSelect: return generate_blip_select(rng);
    case PresetCategory::Randomize: return generate_random(rng);
    case PresetCategory::Mutate:
      return mutate_params(base_for_mutate ? *base_for_mutate : default_params(), rng);
  }
  return default_params();
}

}  // namespace bfxr

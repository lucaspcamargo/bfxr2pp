#pragma once

#include "bfxr2/params.h"
#include "bfxr2/prng.h"

#include <string>
#include <vector>

namespace bfxr {

// Procedural sound categories from original Bfxr
enum class PresetCategory {
  PickupCoin,
  LaserShoot,
  Explosion,
  Powerup,
  HitHurt,
  Jump,
  BlipSelect,
  Randomize,
  Mutate
};

struct PresetMeta {
  PresetCategory category;
  std::string name;       // e.g. "Pickup/Coin"
  std::string short_name; // e.g. "Pickup"
  std::string description;
};

// Returns metadata for all available presets
const std::vector<PresetMeta>& get_preset_list();

// Procedurally generate parameters for each category
ParamMap generate_pickup_coin(Mulberry32& rng);
ParamMap generate_laser_shoot(Mulberry32& rng);
ParamMap generate_explosion(Mulberry32& rng);
ParamMap generate_powerup(Mulberry32& rng);
ParamMap generate_hit_hurt(Mulberry32& rng);
ParamMap generate_jump(Mulberry32& rng);
ParamMap generate_blip_select(Mulberry32& rng);

// Full weighted randomization across parameters
ParamMap generate_random(Mulberry32& rng);

// Slightly mutate existing parameters
ParamMap mutate_params(const ParamMap& current, Mulberry32& rng);

// Dispatcher by preset category
ParamMap generate_preset(PresetCategory category, Mulberry32& rng, const ParamMap* base_for_mutate = nullptr);

}  // namespace bfxr

#include "darwinsim/config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <type_traits>

namespace darwinsim {
namespace {

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::unordered_map<std::string, std::string> parse_simple_yaml(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open config: " + path);

     std::unordered_map<std::string, std::string> values;
     std::string section;
     std::string line;
     while (std::getline(input, line)) {
         const auto comment = line.find('#');
         if (comment != std::string::npos) line.resize(comment);
         if (trim(line).empty()) continue;

         const std::size_t indent = line.find_first_not_of(' ');
         const std::string clean = trim(line);
         const auto colon = clean.find(':');
         if (colon == std::string::npos) continue;
         const std::string key = trim(clean.substr(0, colon));
         const std::string value = trim(clean.substr(colon + 1));

         if ((indent == 0 || indent == std::string::npos) && value.empty()) {
             section = key;
             continue;
         }
         if (!section.empty() && !value.empty()) values[section + "." + key] = value;
     }
     return values;
}

template <typename T>
void assign_numeric(const std::unordered_map<std::string, std::string>& values,
                    const std::string& key,
                    T& target) {
    const auto it = values.find(key);
    if (it == values.end()) return;
    if constexpr (std::is_integral_v<T>) {
        target = static_cast<T>(std::stoull(it->second));
    } else {
        target = static_cast<T>(std::stod(it->second));
    }
}

}   // namespace

SimulationConfig load_config(const std::string& path) {
    const auto v = parse_simple_yaml(path);
    SimulationConfig c;

     assign_numeric(v, "simulation.seed", c.seed);
     assign_numeric(v, "simulation.initial_population", c.initial_population);
     assign_numeric(v, "simulation.max_population", c.max_population);
     assign_numeric(v, "simulation.ticks_per_checkpoint", c.ticks_per_checkpoint);

     assign_numeric(v, "world.width", c.world.width);
     assign_numeric(v, "world.height", c.world.height);
     assign_numeric(v, "world.initial_plants", c.world.initial_plants);
      assign_numeric(v, "world.plant_energy", c.world.plant_energy);
      assign_numeric(v, "world.plant_regeneration_probability", c.world.plant_regeneration_probability);
      assign_numeric(v, "world.carcass_decay_per_tick", c.world.carcass_decay_per_tick);
      assign_numeric(v, "world.interaction_distance", c.world.interaction_distance);

      assign_numeric(v, "world.terrain_enabled", c.world.terrain.enabled);
      assign_numeric(v, "world.terrain_cols", c.world.terrain.cols);
      assign_numeric(v, "world.terrain_rows", c.world.terrain.rows);
      assign_numeric(v, "world.terrain_noise_cell_span", c.world.terrain.noise_cell_span);
      assign_numeric(v, "world.terrain_seed_offset", c.world.terrain.seed_offset);
      assign_numeric(v, "world.river_half_width", c.world.terrain.river_half_width);
      assign_numeric(v, "world.desert_moisture_threshold", c.world.terrain.desert_moisture_threshold);
      assign_numeric(v, "world.highland_elevation_threshold", c.world.terrain.highland_elevation_threshold);
      assign_numeric(v, "world.river_regen_multiplier", c.world.terrain.river_regen_multiplier);
      assign_numeric(v, "world.plains_regen_multiplier", c.world.terrain.plains_regen_multiplier);
      assign_numeric(v, "world.desert_regen_multiplier", c.world.terrain.desert_regen_multiplier);
      assign_numeric(v, "world.highland_regen_multiplier", c.world.terrain.highland_regen_multiplier);
      assign_numeric(v, "world.river_movement_multiplier", c.world.terrain.river_movement_multiplier);
      assign_numeric(v, "world.plains_movement_multiplier", c.world.terrain.plains_movement_multiplier);
      assign_numeric(v, "world.desert_movement_multiplier", c.world.terrain.desert_movement_multiplier);
      assign_numeric(v, "world.highland_movement_multiplier", c.world.terrain.highland_movement_multiplier);
      assign_numeric(v, "world.initial_plant_fertility_bias", c.world.terrain.initial_plant_fertility_bias);

      c.world.terrain.cols = std::max<std::uint32_t>(1, c.world.terrain.cols);
      c.world.terrain.rows = std::max<std::uint32_t>(1, c.world.terrain.rows);
      c.world.terrain.noise_cell_span = std::max<std::uint32_t>(1, c.world.terrain.noise_cell_span);
      c.world.terrain.river_half_width = std::max(0.0, c.world.terrain.river_half_width);
      c.world.terrain.desert_moisture_threshold =
          std::clamp(c.world.terrain.desert_moisture_threshold, 0.0, 1.0);
      c.world.terrain.highland_elevation_threshold =
          std::clamp(c.world.terrain.highland_elevation_threshold, 0.0, 1.0);
      c.world.terrain.river_regen_multiplier = std::max(0.01, c.world.terrain.river_regen_multiplier);
      c.world.terrain.plains_regen_multiplier = std::max(0.01, c.world.terrain.plains_regen_multiplier);
      c.world.terrain.desert_regen_multiplier = std::max(0.01, c.world.terrain.desert_regen_multiplier);
      c.world.terrain.highland_regen_multiplier = std::max(0.01, c.world.terrain.highland_regen_multiplier);
      c.world.terrain.river_movement_multiplier = std::max(0.10, c.world.terrain.river_movement_multiplier);
      c.world.terrain.plains_movement_multiplier = std::max(0.10, c.world.terrain.plains_movement_multiplier);
      c.world.terrain.desert_movement_multiplier = std::max(0.10, c.world.terrain.desert_movement_multiplier);
      c.world.terrain.highland_movement_multiplier = std::max(0.10, c.world.terrain.highland_movement_multiplier);
      c.world.terrain.initial_plant_fertility_bias =
          std::clamp(c.world.terrain.initial_plant_fertility_bias, 0.0, 1.0);

      if (const auto it = v.find("hardware.mode"); it != v.end()) {
          c.hardware_mode = it->second == "coupled" ? HardwareMode::Coupled : HardwareMode::Deterministic;
      }
      assign_numeric(v, "hardware.deterministic_compute_credits", c.deterministic_compute_credits);
      assign_numeric(v, "hardware.credits_per_core", c.credits_per_core);

      assign_numeric(v, "evolution.speciation_interval_ticks", c.speciation.interval_ticks);
      assign_numeric(v, "evolution.min_subgroup_population", c.speciation.min_subgroup_population);
      assign_numeric(v, "evolution.split_threshold", c.speciation.split_threshold);
      assign_numeric(v, "evolution.join_threshold", c.speciation.join_threshold);
      assign_numeric(v, "evolution.stability_checks", c.speciation.stability_checks);

      assign_numeric(v, "energy.initial_energy", c.initial_energy);
      assign_numeric(v, "energy.max_energy", c.max_energy);
      assign_numeric(v, "energy.reproduction_min_energy", c.reproduction_min_energy);
      assign_numeric(v, "energy.reproduction_contribution", c.reproduction_contribution);
      assign_numeric(v, "energy.reproduction_cooldown", c.reproduction_cooldown);
      assign_numeric(v, "energy.attack_energy_cost", c.attack_energy_cost);
      assign_numeric(v, "energy.movement_energy_per_mass", c.movement_energy_per_mass);
      assign_numeric(v, "energy.brain_energy_per_credit", c.brain_energy_per_credit);
     assign_numeric(v, "energy.starvation_damage", c.starvation_damage);

     return c;
}

}   // namespace darwinsim

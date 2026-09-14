#include "darwinsim/checkpoint.hpp"
#include "darwinsim/evolution.hpp"
#include "darwinsim/phenotype.hpp"
#include "darwinsim/rng.hpp"
#include "darwinsim/simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& name) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    } else {
        std::cout << "PASS: " << name << '\n';
    }
}

void test_rng() {
    const auto a = darwinsim::DeterministicRng::hash(1, 2, 3, darwinsim::RandomDomain::Decision, 4);
    const auto b = darwinsim::DeterministicRng::hash(1, 2, 3, darwinsim::RandomDomain::Decision, 4);
    const auto c = darwinsim::DeterministicRng::hash(1, 2, 4, darwinsim::RandomDomain::Decision, 4);
    check(a == b, "deterministic RNG repeats");
    check(a != c, "deterministic RNG changes with tick");
}

void test_genetics() {
    darwinsim::Genome a;
    darwinsim::Genome b = a;
    b.body_size = 0.9F;
    check(darwinsim::genetic_distance(a, a) == 0.0, "genetic distance identity");
    check(darwinsim::genetic_distance(a, b) > 0.0, "genetic distance separation");
    check(darwinsim::reproductive_compatibility(a, a) > darwinsim::reproductive_compatibility(a, b),
          "compatibility falls with distance");

    const auto child1 = darwinsim::crossover_and_mutate(a, b, 77, 1001, 200);
    const auto child2 = darwinsim::crossover_and_mutate(a, b, 77, 1001, 200);
    check(child1.as_array() == child2.as_array(), "crossover/mutation deterministic for same identity");
}

void test_phenotype_tradeoff() {
    darwinsim::Genome low;
    low.compute_capacity = 0.1F;
    darwinsim::Genome high = low;
    high.compute_capacity = 0.95F;
    const auto p_low = darwinsim::derive_phenotype(low);
    const auto p_high = darwinsim::derive_phenotype(high);
    check(p_high.requested_compute > p_low.requested_compute, "compute gene increases compute demand");
    check(p_high.basal_energy_cost > p_low.basal_energy_cost, "larger brain costs more energy");
}


void test_terrain_heterogeneity() {
    darwinsim::WorldConfig config;
    config.width = 512.0;
    config.height = 512.0;
    config.initial_plants = 600;
    config.plant_regeneration_probability = 0.004;

    darwinsim::World a(config);
    darwinsim::World b(config);
    darwinsim::World different(config);
    a.initialize(12345);
    b.initialize(12345);
    different.initialize(12346);

    check(a.terrain_signature() == b.terrain_signature(), "terrain heatmap deterministic for same seed/config");
    check(a.terrain_signature() != different.terrain_signature(), "terrain heatmap changes with seed");

    std::array<std::size_t, 4> counts{};
    double min_regen = std::numeric_limits<double>::infinity();
    double max_regen = 0.0;
    double terrain_regen_sum = 0.0;
    for (const auto& cell : a.terrain_cells()) {
        ++counts[static_cast<std::size_t>(cell.type)];
        min_regen = std::min(min_regen, cell.plant_regen_multiplier);
        max_regen = std::max(max_regen, cell.plant_regen_multiplier);
        terrain_regen_sum += cell.plant_regen_multiplier;
    }
    check(counts[static_cast<std::size_t>(darwinsim::TerrainType::River)] > 0, "terrain contains river cells");
    check(counts[static_cast<std::size_t>(darwinsim::TerrainType::Plains)] > 0, "terrain contains plains cells");
    check(counts[static_cast<std::size_t>(darwinsim::TerrainType::Desert)] > 0, "terrain contains desert cells");
    check(counts[static_cast<std::size_t>(darwinsim::TerrainType::Highlands)] > 0, "terrain contains highland cells");
    check(max_regen > min_regen * 2.0, "plant regeneration varies strongly by location");

    double min_move = std::numeric_limits<double>::infinity();
    double max_move = 0.0;
    for (const auto& cell : a.terrain_cells()) {
        min_move = std::min(min_move, cell.movement_cost_multiplier);
        max_move = std::max(max_move, cell.movement_cost_multiplier);
    }
    check(max_move > min_move * 1.25, "terrain changes movement cost enough to create barriers");

    double plant_regen_sum = 0.0;
    for (const auto& plant : a.plants()) {
        plant_regen_sum += a.terrain_at(plant.position).plant_regen_multiplier;
    }
    const double mean_terrain_regen = terrain_regen_sum / std::max<std::size_t>(1, a.terrain_cells().size());
    const double mean_plant_regen = plant_regen_sum / std::max<std::size_t>(1, a.plants().size());
    check(mean_plant_regen > mean_terrain_regen, "initial plants are biased toward fertile terrain");

    darwinsim::WorldConfig uniform_config = config;
    uniform_config.terrain.enabled = false;
    darwinsim::World uniform_world(uniform_config);
    uniform_world.initialize(12345);
    bool all_uniform = true;
    for (const auto& cell : uniform_world.terrain_cells()) {
        all_uniform = all_uniform && cell.type == darwinsim::TerrainType::Plains &&
                      std::abs(cell.plant_regen_multiplier - 1.0) < 1e-12 &&
                      std::abs(cell.movement_cost_multiplier - 1.0) < 1e-12;
    }
    check(all_uniform, "terrain-disabled control is a uniform plains world");
}

void test_simulation_determinism() {
    darwinsim::SimulationConfig config;
    config.initial_population = 60;
    config.max_population = 200;
    config.world.initial_plants = 250;
    config.seed = 987654;

    darwinsim::Simulation a(config);
    darwinsim::Simulation b(config);
    a.initialize();
    b.initialize();
    for (int i = 0; i < 120; ++i) {
        a.step_local();
        b.step_local();
    }
    check(darwinsim::state_hash(a) == darwinsim::state_hash(b), "same seed/config gives same state hash");
}

void test_checkpoint_roundtrip() {
    darwinsim::SimulationConfig config;
    config.initial_population = 40;
    config.max_population = 120;
    config.world.initial_plants = 150;
    config.seed = 4444;

      darwinsim::Simulation source(config);
      source.initialize();
      for (int i = 0; i < 50; ++i) source.step_local();

      const auto path = (std::filesystem::temp_directory_path() / "darwinsim-test.chk").string();
      darwinsim::save_checkpoint(source, path);

      darwinsim::Simulation restored(config);
      restored.initialize();
      darwinsim::load_checkpoint(restored, path);
      check(source.summary().tick == restored.summary().tick, "checkpoint restores tick");
      check(darwinsim::state_hash(source) == darwinsim::state_hash(restored), "checkpoint restores deterministic state");

      auto wrong_config = config;
      ++wrong_config.world.terrain.seed_offset;
      darwinsim::Simulation wrong_world(wrong_config);
      wrong_world.initialize();
      bool rejected = false;
      try {
          darwinsim::load_checkpoint(wrong_world, path);
      } catch (const std::runtime_error&) {
          rejected = true;
      }
      check(rejected, "checkpoint rejects a different terrain/configuration");
      std::remove(path.c_str());
 }

}   // namespace

int main() {
    test_rng();
    test_genetics();
    test_phenotype_tradeoff();
    test_terrain_heterogeneity();
    test_simulation_determinism();
    test_checkpoint_roundtrip();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}

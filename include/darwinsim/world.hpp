#pragma once

#include "darwinsim/types.hpp"

#include <cstdint>
#include <vector>

namespace darwinsim {

enum class TerrainType : std::uint8_t {
    River = 0,
    Plains = 1,
    Desert = 2,
    Highlands = 3
};

struct TerrainConfig {
    bool enabled{true};
    std::uint32_t cols{64};
    std::uint32_t rows{64};
    std::uint32_t noise_cell_span{8};
    std::uint64_t seed_offset{424242};

    double river_half_width{10.0};
    double desert_moisture_threshold{0.28};
    double highland_elevation_threshold{0.72};

    double river_regen_multiplier{2.40};
    double plains_regen_multiplier{1.00};
    double desert_regen_multiplier{0.18};
    double highland_regen_multiplier{0.60};

    double river_movement_multiplier{1.20};
    double plains_movement_multiplier{1.00};
    double desert_movement_multiplier{1.15};
    double highland_movement_multiplier{1.55};

    // 0.0 = initial plant positions are uniform. 1.0 = strongly favor fertile cells.
    double initial_plant_fertility_bias{0.70};
};

struct TerrainCell {
    TerrainType type{TerrainType::Plains};
    double elevation{0.0};
    double moisture{0.5};
    double plant_regen_multiplier{1.0};
    double movement_cost_multiplier{1.0};
};

struct PlantResource {
    ResourceId id{0};
    Vec2 position{};
    double energy{0.0};
    bool available{true};
};

struct CarcassResource {
    ResourceId id{0};
    AnimalId source_animal{0};
    Vec2 position{};
    double energy{0.0};
    std::uint64_t created_tick{0};
    bool available{true};
};

struct WorldConfig {
    double width{512.0};
    double height{512.0};
    std::uint32_t initial_plants{800};
    double plant_energy{28.0};
    double plant_regeneration_probability{0.0025};
    double carcass_decay_per_tick{0.02};
    double interaction_distance{1.8};
    TerrainConfig terrain{};
};

class World {
public:
    explicit World(WorldConfig config = {});

    const WorldConfig& config() const { return config_; }
    std::vector<PlantResource>& plants() { return plants_; }
    const std::vector<PlantResource>& plants() const { return plants_; }
    std::vector<CarcassResource>& carcasses() { return carcasses_; }
    const std::vector<CarcassResource>& carcasses() const { return carcasses_; }
    const std::vector<TerrainCell>& terrain_cells() const { return terrain_cells_; }

    void initialize(std::uint64_t seed);
    void tick_resources(std::uint64_t seed, std::uint64_t tick);
    ResourceId add_carcass(AnimalId source, Vec2 position, double energy, std::uint64_t tick);
    void recompute_next_resource_id();

    const TerrainCell& terrain_at(Vec2 position) const;
    double plant_regeneration_probability_at(Vec2 position) const;
    double movement_cost_multiplier(Vec2 from, Vec2 to, std::uint32_t samples = 5) const;
    std::uint64_t terrain_signature() const;

private:
    void build_terrain(std::uint64_t seed);
    Vec2 choose_initial_plant_position(std::uint64_t seed, ResourceId plant_id) const;
    std::size_t terrain_index(Vec2 position) const;

    WorldConfig config_;
    std::vector<PlantResource> plants_;
    std::vector<CarcassResource> carcasses_;
    std::vector<TerrainCell> terrain_cells_;
    ResourceId next_resource_id_{1};
};

} // namespace darwinsim

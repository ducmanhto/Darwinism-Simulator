#include "darwinsim/world.hpp"

#include "darwinsim/rng.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace darwinsim {
namespace {

double lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

std::uint64_t lattice_id(std::uint32_t x, std::uint32_t y) {
    return (static_cast<std::uint64_t>(y) << 32U) | static_cast<std::uint64_t>(x);
}

std::uint64_t fnv1a(std::uint64_t hash, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

double biome_regen_multiplier(TerrainType type, const TerrainConfig& config) {
    switch (type) {
        case TerrainType::River: return config.river_regen_multiplier;
        case TerrainType::Plains: return config.plains_regen_multiplier;
        case TerrainType::Desert: return config.desert_regen_multiplier;
        case TerrainType::Highlands: return config.highland_regen_multiplier;
    }
    return 1.0;
}

double biome_movement_multiplier(TerrainType type, const TerrainConfig& config) {
    switch (type) {
        case TerrainType::River: return config.river_movement_multiplier;
        case TerrainType::Plains: return config.plains_movement_multiplier;
        case TerrainType::Desert: return config.desert_movement_multiplier;
        case TerrainType::Highlands: return config.highland_movement_multiplier;
    }
    return 1.0;
}

} // namespace

World::World(WorldConfig config) : config_(config) {}

void World::build_terrain(std::uint64_t seed) {
    const auto cols = std::max<std::uint32_t>(1, config_.terrain.cols);
    const auto rows = std::max<std::uint32_t>(1, config_.terrain.rows);
    const auto span = std::max<std::uint32_t>(1, config_.terrain.noise_cell_span);
    const std::uint64_t terrain_seed = seed + config_.terrain.seed_offset;

    terrain_cells_.assign(static_cast<std::size_t>(cols) * rows, TerrainCell{});

    if (!config_.terrain.enabled) {
        for (auto& cell : terrain_cells_) {
            cell.type = TerrainType::Plains;
            cell.elevation = 0.25;
            cell.moisture = 0.50;
            cell.plant_regen_multiplier = 1.0;
            cell.movement_cost_multiplier = 1.0;
        }
        return;
    }

    const auto random_lattice = [&](std::uint32_t gx, std::uint32_t gy, std::uint64_t sample) {
        return DeterministicRng::uniform01(
            terrain_seed, lattice_id(gx, gy), 0, RandomDomain::Terrain, sample);
    };

    const auto value_noise = [&](std::uint32_t cx, std::uint32_t cy, std::uint64_t sample) {
        const std::uint32_t gx0 = cx / span;
        const std::uint32_t gy0 = cy / span;
        const std::uint32_t gx1 = gx0 + 1;
        const std::uint32_t gy1 = gy0 + 1;
        const double tx = static_cast<double>(cx % span) / static_cast<double>(span);
        const double ty = static_cast<double>(cy % span) / static_cast<double>(span);

        const double n00 = random_lattice(gx0, gy0, sample);
        const double n10 = random_lattice(gx1, gy0, sample);
        const double n01 = random_lattice(gx0, gy1, sample);
        const double n11 = random_lattice(gx1, gy1, sample);
        return lerp(lerp(n00, n10, tx), lerp(n01, n11, tx), ty);
    };

    const auto river_anchor = [&](std::uint32_t anchor) {
        const auto sample = [&](std::uint32_t a) {
            return random_lattice(a, 0, 900);
        };
        const std::uint32_t prev = anchor == 0 ? 0 : anchor - 1;
        const double smooth = (sample(prev) + 2.0 * sample(anchor) + sample(anchor + 1)) / 4.0;
        return config_.height * (0.15 + 0.70 * smooth);
    };

    for (std::uint32_t cy = 0; cy < rows; ++cy) {
        for (std::uint32_t cx = 0; cx < cols; ++cx) {
            const double world_x = (static_cast<double>(cx) + 0.5) * config_.width / cols;
            const double world_y = (static_cast<double>(cy) + 0.5) * config_.height / rows;

            const double river_position = world_x / std::max(1.0, config_.width) * (cols - 1);
            const std::uint32_t river_segment = static_cast<std::uint32_t>(river_position) / span;
            const double river_t = static_cast<double>(static_cast<std::uint32_t>(river_position) % span) /
                                   static_cast<double>(span);
            const double river_y = lerp(river_anchor(river_segment), river_anchor(river_segment + 1), river_t);
            const double river_distance = std::abs(world_y - river_y);

            const double elevation = std::clamp(
                0.72 * value_noise(cx, cy, 100) + 0.28 * value_noise(cx, cy, 101), 0.0, 1.0);
            const double river_proximity = std::clamp(
                1.0 - river_distance / std::max(1.0, config_.terrain.river_half_width * 6.0), 0.0, 1.0);
            const double moisture = std::clamp(
                0.72 * value_noise(cx, cy, 200) + 0.28 * river_proximity, 0.0, 1.0);

            TerrainType type = TerrainType::Plains;
            if (river_distance <= config_.terrain.river_half_width) {
                type = TerrainType::River;
            } else if (elevation >= config_.terrain.highland_elevation_threshold) {
                type = TerrainType::Highlands;
            } else if (moisture <= config_.terrain.desert_moisture_threshold) {
                type = TerrainType::Desert;
            }

            TerrainCell cell;
            cell.type = type;
            cell.elevation = elevation;
            cell.moisture = moisture;

            // The discrete biome gives a large-scale niche, while moisture adds a
            // continuous local fertility gradient inside that biome.
            const double local_fertility = 0.72 + 0.56 * moisture;
            cell.plant_regen_multiplier = std::max(
                0.01, biome_regen_multiplier(type, config_.terrain) * local_fertility);

            // Elevation adds a small continuous cost on top of the biome cost.
            cell.movement_cost_multiplier = std::max(
                0.1, biome_movement_multiplier(type, config_.terrain) * (1.0 + 0.12 * elevation));

            terrain_cells_[static_cast<std::size_t>(cy) * cols + cx] = cell;
        }
    }
}

std::size_t World::terrain_index(Vec2 position) const {
    const auto cols = std::max<std::uint32_t>(1, config_.terrain.cols);
    const auto rows = std::max<std::uint32_t>(1, config_.terrain.rows);
    const double x = std::clamp(position.x, 0.0, config_.width);
    const double y = std::clamp(position.y, 0.0, config_.height);
    const auto cx = std::min<std::uint32_t>(
        cols - 1, static_cast<std::uint32_t>(x / std::max(1.0, config_.width) * cols));
    const auto cy = std::min<std::uint32_t>(
        rows - 1, static_cast<std::uint32_t>(y / std::max(1.0, config_.height) * rows));
    return static_cast<std::size_t>(cy) * cols + cx;
}

const TerrainCell& World::terrain_at(Vec2 position) const {
    static const TerrainCell fallback{};
    if (terrain_cells_.empty()) return fallback;
    return terrain_cells_[terrain_index(position)];
}

double World::plant_regeneration_probability_at(Vec2 position) const {
    const double multiplier = terrain_at(position).plant_regen_multiplier;
    return std::clamp(config_.plant_regeneration_probability * multiplier, 0.0, 1.0);
}

double World::movement_cost_multiplier(Vec2 from, Vec2 to, std::uint32_t samples) const {
    samples = std::max<std::uint32_t>(2, samples);
    double total = 0.0;
    for (std::uint32_t i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(samples - 1);
        const Vec2 p{lerp(from.x, to.x, t), lerp(from.y, to.y, t)};
        total += terrain_at(p).movement_cost_multiplier;
    }
    return total / static_cast<double>(samples);
}

Vec2 World::choose_initial_plant_position(std::uint64_t seed, ResourceId plant_id) const {
    constexpr std::uint32_t kCandidates = 6;
    const double bias = std::clamp(config_.terrain.initial_plant_fertility_bias, 0.0, 1.0);

    // This is the clean control path: disabling terrain (or setting bias to zero)
    // restores uniform plant-site placement with the original initialization samples.
    if (!config_.terrain.enabled || bias <= 0.0) {
        return {
            DeterministicRng::uniform(seed, plant_id, 0, RandomDomain::Initialization, 0.0, config_.width, 0),
            DeterministicRng::uniform(seed, plant_id, 0, RandomDomain::Initialization, 0.0, config_.height, 1)
        };
    }

    double max_fertility = 1.0;
    for (const auto& cell : terrain_cells_) {
        max_fertility = std::max(max_fertility, cell.plant_regen_multiplier);
    }

    Vec2 best{};
    double best_score = -std::numeric_limits<double>::infinity();
    for (std::uint32_t i = 0; i < kCandidates; ++i) {
        Vec2 candidate;
        candidate.x = DeterministicRng::uniform(
            seed, plant_id, 0, RandomDomain::Initialization, 0.0, config_.width, 1000 + i * 3);
        candidate.y = DeterministicRng::uniform(
            seed, plant_id, 0, RandomDomain::Initialization, 0.0, config_.height, 1001 + i * 3);
        const double tie_break = DeterministicRng::uniform01(
            seed, plant_id, 0, RandomDomain::Initialization, 1002 + i * 3);
        const double fertility = terrain_at(candidate).plant_regen_multiplier / max_fertility;
        const double score = (1.0 - bias) * tie_break + bias * fertility;
        if (score > best_score) {
            best_score = score;
            best = candidate;
        }
    }
    return best;
}

void World::initialize(std::uint64_t seed) {
    plants_.clear();
    carcasses_.clear();
    next_resource_id_ = 1;
    build_terrain(seed);

    plants_.reserve(config_.initial_plants);
    for (std::uint32_t i = 0; i < config_.initial_plants; ++i) {
        PlantResource plant;
        plant.id = next_resource_id_++;
        plant.position = choose_initial_plant_position(seed, plant.id);
        plant.energy = config_.plant_energy;
        plant.available = true;
        plants_.push_back(plant);
    }
}

void World::tick_resources(std::uint64_t seed, std::uint64_t tick) {
    for (auto& plant : plants_) {
        if (!plant.available) {
            const double p = DeterministicRng::uniform01(
                seed, plant.id, tick, RandomDomain::ResourceSpawn);
            if (p < plant_regeneration_probability_at(plant.position)) {
                plant.available = true;
                plant.energy = config_.plant_energy;
            }
        }
    }

    for (auto& carcass : carcasses_) {
        if (!carcass.available) continue;
        carcass.energy = std::max(0.0, carcass.energy - config_.carcass_decay_per_tick);
        if (carcass.energy <= 0.0) carcass.available = false;
    }

    if (carcasses_.size() > 10000) {
        carcasses_.erase(
            std::remove_if(carcasses_.begin(), carcasses_.end(),
                           [](const CarcassResource& c) { return !c.available; }),
            carcasses_.end());
    }
}

void World::recompute_next_resource_id() {
    ResourceId maximum = 0;
    for (const auto& plant : plants_) maximum = std::max(maximum, plant.id);
    for (const auto& carcass : carcasses_) maximum = std::max(maximum, carcass.id);
    next_resource_id_ = maximum + 1;
}

ResourceId World::add_carcass(AnimalId source, Vec2 position, double energy, std::uint64_t tick) {
    CarcassResource c;
    c.id = next_resource_id_++;
    c.source_animal = source;
    c.position = position;
    c.energy = std::max(0.0, energy);
    c.created_tick = tick;
    c.available = c.energy > 0.0;
    carcasses_.push_back(c);
    return c.id;
}

std::uint64_t World::terrain_signature() const {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto cols = std::max<std::uint32_t>(1, config_.terrain.cols);
    const auto rows = std::max<std::uint32_t>(1, config_.terrain.rows);
    hash = fnv1a(hash, &cols, sizeof(cols));
    hash = fnv1a(hash, &rows, sizeof(rows));

    for (const auto& cell : terrain_cells_) {
        const auto type = static_cast<std::uint8_t>(cell.type);
        const auto elevation = static_cast<std::int32_t>(std::llround(cell.elevation * 1000000.0));
        const auto moisture = static_cast<std::int32_t>(std::llround(cell.moisture * 1000000.0));
        const auto regen = static_cast<std::int32_t>(std::llround(cell.plant_regen_multiplier * 1000000.0));
        const auto movement = static_cast<std::int32_t>(std::llround(cell.movement_cost_multiplier * 1000000.0));
        hash = fnv1a(hash, &type, sizeof(type));
        hash = fnv1a(hash, &elevation, sizeof(elevation));
        hash = fnv1a(hash, &moisture, sizeof(moisture));
        hash = fnv1a(hash, &regen, sizeof(regen));
        hash = fnv1a(hash, &movement, sizeof(movement));
    }
    return hash;
}

} // namespace darwinsim

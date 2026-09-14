#pragma once

#include "darwinsim/genome.hpp"
#include "darwinsim/phenotype.hpp"
#include "darwinsim/types.hpp"

#include <cstdint>
#include <deque>

namespace darwinsim {

struct MemoryEvent {
    std::uint64_t tick{0};
    std::uint64_t entity_id{0};
    Vec2 position{};
    double value{0.0};
};

struct Lineage {
    AnimalId parent_a{0};
    AnimalId parent_b{0};
    std::uint32_t generation{0};
};

struct AnimalState {
    Vec2 position{};
    double energy{100.0};
    double health{100.0};
    std::uint64_t age{0};
    std::uint64_t reproduction_cooldown{0};
    bool alive{true};
};

struct Animal {
    AnimalId id{0};
    SpeciesId species_id{1};
    Genome genome{};
    Phenotype phenotype{};
    AnimalState state{};
    Lineage lineage{};
    std::deque<MemoryEvent> memory{};
};

}   // namespace darwinsim

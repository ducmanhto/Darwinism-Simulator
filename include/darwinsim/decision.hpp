#pragma once

#include "darwinsim/animal.hpp"
#include "darwinsim/types.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace darwinsim {

enum class ResourceKind : std::uint8_t { Plant = 0, Carcass = 1 };

struct ObservedResource {
    ResourceId id{0};
    ResourceKind kind{ResourceKind::Plant};
    Vec2 position{};
    double distance{0.0};
    double energy{0.0};
};

struct ObservedAnimal {
    AnimalId id{0};
    SpeciesId species_id{0};
    Vec2 position{};
      double distance{0.0};
      double apparent_mass{0.0};
      double health_fraction{1.0};
      double threat{0.0};
      bool mature{false};
};

struct DecisionInput {
    std::uint64_t tick{0};
    std::uint64_t seed{0};
    AnimalId animal_id{0};
    SpeciesId species_id{0};
    Genome genome{};
    Phenotype phenotype{};
    AnimalState state{};
    std::uint32_t granted_compute{64};
    std::vector<ObservedResource> resources;
    std::vector<ObservedAnimal> animals;
};

struct Action {
    AnimalId animal_id{0};
    ActionType type{ActionType::Rest};
    std::uint64_t target_id{0};
    Vec2 destination{};
    double utility{0.0};
};

class DecisionEngine {
public:
    static Action decide(const DecisionInput& input);
};

}    // namespace darwinsim

#pragma once

#include "darwinsim/animal.hpp"
#include "darwinsim/genome.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace darwinsim {

double genetic_distance(const Genome& a, const Genome& b);
double reproductive_compatibility(const Genome& a, const Genome& b, double tau = 0.22);
Genome crossover_and_mutate(const Genome& a,
                            const Genome& b,
                            std::uint64_t seed,
                            AnimalId child_id,
                            std::uint64_t tick);

struct SpeciesStats {
    SpeciesId id{0};
    SpeciesId parent_species{0};
    std::uint64_t birth_tick{0};
    std::size_t population{0};
    Genome centroid{};
    double diversity{0.0};
};

struct SpeciationEvent {
    SpeciesId parent{0};
    SpeciesId child{0};
    std::size_t moved_population{0};
    double centroid_distance{0.0};
};

struct SpeciationConfig {
    std::uint64_t interval_ticks{250};
    std::size_t min_subgroup_population{20};
    double split_threshold{0.28};
    double join_threshold{0.18};
    std::uint32_t stability_checks{3};
};

class SpeciationManager {
public:
    explicit SpeciationManager(SpeciationConfig config = {});

      std::vector<SpeciationEvent> update(std::vector<Animal>& animals, std::uint64_t tick);
      std::vector<SpeciesStats> stats(const std::vector<Animal>& animals) const;
      SpeciesId assign_child_species(const Genome& child, SpeciesId fallback, const std::vector<Animal>& animals) const;

private:
    struct SplitCandidate {
         std::vector<std::size_t> group_a;
         std::vector<std::size_t> group_b;
         Genome centroid_a{};
         Genome centroid_b{};
         double distance{0.0};
         bool valid{false};
    };

      SplitCandidate cluster_two(const std::vector<Animal>& animals,
                                 const std::vector<std::size_t>& members) const;

      SpeciationConfig config_;
      SpeciesId next_species_id_{2};
      std::unordered_map<SpeciesId, std::uint32_t> stable_counts_;
      std::unordered_map<SpeciesId, SpeciesId> parent_map_;
      std::unordered_map<SpeciesId, std::uint64_t> birth_tick_;
};

}    // namespace darwinsim

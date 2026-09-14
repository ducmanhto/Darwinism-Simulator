#pragma once

#include "darwinsim/genome.hpp"

#include <cstdint>

namespace darwinsim {

struct Phenotype {
    double mass{1.0};
    double max_speed{1.0};
    double endurance_factor{1.0};

      double attack_power{1.0};
      double defense{1.0};
      double max_health{100.0};

      double perception_radius{10.0};
      std::uint32_t sensor_samples{8};
      std::uint32_t requested_compute{64};
      std::uint32_t memory_slots{4};

      double basal_energy_cost{0.1};
      double plant_efficiency{0.5};
      double meat_efficiency{0.5};

      std::uint64_t maturity_age{100};
      double longevity_potential{1500.0};
};

struct PhenotypeConfig {
    double min_mass{0.5};
    double max_mass{8.0};
    double min_speed{0.25};
    double max_speed{4.0};
    double min_perception{4.0};
    double max_perception{28.0};
    std::uint32_t min_compute{64};
    std::uint32_t max_compute{1024};
    std::uint32_t min_memory_slots{4};
    std::uint32_t max_memory_slots{128};
    std::uint64_t min_maturity_age{80};
    std::uint64_t max_maturity_age{350};
    double baseline_longevity{1600.0};
};

Phenotype derive_phenotype(const Genome& genome, const PhenotypeConfig& config = {});

}    // namespace darwinsim

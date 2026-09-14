#pragma once

#include <array>
#include <cstddef>

namespace darwinsim {

struct Genome {
    float body_size{0.5F};
    float locomotion{0.5F};
    float endurance{0.5F};

      float compute_capacity{0.5F};
      float memory_capacity{0.5F};
      float sensory_capacity{0.5F};

      float aggression{0.5F};
      float sociality{0.5F};
      float curiosity{0.5F};
      float risk_tolerance{0.5F};

      float metabolic_efficiency{0.5F};
      float food_priority{0.5F};

      float reproductive_drive{0.5F};
      float mate_selectivity{0.5F};
      float communication{0.5F};

      float mutation_rate{0.35F};
      float mutation_scale{0.25F};

      float plant_digestion{0.7F};
      float meat_digestion{0.3F};

      float cellular_maintenance{0.5F};
      float recovery_efficiency{0.5F};
      float maturation_rate{0.5F};

      static constexpr std::size_t kGeneCount = 22;

      std::array<float, kGeneCount> as_array() const;
      static Genome from_array(const std::array<float, kGeneCount>& values);
};

float clamp_gene(float value);

}    // namespace darwinsim

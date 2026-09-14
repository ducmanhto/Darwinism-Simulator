#include "darwinsim/genome.hpp"

#include <algorithm>

namespace darwinsim {

float clamp_gene(float value) {
    return std::clamp(value, 0.0F, 1.0F);
}

 std::array<float, Genome::kGeneCount> Genome::as_array() const {
     return {
         body_size, locomotion, endurance,
         compute_capacity, memory_capacity, sensory_capacity,
         aggression, sociality, curiosity, risk_tolerance,
         metabolic_efficiency, food_priority,
         reproductive_drive, mate_selectivity, communication,
         mutation_rate, mutation_scale,
         plant_digestion, meat_digestion,
         cellular_maintenance, recovery_efficiency, maturation_rate
     };
}

Genome Genome::from_array(const std::array<float, kGeneCount>& v) {
    Genome g;
    std::size_t i = 0;
    g.body_size = clamp_gene(v[i++]);
    g.locomotion = clamp_gene(v[i++]);
    g.endurance = clamp_gene(v[i++]);
    g.compute_capacity = clamp_gene(v[i++]);
    g.memory_capacity = clamp_gene(v[i++]);
    g.sensory_capacity = clamp_gene(v[i++]);
    g.aggression = clamp_gene(v[i++]);
    g.sociality = clamp_gene(v[i++]);
    g.curiosity = clamp_gene(v[i++]);
    g.risk_tolerance = clamp_gene(v[i++]);
    g.metabolic_efficiency = clamp_gene(v[i++]);
    g.food_priority = clamp_gene(v[i++]);
    g.reproductive_drive = clamp_gene(v[i++]);
    g.mate_selectivity = clamp_gene(v[i++]);
    g.communication = clamp_gene(v[i++]);
    g.mutation_rate = clamp_gene(v[i++]);
    g.mutation_scale = clamp_gene(v[i++]);
    g.plant_digestion = clamp_gene(v[i++]);
    g.meat_digestion = clamp_gene(v[i++]);
    g.cellular_maintenance = clamp_gene(v[i++]);
    g.recovery_efficiency = clamp_gene(v[i++]);
    g.maturation_rate = clamp_gene(v[i++]);
    return g;
}

}   // namespace darwinsim

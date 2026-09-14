#include "darwinsim/phenotype.hpp"

#include <algorithm>
#include <cmath>

namespace darwinsim {

Phenotype derive_phenotype(const Genome& g, const PhenotypeConfig& c) {
    Phenotype p;

     const double size = static_cast<double>(g.body_size);
     p.mass = c.min_mass + (c.max_mass - c.min_mass) * size * size;

     const double locomotion = static_cast<double>(g.locomotion);
     p.max_speed = c.min_speed + (c.max_speed - c.min_speed) * locomotion * (1.0 - 0.30 * size);
     p.endurance_factor = 0.65 + 0.70 * static_cast<double>(g.endurance);

     // Offensive and defensive ability are emergent from size, locomotion,
     // endurance and aggression rather than separate "weapon" genes.
     p.attack_power = 1.2 * std::pow(p.mass, 0.60) * (0.65 + 0.70 * static_cast<double>(g.aggression));
     p.defense = 1.1 * std::pow(p.mass, 0.45) * (0.75 + 0.50 * static_cast<double>(g.endurance));
     p.max_health = 50.0 + 18.0 * p.mass + 25.0 * static_cast<double>(g.endurance);

     const double sensory = static_cast<double>(g.sensory_capacity);
     p.perception_radius = c.min_perception + (c.max_perception - c.min_perception) * std::sqrt(sensory);
     p.sensor_samples = static_cast<std::uint32_t>(8U + std::lround(56.0 * sensory));

     const double compute = std::pow(static_cast<double>(g.compute_capacity), 1.5);
     p.requested_compute = static_cast<std::uint32_t>(
         c.min_compute + std::lround((c.max_compute - c.min_compute) * compute));

     p.memory_slots = c.min_memory_slots + static_cast<std::uint32_t>(
         std::lround((c.max_memory_slots - c.min_memory_slots) * static_cast<double>(g.memory_capacity)));

     p.plant_efficiency = 0.20 + 0.80 * static_cast<double>(g.plant_digestion);
     p.meat_efficiency = 0.20 + 0.80 * static_cast<double>(g.meat_digestion);

     const double efficiency = 0.55 + 0.90 * static_cast<double>(g.metabolic_efficiency);
     const double brain_cost = 0.00045 * std::pow(static_cast<double>(p.requested_compute), 1.12);
     const double memory_cost = 0.00012 * static_cast<double>(p.memory_slots);
     const double maintenance_cost = 0.04 * std::pow(static_cast<double>(g.cellular_maintenance), 2.0) * p.mass;
     p.basal_energy_cost = (0.12 + 0.05 * std::pow(p.mass, 0.75) + brain_cost + memory_cost + maintenance_cost) / efficiency;

     const double maturation = static_cast<double>(g.maturation_rate);
     p.maturity_age = static_cast<std::uint64_t>(std::lround(
         static_cast<double>(c.max_maturity_age) -
         (static_cast<double>(c.max_maturity_age - c.min_maturity_age) * maturation)));

     // Longevity is a potential under ideal conditions, not a death deadline.
     const double maintenance = static_cast<double>(g.cellular_maintenance);
     p.longevity_potential = c.baseline_longevity * (0.65 + 0.90 * maintenance) * (1.05 - 0.25 * maturation);
     return p;
}

}   // namespace darwinsim

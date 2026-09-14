#include "darwinsim/decision.hpp"

#include "darwinsim/evolution.hpp"
#include "darwinsim/rng.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

 namespace darwinsim {
 namespace {

 struct Candidate {
     Action action;
     double utility;
 };

 Vec2 move_toward(Vec2 from, Vec2 to, double distance_to_move) {
     const double dx = to.x - from.x;
     const double dy = to.y - from.y;
     const double len = std::sqrt(dx * dx + dy * dy);
     if (len < 1e-9) return from;
     const double step = std::min(distance_to_move, len);
     return {from.x + dx / len * step, from.y + dy / len * step};
 }

 Vec2 move_away(Vec2 from, Vec2 threat, double distance_to_move) {
     const double dx = from.x - threat.x;
     const double dy = from.y - threat.y;
     const double len = std::sqrt(dx * dx + dy * dy);
     if (len < 1e-9) return {from.x + distance_to_move, from.y};
     return {from.x + dx / len * distance_to_move, from.y + dy / len * distance_to_move};
 }

 std::size_t compute_scan_limit(std::uint32_t granted_compute, std::size_t available) {
     const std::size_t logical_ops = std::max<std::size_t>(1, granted_compute / 24U);
     return std::min(available, logical_ops);
 }

 }    // namespace

 Action DecisionEngine::decide(const DecisionInput& in) {
     std::vector<Candidate> candidates;
     candidates.reserve(8);

       candidates.push_back({Action{in.animal_id, ActionType::Rest, 0, in.state.position, 0.05}, 0.05});

       const double energy_fraction = std::clamp(in.state.energy / 140.0, 0.0, 1.0);
       const double hunger = 1.0 - energy_fraction;
       const double risk_aversion = 1.05 - static_cast<double>(in.genome.risk_tolerance);
       const double aggression = static_cast<double>(in.genome.aggression);

       const std::size_t resource_limit = compute_scan_limit(in.granted_compute, in.resources.size());
       const ObservedResource* best_plant = nullptr;
       const ObservedResource* best_carcass = nullptr;
       double best_plant_score = -1e9;
       double best_carcass_score = -1e9;

       for (std::size_t i = 0; i < resource_limit; ++i) {
           const auto& r = in.resources[i];
           const double travel = r.distance / std::max(0.1, in.phenotype.max_speed);
           if (r.kind == ResourceKind::Plant) {
               const double score = hunger * in.genome.food_priority * r.energy * in.phenotype.plant_efficiency - 0.30 * travel;
               if (score > best_plant_score) {
                    best_plant_score = score;
                    best_plant = &r;
               }
           } else {
               const double score = hunger * in.genome.food_priority * r.energy * in.phenotype.meat_efficiency - 0.30 * travel;
               if (score > best_carcass_score) {
                    best_carcass_score = score;
                    best_carcass = &r;
               }
           }
       }

       if (best_plant) {
           const bool close = best_plant->distance <= 1.8;
           Action a;
           a.animal_id = in.animal_id;
           a.type = close ? ActionType::EatPlant : ActionType::Move;
           a.target_id = best_plant->id;
           a.destination = move_toward(in.state.position, best_plant->position, in.phenotype.max_speed);
           a.utility = best_plant_score;
           candidates.push_back({a, best_plant_score});
       }

       if (best_carcass) {
           const bool close = best_carcass->distance <= 1.8;
           Action a;
           a.animal_id = in.animal_id;
           a.type = close ? ActionType::EatCarcass : ActionType::Move;
           a.target_id = best_carcass->id;
           a.destination = move_toward(in.state.position, best_carcass->position, in.phenotype.max_speed);
           a.utility = best_carcass_score;
           candidates.push_back({a, best_carcass_score});
       }

      const std::size_t animal_limit = compute_scan_limit(in.granted_compute > 96 ? in.granted_compute - 96 : in.granted_compute,
                                                          in.animals.size());
      const ObservedAnimal* biggest_threat = nullptr;
double biggest_threat_score = -1.0;
const ObservedAnimal* attack_target = nullptr;
double best_attack_score = -1e9;
const ObservedAnimal* mate_target = nullptr;
double best_mate_score = -1e9;

for (std::size_t i = 0; i < animal_limit; ++i) {
    const auto& other = in.animals[i];
    if (other.id == in.animal_id) continue;

      const double danger = other.threat / std::max(0.25, in.phenotype.defense);
      if (danger > biggest_threat_score) {
          biggest_threat_score = danger;
          biggest_threat = &other;
      }

      if (in.granted_compute >= 256) {
          const double prey_value = in.phenotype.meat_efficiency * other.apparent_mass * 14.0;
          const double injury_risk = other.threat / (in.phenotype.attack_power + 0.25);
          const double attack_score = aggression * prey_value * 0.06 - risk_aversion * injury_risk - 0.10 * other.distance;
          if (attack_score > best_attack_score) {
              best_attack_score = attack_score;
              attack_target = &other;
          }
      }

       if (in.granted_compute >= 320 && other.mature && in.state.age >= in.phenotype.maturity_age && in.state.reproduction_cooldown == 0) {
           const double related_species_penalty = (other.species_id == in.species_id) ? 0.0 : 0.12;
           const double score = static_cast<double>(in.genome.reproductive_drive) * energy_fraction
                              - 0.08 * other.distance - related_species_penalty;
           if (score > best_mate_score) {
               best_mate_score = score;
               mate_target = &other;
           }
       }
}

if (biggest_threat && biggest_threat_score > 1.1) {
    const double flee_utility = risk_aversion * biggest_threat_score + 0.25 * (1.0 - energy_fraction);
    Action a;
    a.animal_id = in.animal_id;
    a.type = ActionType::Flee;
    a.target_id = biggest_threat->id;
    a.destination = move_away(in.state.position, biggest_threat->position, in.phenotype.max_speed);
    a.utility = flee_utility;
    candidates.push_back({a, flee_utility});
}

if (attack_target && best_attack_score > 0.0) {
    Action a;
    a.animal_id = in.animal_id;
    a.type = attack_target->distance <= 1.8 ? ActionType::Attack : ActionType::Move;
    a.target_id = attack_target->id;
    a.destination = move_toward(in.state.position, attack_target->position, in.phenotype.max_speed);
    a.utility = best_attack_score;
    candidates.push_back({a, best_attack_score});
}

if (mate_target && best_mate_score > 0.0) {
    Action a;
    a.animal_id = in.animal_id;
    a.type = mate_target->distance <= 1.8 ? ActionType::Mate : ActionType::Move;
    a.target_id = mate_target->id;
    a.destination = move_toward(in.state.position, mate_target->position, in.phenotype.max_speed);
    a.utility = best_mate_score;
    candidates.push_back({a, best_mate_score});
}

if (in.granted_compute >= 128) {
    const double angle = DeterministicRng::uniform(in.seed, in.animal_id, in.tick, RandomDomain::Movement,
                                                   0.0, 6.283185307179586, 0);
    const double explore_u = 0.15 + 0.55 * static_cast<double>(in.genome.curiosity) * (1.0 - hunger * 0.6);
    Action a;
    a.animal_id = in.animal_id;
    a.type = ActionType::Explore;
    a.destination = {in.state.position.x + std::cos(angle) * in.phenotype.max_speed,
                     in.state.position.y + std::sin(angle) * in.phenotype.max_speed};
    a.utility = explore_u;
    candidates.push_back({a, explore_u});
}

// Softmax selection gives stochastic behavior while remaining reproducible.
const double temperature = 0.55 + 0.70 * static_cast<double>(in.genome.curiosity);
double max_u = -std::numeric_limits<double>::infinity();
for (const auto& c : candidates) max_u = std::max(max_u, c.utility);

     std::vector<double> weights;
     weights.reserve(candidates.size());
     double total = 0.0;
     for (const auto& c : candidates) {
         const double w = std::exp((c.utility - max_u) / temperature);
         weights.push_back(w);
         total += w;
     }

     const double draw = DeterministicRng::uniform01(in.seed, in.animal_id, in.tick, RandomDomain::Decision) * total;
     double cumulative = 0.0;
     for (std::size_t i = 0; i < candidates.size(); ++i) {
         cumulative += weights[i];
         if (draw <= cumulative) return candidates[i].action;
     }
     return candidates.back().action;
}

}   // namespace darwinsim

#include "darwinsim/evolution.hpp"

#include "darwinsim/rng.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>

 namespace darwinsim {
 namespace {

 Genome mean_genome(const std::vector<Animal>& animals, const std::vector<std::size_t>& members) {
     std::array<double, Genome::kGeneCount> sums{};
     if (members.empty()) return {};
     for (auto index : members) {
         const auto genes = animals[index].genome.as_array();
         for (std::size_t i = 0; i < genes.size(); ++i) sums[i] += genes[i];
     }
     std::array<float, Genome::kGeneCount> mean{};
     for (std::size_t i = 0; i < mean.size(); ++i) {
         mean[i] = static_cast<float>(sums[i] / static_cast<double>(members.size()));
     }
     return Genome::from_array(mean);
 }

 float reflect_gene(double value) {
     while (value < 0.0 || value > 1.0) {
         if (value < 0.0) value = -value;
         if (value > 1.0) value = 2.0 - value;
     }
     return clamp_gene(static_cast<float>(value));
 }

 }   // namespace

 double genetic_distance(const Genome& a, const Genome& b) {
     const auto ga = a.as_array();
     const auto gb = b.as_array();
     double sum = 0.0;
     for (std::size_t i = 0; i < ga.size(); ++i) {
         // Give core physiology/behavior slightly more weight than mutation-control genes.
         const double weight = (i == 15 || i == 16) ? 0.5 : 1.0;
         const double d = static_cast<double>(ga[i] - gb[i]);
         sum += weight * d * d;
     }
     return std::sqrt(sum / static_cast<double>(Genome::kGeneCount));
 }

 double reproductive_compatibility(const Genome& a, const Genome& b, double tau) {
     const double d = genetic_distance(a, b);
     const double ratio = d / std::max(1e-6, tau);
     return std::exp(-(ratio * ratio));
 }

 Genome crossover_and_mutate(const Genome& a,
                             const Genome& b,
                             std::uint64_t seed,
                             AnimalId child_id,
                             std::uint64_t tick) {
     const auto ga = a.as_array();
     const auto gb = b.as_array();
     std::array<float, Genome::kGeneCount> child{};

     const double parent_rate = 0.5 * (static_cast<double>(a.mutation_rate) + static_cast<double>(b.mutation_rate));
     const double parent_scale = 0.5 * (static_cast<double>(a.mutation_scale) + static_cast<double>(b.mutation_scale));
     const double mutation_probability = 0.005 + 0.045 * parent_rate;
     const double sigma = 0.005 + 0.045 * parent_scale;

     for (std::size_t i = 0; i < child.size(); ++i) {
         const double alpha = DeterministicRng::uniform(seed, child_id, tick, RandomDomain::Crossover, 0.25, 0.75, i);
         double value = alpha * static_cast<double>(ga[i]) + (1.0 - alpha) * static_cast<double>(gb[i]);
         const double mutate = DeterministicRng::uniform01(seed, child_id, tick, RandomDomain::Mutation, i * 3U);
         if (mutate < mutation_probability) {
             value += DeterministicRng::normal(seed, child_id, tick, RandomDomain::Mutation, 0.0, sigma, i * 3U + 1U);
         }
         child[i] = reflect_gene(value);
     }
     return Genome::from_array(child);
 }

 SpeciationManager::SpeciationManager(SpeciationConfig config) : config_(config) {
     parent_map_[1] = 0;
     birth_tick_[1] = 0;
 }

 SpeciationManager::SplitCandidate SpeciationManager::cluster_two(
     const std::vector<Animal>& animals,
     const std::vector<std::size_t>& members) const {
     SplitCandidate result;
     if (members.size() < config_.min_subgroup_population * 2U) return result;

     Genome c1 = animals[members.front()].genome;
     std::size_t farthest = members.front();
     double farthest_d = -1.0;
     for (auto idx : members) {
         const double d = genetic_distance(c1, animals[idx].genome);
         if (d > farthest_d) {
            farthest_d = d;
            farthest = idx;
        }
    }
    Genome c2 = animals[farthest].genome;

    for (int iteration = 0; iteration < 8; ++iteration) {
        result.group_a.clear();
        result.group_b.clear();
        for (auto idx : members) {
            const double d1 = genetic_distance(animals[idx].genome, c1);
            const double d2 = genetic_distance(animals[idx].genome, c2);
            (d1 <= d2 ? result.group_a : result.group_b).push_back(idx);
        }
        if (result.group_a.empty() || result.group_b.empty()) return {};
        c1 = mean_genome(animals, result.group_a);
        c2 = mean_genome(animals, result.group_b);
    }

    result.centroid_a = c1;
    result.centroid_b = c2;
    result.distance = genetic_distance(c1, c2);
    result.valid = result.group_a.size() >= config_.min_subgroup_population &&
                   result.group_b.size() >= config_.min_subgroup_population &&
                   result.distance >= config_.split_threshold;
    return result;
}

std::vector<SpeciationEvent> SpeciationManager::update(std::vector<Animal>& animals, std::uint64_t tick) {
    std::vector<SpeciationEvent> events;
    if (config_.interval_ticks == 0 || tick == 0 || tick % config_.interval_ticks != 0) return events;

    std::unordered_map<SpeciesId, std::vector<std::size_t>> by_species;
    for (std::size_t i = 0; i < animals.size(); ++i) {
        if (animals[i].state.alive) by_species[animals[i].species_id].push_back(i);
        next_species_id_ = std::max(next_species_id_, static_cast<SpeciesId>(animals[i].species_id + 1));
    }

    for (auto& [species, members] : by_species) {
        auto candidate = cluster_two(animals, members);
        if (!candidate.valid) {
            stable_counts_[species] = 0;
            continue;
        }

        auto& stable = stable_counts_[species];
        ++stable;
         if (stable < config_.stability_checks) continue;
         stable = 0;

         // Keep the larger cluster in the parent species to reduce churn.
         auto* moved = &candidate.group_b;
         if (candidate.group_b.size() > candidate.group_a.size()) moved = &candidate.group_a;

         const SpeciesId child_species = next_species_id_++;
         for (auto idx : *moved) animals[idx].species_id = child_species;
         parent_map_[child_species] = species;
         birth_tick_[child_species] = tick;

         events.push_back({species, child_species, moved->size(), candidate.distance});
     }
     return events;
}

std::vector<SpeciesStats> SpeciationManager::stats(const std::vector<Animal>& animals) const {
    std::unordered_map<SpeciesId, std::vector<std::size_t>> by_species;
    for (std::size_t i = 0; i < animals.size(); ++i) {
        if (animals[i].state.alive) by_species[animals[i].species_id].push_back(i);
    }

     std::vector<SpeciesStats> out;
     out.reserve(by_species.size());
     for (const auto& [species, members] : by_species) {
         SpeciesStats s;
         s.id = species;
         s.population = members.size();
         s.centroid = mean_genome(animals, members);
         const auto parent_it = parent_map_.find(species);
         s.parent_species = parent_it == parent_map_.end() ? 0 : parent_it->second;
         const auto birth_it = birth_tick_.find(species);
         s.birth_tick = birth_it == birth_tick_.end() ? 0 : birth_it->second;
         double diversity = 0.0;
         for (auto idx : members) diversity += genetic_distance(animals[idx].genome, s.centroid);
         s.diversity = members.empty() ? 0.0 : diversity / static_cast<double>(members.size());
         out.push_back(s);
     }
     std::sort(out.begin(), out.end(), [](const SpeciesStats& a, const SpeciesStats& b) { return a.id < b.id; });
     return out;
}

SpeciesId SpeciationManager::assign_child_species(const Genome& child,
                                                   SpeciesId fallback,
                                                   const std::vector<Animal>& animals) const {
    const auto current = stats(animals);
    SpeciesId best = fallback;
    double best_d = std::numeric_limits<double>::infinity();
    for (const auto& s : current) {
        const double d = genetic_distance(child, s.centroid);
        if (d < best_d) {
            best_d = d;
            best = s.id;
        }
    }
    return best_d <= config_.join_threshold ? best : fallback;
}

}   // namespace darwinsim

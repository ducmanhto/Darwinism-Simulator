#include "darwinsim/simulation.hpp"

#include "darwinsim/rng.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <unordered_map>

namespace darwinsim {
namespace {

Genome random_genome(std::uint64_t seed, AnimalId id) {
    Genome base;
    auto values = base.as_array();
    for (std::size_t i = 0; i < values.size(); ++i) {
        // Start populations near the middle but genetically diverse.
        const double v = DeterministicRng::normal(seed, id, 0, RandomDomain::Initialization, 0.5, 0.16, i);
        values[i] = clamp_gene(static_cast<float>(v));
    }
    // Favor plants initially so the ecosystem has a stable energy source.
    values[17] = clamp_gene(static_cast<float>(0.65 + 0.25 * DeterministicRng::uniform01(
        seed, id, 0, RandomDomain::Initialization, 100)));
    values[18] = clamp_gene(static_cast<float>(0.15 + 0.45 * DeterministicRng::uniform01(
          seed, id, 0, RandomDomain::Initialization, 101)));
      return Genome::from_array(values);
 }

std::uint64_t action_priority(std::uint64_t seed, std::uint64_t tick, AnimalId id) {
    return DeterministicRng::hash(seed, id, tick, RandomDomain::ResolutionOrder);
}

}   // namespace

Simulation::Simulation(SimulationConfig config)
    : config_(config),
    world_(config.world),
    grid_(config.world.width, config.world.height, 16.0),
    speciation_(config.speciation) {}

Animal Simulation::make_initial_animal(AnimalId id) const {
    Animal animal;
    animal.id = id;
    animal.species_id = 1;
    animal.genome = random_genome(config_.seed, id);
    animal.phenotype = derive_phenotype(animal.genome, config_.phenotype);
    animal.state.position.x = DeterministicRng::uniform(config_.seed, id, 0, RandomDomain::Initialization,
                                                        0.0, config_.world.width, 200);
    animal.state.position.y = DeterministicRng::uniform(config_.seed, id, 0, RandomDomain::Initialization,
                                                        0.0, config_.world.height, 201);
    animal.state.energy = config_.initial_energy;
    animal.state.health = animal.phenotype.max_health;
    animal.state.alive = true;
    return animal;
}

void Simulation::initialize() {
    tick_ = 0;
    births_ = 0;
    deaths_ = 0;
    speciation_event_count_ = 0;
    combat_deaths_ = 0;
    starvation_deaths_ = 0;
    aging_deaths_ = 0;
    unknown_deaths_ = 0;
    combat_damage_total_ = 0.0;
    starvation_damage_total_ = 0.0;
    aging_damage_total_ = 0.0;

    prepared_ = false;
    next_animal_id_ = 1;
    animals_.clear();
    animals_.reserve(config_.max_population);
    world_.initialize(config_.seed);
    for (std::uint32_t i = 0; i < config_.initial_population; ++i) {
        animals_.push_back(make_initial_animal(next_animal_id_++));
    }
}

DecisionInput Simulation::build_observation(const Animal& animal, std::uint32_t granted_compute) const {
    DecisionInput input;
    input.tick = tick_;
    input.seed = config_.seed;
    input.animal_id = animal.id;
    input.species_id = animal.species_id;
    input.genome = animal.genome;
    input.phenotype = animal.phenotype;
    input.state = animal.state;
    input.granted_compute = granted_compute;

    const double radius = animal.phenotype.perception_radius;
    const std::size_t sample_limit = animal.phenotype.sensor_samples;

    auto plant_indices = grid_.nearby_plants(animal.state.position, radius);
    for (auto index : plant_indices) {
        const auto& plant = world_.plants()[index];
        const double d = distance(animal.state.position, plant.position);
        if (d <= radius && plant.available) {
            input.resources.push_back({plant.id, ResourceKind::Plant, plant.position, d, plant.energy});
        }
    }

    auto carcass_indices = grid_.nearby_carcasses(animal.state.position, radius);
    for (auto index : carcass_indices) {
        const auto& carcass = world_.carcasses()[index];
        const double d = distance(animal.state.position, carcass.position);
        if (d <= radius && carcass.available) {
            input.resources.push_back({carcass.id, ResourceKind::Carcass, carcass.position, d, carcass.energy});
        }
    }

    std::sort(input.resources.begin(), input.resources.end(), [](const auto& a, const auto& b) {
        return a.distance < b.distance;
    });
    if (input.resources.size() > sample_limit) input.resources.resize(sample_limit);

    auto animal_indices = grid_.nearby_animals(animal.state.position, radius);
    for (auto index : animal_indices) {
        const auto& other = animals_[index];
        if (!other.state.alive || other.id == animal.id) continue;
        const double d = distance(animal.state.position, other.state.position);
        if (d > radius) continue;
        ObservedAnimal observed;
        observed.id = other.id;
        observed.species_id = other.species_id;
        observed.position = other.state.position;
        observed.distance = d;
        observed.apparent_mass = other.phenotype.mass;
        observed.health_fraction = std::clamp(other.state.health / std::max(1.0, other.phenotype.max_health), 0.0, 1.0);
        observed.threat = other.phenotype.attack_power * (0.5 + 0.5 * observed.health_fraction);
        observed.mature = other.state.age >= other.phenotype.maturity_age && other.state.reproduction_cooldown == 0;
        input.animals.push_back(observed);
    }
    std::sort(input.animals.begin(), input.animals.end(), [](const auto& a, const auto& b) {
        return a.distance < b.distance;
    });
    if (input.animals.size() > sample_limit) input.animals.resize(sample_limit);

    return input;
}

std::vector<DecisionInput> Simulation::prepare_tick(std::uint64_t available_compute_credits) {
    if (prepared_) return {};
    world_.tick_resources(config_.seed, tick_);
    grid_.rebuild(animals_, world_.plants(), world_.carcasses());

    std::uint64_t capacity = available_compute_credits;
    if (capacity == 0) {
        if (config_.hardware_mode == HardwareMode::Coupled) {
            capacity = read_cgroup_capacity(config_.credits_per_core).compute_credits_per_tick;
        } else {
            capacity = config_.deterministic_compute_credits;
        }
    }

    std::uint64_t requested = 0;
    for (const auto& animal : animals_) {
        if (animal.state.alive) requested += animal.phenotype.requested_compute;
    }
    const double scale = requested == 0 ? 1.0 : std::min(1.0, static_cast<double>(capacity) / static_cast<double>(requested));

    std::vector<DecisionInput> inputs;
    inputs.reserve(animals_.size());
    for (const auto& animal : animals_) {
        if (!animal.state.alive) continue;
        const auto granted = static_cast<std::uint32_t>(std::max(32.0, animal.phenotype.requested_compute * scale));
        inputs.push_back(build_observation(animal, granted));
    }
    prepared_ = true;
    return inputs;
}

Animal* Simulation::find_animal(AnimalId id) {
    for (auto& animal : animals_) if (animal.id == id) return &animal;
    return nullptr;
}

const Animal* Simulation::find_animal(AnimalId id) const {
    for (const auto& animal : animals_) if (animal.id == id) return &animal;
    return nullptr;
}

void Simulation::resolve_action(const Action& action,
                                std::vector<std::pair<AnimalId, AnimalId>>& mate_requests) {
    Animal* animal = find_animal(action.animal_id);
    if (!animal || !animal->state.alive) return;

    switch (action.type) {
        case ActionType::Rest:
            animal->state.energy = std::min(config_.max_energy, animal->state.energy + 0.05);
            break;
        case ActionType::Move:
        case ActionType::Flee:
        case ActionType::Explore: {
            const Vec2 destination = clamp_position(action.destination, config_.world.width, config_.world.height);
            const double d = distance(animal->state.position, destination);
            const double terrain_cost = world_.movement_cost_multiplier(animal->state.position, destination);
            const double cost = config_.movement_energy_per_mass * animal->phenotype.mass * d * terrain_cost /
                                std::max(0.5, animal->phenotype.endurance_factor);
            if (animal->state.energy > cost) {
                animal->state.position = destination;
                animal->state.energy -= cost;
            }
            break;
        }
        case ActionType::EatPlant: {
            for (auto& plant : world_.plants()) {
                if (plant.id != action.target_id || !plant.available) continue;
                if (distance(animal->state.position, plant.position) <= config_.world.interaction_distance) {
                    const double gain = plant.energy * animal->phenotype.plant_efficiency;
                    animal->state.energy = std::min(config_.max_energy, animal->state.energy + gain);
                    plant.available = false;
                }
                break;
            }
            break;
        }
        case ActionType::EatCarcass: {
            for (auto& carcass : world_.carcasses()) {
                if (carcass.id != action.target_id || !carcass.available) continue;
                if (distance(animal->state.position, carcass.position) <= config_.world.interaction_distance) {
                    const double bite = std::min(24.0, carcass.energy);
                    const double gain = bite * animal->phenotype.meat_efficiency;
                    animal->state.energy = std::min(config_.max_energy, animal->state.energy + gain);
                    carcass.energy -= bite;
                    if (carcass.energy <= 0.0) carcass.available = false;
                }
                break;
            }
            break;
        }
        case ActionType::Attack: {
            Animal* target = find_animal(action.target_id);
            if (!target || !target->state.alive) break;
            if (distance(animal->state.position, target->state.position) > config_.world.interaction_distance) break;
            if (animal->state.energy <= config_.attack_energy_cost) break;
            animal->state.energy -= config_.attack_energy_cost;

            const double advantage = animal->phenotype.attack_power /
                                     std::max(0.25, target->phenotype.defense);
            const double hit_probability = std::clamp(0.25 + 0.45 * advantage / (1.0 + advantage), 0.15, 0.85);
            const double hit_draw = DeterministicRng::uniform01(config_.seed, animal->id, tick_, RandomDomain::Combat, target->id);
            if (hit_draw < hit_probability) {
                const double variation = DeterministicRng::uniform(config_.seed, animal->id, tick_, RandomDomain::Combat,
                                                                   0.90, 1.10, target->id + 1);
                const double damage = 3.5 * advantage * variation;
                const double applied_damage = std::min(damage, std::max(0.0, target->state.health));
                combat_damage_total_ += applied_damage;

                target->state.health -= damage;
                if (target->state.health <= 0.0)
                    mark_dead(*target, DeathCause::Combat);
            }
            break;
        }
        case ActionType::Mate:
            mate_requests.emplace_back(action.animal_id, action.target_id);
            break;
    }
}

void Simulation::resolve_mating(const std::vector<std::pair<AnimalId, AnimalId>>& requests) {
    std::set<std::pair<AnimalId, AnimalId>> request_set(requests.begin(), requests.end());
    std::set<std::pair<AnimalId, AnimalId>> handled;
    std::vector<Animal> newborns;

    for (const auto& [a_id, b_id] : requests) {
        if (!request_set.contains({b_id, a_id})) continue;    // mutual choice
        const auto normalized = std::minmax(a_id, b_id);
        if (!handled.insert(normalized).second) continue;

        Animal* a = find_animal(a_id);
        Animal* b = find_animal(b_id);
        if (!a || !b || !a->state.alive || !b->state.alive) continue;
        if (a->state.age < a->phenotype.maturity_age || b->state.age < b->phenotype.maturity_age) continue;
        if (a->state.reproduction_cooldown > 0 || b->state.reproduction_cooldown > 0) continue;
        if (a->state.energy < config_.reproduction_min_energy || b->state.energy < config_.reproduction_min_energy) continue;
        if (distance(a->state.position, b->state.position) > config_.world.interaction_distance) continue;

        const double compatibility = reproductive_compatibility(a->genome, b->genome);
        const double selectivity = 0.5 * (a->genome.mate_selectivity + b->genome.mate_selectivity);
        const double accept_probability = std::pow(compatibility, 1.0 + 2.0 * selectivity);
        const double draw = DeterministicRng::uniform01(config_.seed, next_animal_id_, tick_, RandomDomain::MateChoice);
        if (draw > accept_probability) continue;
        if (animals_.size() + newborns.size() >= config_.max_population) break;

        Animal child;
        child.id = next_animal_id_++;
        child.genome = crossover_and_mutate(a->genome, b->genome, config_.seed, child.id, tick_);
        child.phenotype = derive_phenotype(child.genome, config_.phenotype);
        child.species_id = speciation_.assign_child_species(child.genome, a->species_id, animals_);
        child.state.position = {(a->state.position.x + b->state.position.x) * 0.5,
                                (a->state.position.y + b->state.position.y) * 0.5};
        child.state.energy = config_.reproduction_contribution * 1.5;
        child.state.health = child.phenotype.max_health;
        child.lineage.parent_a = a->id;
        child.lineage.parent_b = b->id;
        child.lineage.generation = std::max(a->lineage.generation, b->lineage.generation) + 1;

        a->state.energy -= config_.reproduction_contribution;
        b->state.energy -= config_.reproduction_contribution;
        a->state.reproduction_cooldown = config_.reproduction_cooldown;
        b->state.reproduction_cooldown = config_.reproduction_cooldown;
        newborns.push_back(std::move(child));
        ++births_;
    }

    animals_.insert(animals_.end(), newborns.begin(), newborns.end());
}

void Simulation::mark_dead(Animal& animal, DeathCause cause) {
    if (!animal.state.alive) return;

    animal.state.alive = false;
    animal.state.health = 0.0;

    switch (cause) {
    case DeathCause::Combat:
        ++combat_deaths_;
        break;
    case DeathCause::Starvation:
        ++starvation_deaths_;
        break;
    case DeathCause::Aging:
        ++aging_deaths_;
        break;
    case DeathCause::Unknown:
    default:
        ++unknown_deaths_;
        break;
    }

    const double carcass_energy =
        std::max(5.0, animal.phenotype.mass * 18.0 + std::max(0.0, animal.state.energy) * 0.25);

    world_.add_carcass(
        animal.id,
        animal.state.position,
        carcass_energy,
        tick_);

    ++deaths_;
}

void Simulation::advance_metabolism() {
    for (auto& animal : animals_) {
        if (!animal.state.alive) continue;

        ++animal.state.age;

        if (animal.state.reproduction_cooldown > 0)
            --animal.state.reproduction_cooldown;

        DeathCause strongest_cause = DeathCause::Unknown;
        double strongest_damage = 0.0;

        auto record_damage = [&](DeathCause cause, double damage) {
            if (damage > strongest_damage) {
                strongest_damage = damage;
                strongest_cause = cause;
            }
        };

        // Energy expenditure
        const double brain_cost =
            config_.brain_energy_per_credit *
            animal.phenotype.requested_compute;

        animal.state.energy -=
            animal.phenotype.basal_energy_cost + brain_cost;

        // Normal aging pressure
        const double age_ratio =
            static_cast<double>(animal.state.age) /
            std::max(1.0, animal.phenotype.longevity_potential);

        const double aging_pressure =
            std::pow(
                std::max(0.0, age_ratio),
                config_.aging_exponent) *
            (1.0 - 0.45 * animal.genome.cellular_maintenance);

        if (aging_pressure > 0.02) {
            const double damage = aging_pressure * 0.15;
            const double applied_damage = std::min(damage, std::max(0.0, animal.state.health));
            aging_damage_total_ += applied_damage;

            animal.state.health -= damage;
            record_damage(DeathCause::Aging, damage);
        }

        // Starvation
        if (animal.state.energy < 0.0) {
            const double damage = config_.starvation_damage;
            const double applied_damage = std::min(damage, std::max(0.0, animal.state.health));

            starvation_damage_total_ += applied_damage;
            animal.state.health -= damage;
            animal.state.energy = 0.0;

            record_damage(DeathCause::Starvation, damage);
        } else {
            const double recovery =
                0.02 * animal.genome.recovery_efficiency;

            animal.state.health =
                std::min(
                    animal.phenotype.max_health,
                    animal.state.health + recovery);
        }

        // Severe old-age pressure
        const double hard_age =
            animal.phenotype.longevity_potential *
            config_.soft_death_age_multiplier;

        if (static_cast<double>(animal.state.age) > hard_age) {
            const double damage =
                0.4 +
                0.8 *
                (static_cast<double>(animal.state.age) /
                     hard_age -
                 1.0);
            const double applied_damage = std::min(damage, std::max(0.0, animal.state.health));
            aging_damage_total_ += applied_damage;

            animal.state.health -= damage;
            record_damage(DeathCause::Aging, damage);
        }

        // Keep the original single death check at the end.
        if (animal.state.health <= 0.0) {
            mark_dead(animal, strongest_cause);
        }
    }
}

void Simulation::apply_actions(std::vector<Action> actions) {
    if (!prepared_) return;

    std::sort(actions.begin(), actions.end(), [this](const Action& a, const Action& b) {
        const auto pa = action_priority(config_.seed, tick_, a.animal_id);
        const auto pb = action_priority(config_.seed, tick_, b.animal_id);
        if (pa != pb) return pa > pb;
        return a.animal_id < b.animal_id;
    });

    std::vector<std::pair<AnimalId, AnimalId>> mate_requests;
    for (const auto& action : actions) resolve_action(action, mate_requests);
    resolve_mating(mate_requests);
    advance_metabolism();

    const auto events = speciation_.update(animals_, tick_ + 1);
    speciation_event_count_ += events.size();

    ++tick_;
    prepared_ = false;
}

void Simulation::step_local() {
    auto inputs = prepare_tick();
    std::vector<Action> actions;
    actions.reserve(inputs.size());
    for (const auto& input : inputs) actions.push_back(DecisionEngine::decide(input));
    apply_actions(std::move(actions));
}

SimulationSummary Simulation::summary() const {
    SimulationSummary s;
    s.tick = tick_;
    s.total_animals_created = animals_.size();
    s.births = births_;
    s.deaths = deaths_;
    s.combat_deaths = combat_deaths_;
    s.starvation_deaths = starvation_deaths_;
    s.aging_deaths = aging_deaths_;
    s.unknown_deaths = unknown_deaths_;
    s.combat_damage_total = combat_damage_total_;
    s.starvation_damage_total = starvation_damage_total_;
    s.aging_damage_total = aging_damage_total_;
    s.speciation_events = speciation_event_count_;
    s.species_count = species_stats().size();
    for (const auto& a : animals_) if (a.state.alive) ++s.living_animals;
    for (const auto& p : world_.plants()) if (p.available) ++s.available_plants;
    for (const auto& c : world_.carcasses()) if (c.available) ++s.carcasses;
    return s;
}

std::vector<SpeciesStats> Simulation::species_stats() const {
    return speciation_.stats(animals_);
}

void Simulation::restore_counters(std::uint64_t tick,
                                  std::uint64_t births,
                                  std::uint64_t deaths,
                                  std::uint64_t speciation_events,
                                  AnimalId next_animal_id) {
    tick_ = tick;
    births_ = births;
    deaths_ = deaths;
    speciation_event_count_ = speciation_events;
    next_animal_id_ = next_animal_id;
    prepared_ = false;
}

}   // namespace darwinsim

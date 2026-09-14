#pragma once

#include "darwinsim/animal.hpp"
#include "darwinsim/decision.hpp"
#include "darwinsim/evolution.hpp"
#include "darwinsim/hardware.hpp"
#include "darwinsim/spatial_grid.hpp"
#include "darwinsim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace darwinsim {

struct SimulationConfig {
    std::uint64_t seed{12345};
    std::uint32_t initial_population{300};
    std::uint32_t max_population{5000};
    std::uint64_t ticks_per_checkpoint{1000};

    WorldConfig world{};
    PhenotypeConfig phenotype{};
    SpeciationConfig speciation{};

    HardwareMode hardware_mode{HardwareMode::Deterministic};
    std::uint64_t deterministic_compute_credits{250000};
    std::uint64_t credits_per_core{250000};

    double initial_energy{110.0};
    double max_energy{180.0};
    double reproduction_min_energy{90.0};
    double reproduction_contribution{24.0};
    std::uint64_t reproduction_cooldown{180};
    double attack_energy_cost{1.2};
    double movement_energy_per_mass{0.025};
    double brain_energy_per_credit{0.000002};
    double starvation_damage{0.8};
    double aging_exponent{3.0};
    double soft_death_age_multiplier{1.5};
};

struct SimulationSummary {
    std::uint64_t tick{0};
    std::size_t living_animals{0};
    std::size_t total_animals_created{0};
    std::size_t species_count{0};
    std::size_t available_plants{0};
    std::size_t carcasses{0};

    std::uint64_t births{0};
    std::uint64_t deaths{0};
    std::uint64_t speciation_events{0};

    std::uint64_t combat_deaths{0};
    std::uint64_t starvation_deaths{0};
    std::uint64_t aging_deaths{0};
    std::uint64_t unknown_deaths{0};

    double combat_damage_total{0.0};
    double starvation_damage_total{0.0};
    double aging_damage_total{0.0};
};

enum class DeathCause : std::uint8_t {
    Unknown = 0,
    Combat,
    Starvation,
    Aging
};

class Simulation {
    public:
        explicit Simulation(SimulationConfig config = {});

        void initialize();
        std::vector<DecisionInput> prepare_tick(std::uint64_t available_compute_credits = 0);
        void apply_actions(std::vector<Action> actions);
        void step_local();

        const SimulationConfig& config() const { return config_; }
        const std::vector<Animal>& animals() const { return animals_; }
        std::vector<Animal>& animals_mutable() { return animals_; }
        const World& world() const { return world_; }
        World& world_mutable() { return world_; }
        std::uint64_t tick() const { return tick_; }

        SimulationSummary summary() const;
        std::vector<SpeciesStats> species_stats() const;

        std::uint64_t births() const { return births_; }
        std::uint64_t deaths() const { return deaths_; }
        std::uint64_t speciation_event_count() const { return speciation_event_count_; }

        void restore_counters(std::uint64_t tick,
                                std::uint64_t births,
                                std::uint64_t deaths,
                                std::uint64_t speciation_events,
                                AnimalId next_animal_id);

    private:
        Animal make_initial_animal(AnimalId id) const;
        DecisionInput build_observation(const Animal& animal, std::uint32_t granted_compute) const;
        void resolve_action(const Action& action, std::vector<std::pair<AnimalId, AnimalId>>& mate_requests);
        void resolve_mating(const std::vector<std::pair<AnimalId, AnimalId>>& mate_requests);
        void advance_metabolism();
        void mark_dead(Animal& animal, DeathCause cause);
        Animal* find_animal(AnimalId id);
        const Animal* find_animal(AnimalId id) const;

        SimulationConfig config_;
        World world_;
        SpatialGrid grid_;
        SpeciationManager speciation_;
        DecisionEngine decision_engine_;

        std::vector<Animal> animals_;
        std::uint64_t tick_{0};
        AnimalId next_animal_id_{1};
        std::uint64_t births_{0};
        std::uint64_t deaths_{0};
        std::uint64_t speciation_event_count_{0};
        std::uint64_t combat_deaths_{0};
        std::uint64_t starvation_deaths_{0};
        std::uint64_t aging_deaths_{0};
        std::uint64_t unknown_deaths_{0};

        double combat_damage_total_{0.0};
        double starvation_damage_total_{0.0};
        double aging_damage_total_{0.0};

        bool prepared_{false};
    };

}    // namespace darwinsim

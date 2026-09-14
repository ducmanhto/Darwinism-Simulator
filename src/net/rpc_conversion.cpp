#include "darwinsim/rpc_conversion.hpp"

#include <algorithm>
#include <array>

namespace darwinsim {
namespace {

rpc::ActionType to_rpc_action_type(ActionType type) {
    switch (type) {
        case ActionType::Rest: return rpc::REST;
        case ActionType::Move: return rpc::MOVE;
        case ActionType::EatPlant: return rpc::EAT_PLANT;
        case ActionType::EatCarcass: return rpc::EAT_CARCASS;
        case ActionType::Flee: return rpc::FLEE;
        case ActionType::Attack: return rpc::ATTACK;
        case ActionType::Mate: return rpc::MATE;
        case ActionType::Explore: return rpc::EXPLORE;
    }
    return rpc::REST;
}

ActionType from_rpc_action_type(rpc::ActionType type) {
    switch (type) {
        case rpc::REST: return ActionType::Rest;
        case rpc::MOVE: return ActionType::Move;
        case rpc::EAT_PLANT: return ActionType::EatPlant;
        case rpc::EAT_CARCASS: return ActionType::EatCarcass;
        case rpc::FLEE: return ActionType::Flee;
        case rpc::ATTACK: return ActionType::Attack;
        case rpc::MATE: return ActionType::Mate;
        case rpc::EXPLORE: return ActionType::Explore;
        default: return ActionType::Rest;
    }
}

void fill_vec(rpc::Vec2* out, Vec2 v) {
    out->set_x(v.x);
    out->set_y(v.y);
}

Vec2 read_vec(const rpc::Vec2& v) {
    return {v.x(), v.y()};
}

}   // namespace

rpc::DecisionInput to_rpc(const DecisionInput& in) {
     rpc::DecisionInput out;
     out.set_tick(in.tick);
     out.set_seed(in.seed);
     out.set_animal_id(in.animal_id);
     out.set_species_id(in.species_id);
     for (float gene : in.genome.as_array()) out.mutable_genome()->add_genes(gene);

     auto* p = out.mutable_phenotype();
     p->set_mass(in.phenotype.mass);
     p->set_max_speed(in.phenotype.max_speed);
     p->set_attack_power(in.phenotype.attack_power);
     p->set_defense(in.phenotype.defense);
     p->set_max_health(in.phenotype.max_health);
     p->set_perception_radius(in.phenotype.perception_radius);
     p->set_sensor_samples(in.phenotype.sensor_samples);
     p->set_requested_compute(in.phenotype.requested_compute);
     p->set_memory_slots(in.phenotype.memory_slots);
     p->set_plant_efficiency(in.phenotype.plant_efficiency);
     p->set_meat_efficiency(in.phenotype.meat_efficiency);
     p->set_maturity_age(in.phenotype.maturity_age);
     p->set_longevity_potential(in.phenotype.longevity_potential);

     auto* s = out.mutable_state();
     fill_vec(s->mutable_position(), in.state.position);
     s->set_energy(in.state.energy);
     s->set_health(in.state.health);
     s->set_age(in.state.age);
     s->set_reproduction_cooldown(in.state.reproduction_cooldown);
     s->set_alive(in.state.alive);
     out.set_requested_compute(in.phenotype.requested_compute);

     for (const auto& r : in.resources) {
         auto* rr = out.add_resources();
         rr->set_id(r.id);
         rr->set_kind(r.kind == ResourceKind::Plant ? rpc::PLANT : rpc::CARCASS);
         fill_vec(rr->mutable_position(), r.position);
         rr->set_distance(r.distance);
         rr->set_energy(r.energy);
     }
     for (const auto& a : in.animals) {
         auto* aa = out.add_animals();
         aa->set_id(a.id);
         aa->set_species_id(a.species_id);
         fill_vec(aa->mutable_position(), a.position);
         aa->set_distance(a.distance);
         aa->set_apparent_mass(a.apparent_mass);
         aa->set_health_fraction(a.health_fraction);
         aa->set_threat(a.threat);
         aa->set_mature(a.mature);
     }
     return out;
}

DecisionInput from_rpc(const rpc::DecisionInput& in, std::uint32_t granted_compute) {
    DecisionInput out;
    out.tick = in.tick();
    out.seed = in.seed();
    out.animal_id = in.animal_id();
    out.species_id = in.species_id();

    std::array<float, Genome::kGeneCount> genes{};
    const int count = std::min<int>(in.genome().genes_size(), static_cast<int>(genes.size()));
    for (int i = 0; i < count; ++i) genes[static_cast<std::size_t>(i)] = in.genome().genes(i);
    out.genome = Genome::from_array(genes);

    const auto& p = in.phenotype();
    out.phenotype.mass = p.mass();
    out.phenotype.max_speed = p.max_speed();
    out.phenotype.attack_power = p.attack_power();
    out.phenotype.defense = p.defense();
    out.phenotype.max_health = p.max_health();
    out.phenotype.perception_radius = p.perception_radius();
    out.phenotype.sensor_samples = p.sensor_samples();
    out.phenotype.requested_compute = p.requested_compute();
    out.phenotype.memory_slots = p.memory_slots();
    out.phenotype.plant_efficiency = p.plant_efficiency();
    out.phenotype.meat_efficiency = p.meat_efficiency();
    out.phenotype.maturity_age = p.maturity_age();
    out.phenotype.longevity_potential = p.longevity_potential();
    out.granted_compute = granted_compute;

    out.state.position = read_vec(in.state().position());
    out.state.energy = in.state().energy();
    out.state.health = in.state().health();
    out.state.age = in.state().age();
    out.state.reproduction_cooldown = in.state().reproduction_cooldown();
     out.state.alive = in.state().alive();

     out.resources.reserve(static_cast<std::size_t>(in.resources_size()));
     for (const auto& r : in.resources()) {
         out.resources.push_back({r.id(), r.kind() == rpc::PLANT ? ResourceKind::Plant : ResourceKind::Carcass,
                                  read_vec(r.position()), r.distance(), r.energy()});
     }
     out.animals.reserve(static_cast<std::size_t>(in.animals_size()));
     for (const auto& a : in.animals()) {
         out.animals.push_back({a.id(), a.species_id(), read_vec(a.position()), a.distance(),
                                a.apparent_mass(), a.health_fraction(), a.threat(), a.mature()});
     }
     return out;
}

rpc::Action to_rpc(const Action& in) {
    rpc::Action out;
    out.set_animal_id(in.animal_id);
    out.set_type(to_rpc_action_type(in.type));
    out.set_target_id(in.target_id);
    fill_vec(out.mutable_destination(), in.destination);
    out.set_utility(in.utility);
    return out;
}

Action from_rpc(const rpc::Action& in) {
    Action out;
    out.animal_id = in.animal_id();
    out.type = from_rpc_action_type(in.type());
    out.target_id = in.target_id();
    out.destination = read_vec(in.destination());
    out.utility = in.utility();
    return out;
}

}   // namespace darwinsim

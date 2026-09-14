#include "darwinsim/checkpoint.hpp"

#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace darwinsim {
namespace {

constexpr std::array<char, 8> kMagic{'D','A','R','W','I','N','0','2'};

template <typename T>
void write_value(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
void read_value(std::ifstream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!in) throw std::runtime_error("checkpoint read failed");
}

void write_genome(std::ofstream& out, const Genome& genome) {
    const auto genes = genome.as_array();
    out.write(reinterpret_cast<const char*>(genes.data()), static_cast<std::streamsize>(genes.size() * sizeof(float)));
}

Genome read_genome(std::ifstream& in) {
    std::array<float, Genome::kGeneCount> genes{};
    in.read(reinterpret_cast<char*>(genes.data()), static_cast<std::streamsize>(genes.size() * sizeof(float)));
    if (!in) throw std::runtime_error("checkpoint genome read failed");
    return Genome::from_array(genes);
}

std::uint64_t fnv1a(std::uint64_t hash, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

}   // namespace

void save_checkpoint(const Simulation& sim, const std::string& path) {
     std::ofstream out(path, std::ios::binary | std::ios::trunc);
     if (!out) throw std::runtime_error("cannot open checkpoint for writing: " + path);
     out.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
     const std::uint64_t terrain_signature = sim.world().terrain_signature();
     write_value(out, terrain_signature);

     const auto summary = sim.summary();
     write_value(out, summary.tick);
     write_value(out, summary.births);
     write_value(out, summary.deaths);
     write_value(out, summary.speciation_events);

     AnimalId next_id = 1;
     for (const auto& a : sim.animals()) next_id = std::max(next_id, a.id + 1);
     write_value(out, next_id);

     const std::uint64_t animal_count = sim.animals().size();
     write_value(out, animal_count);
     for (const auto& a : sim.animals()) {
         write_value(out, a.id);
         write_value(out, a.species_id);
         write_genome(out, a.genome);
         write_value(out, a.state.position.x);
         write_value(out, a.state.position.y);
         write_value(out, a.state.energy);
         write_value(out, a.state.health);
         write_value(out, a.state.age);
         write_value(out, a.state.reproduction_cooldown);
         write_value(out, a.state.alive);
         write_value(out, a.lineage.parent_a);
         write_value(out, a.lineage.parent_b);
         write_value(out, a.lineage.generation);
     }

     const std::uint64_t plant_count = sim.world().plants().size();
     write_value(out, plant_count);
     for (const auto& p : sim.world().plants()) {
         write_value(out, p.id);
         write_value(out, p.position.x);
         write_value(out, p.position.y);
         write_value(out, p.energy);
         write_value(out, p.available);
     }

     const std::uint64_t carcass_count = sim.world().carcasses().size();
     write_value(out, carcass_count);
     for (const auto& c : sim.world().carcasses()) {
         write_value(out, c.id);
         write_value(out, c.source_animal);
         write_value(out, c.position.x);
         write_value(out, c.position.y);
         write_value(out, c.energy);
         write_value(out, c.created_tick);
         write_value(out, c.available);
    }
}

void load_checkpoint(Simulation& sim, const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open checkpoint: " + path);
    std::array<char, 8> magic{};
    in.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (magic != kMagic) throw std::runtime_error("unsupported checkpoint format");

    std::uint64_t checkpoint_terrain_signature = 0;
    read_value(in, checkpoint_terrain_signature);
    if (checkpoint_terrain_signature != sim.world().terrain_signature()) {
        throw std::runtime_error("checkpoint terrain/configuration does not match this simulation");
    }

    std::uint64_t tick = 0, births = 0, deaths = 0, speciation_events = 0;
    AnimalId next_id = 1;
    read_value(in, tick);
    read_value(in, births);
    read_value(in, deaths);
    read_value(in, speciation_events);
    read_value(in, next_id);

    auto& animals = sim.animals_mutable();
    animals.clear();
    std::uint64_t animal_count = 0;
    read_value(in, animal_count);
    animals.reserve(static_cast<std::size_t>(animal_count));
    for (std::uint64_t i = 0; i < animal_count; ++i) {
        Animal a;
        read_value(in, a.id);
        read_value(in, a.species_id);
        a.genome = read_genome(in);
        a.phenotype = derive_phenotype(a.genome, sim.config().phenotype);
        read_value(in, a.state.position.x);
        read_value(in, a.state.position.y);
        read_value(in, a.state.energy);
        read_value(in, a.state.health);
        read_value(in, a.state.age);
         read_value(in, a.state.reproduction_cooldown);
         read_value(in, a.state.alive);
         read_value(in, a.lineage.parent_a);
         read_value(in, a.lineage.parent_b);
         read_value(in, a.lineage.generation);
         animals.push_back(a);
     }

     auto& plants = sim.world_mutable().plants();
     plants.clear();
     std::uint64_t plant_count = 0;
     read_value(in, plant_count);
     plants.reserve(static_cast<std::size_t>(plant_count));
     for (std::uint64_t i = 0; i < plant_count; ++i) {
         PlantResource p;
         read_value(in, p.id);
         read_value(in, p.position.x);
         read_value(in, p.position.y);
         read_value(in, p.energy);
         read_value(in, p.available);
         plants.push_back(p);
     }

     auto& carcasses = sim.world_mutable().carcasses();
     carcasses.clear();
     std::uint64_t carcass_count = 0;
     read_value(in, carcass_count);
     carcasses.reserve(static_cast<std::size_t>(carcass_count));
     for (std::uint64_t i = 0; i < carcass_count; ++i) {
         CarcassResource c;
         read_value(in, c.id);
         read_value(in, c.source_animal);
         read_value(in, c.position.x);
         read_value(in, c.position.y);
         read_value(in, c.energy);
         read_value(in, c.created_tick);
         read_value(in, c.available);
         carcasses.push_back(c);
     }

     sim.world_mutable().recompute_next_resource_id();
     sim.restore_counters(tick, births, deaths, speciation_events, next_id);
}

std::uint64_t state_hash(const Simulation& sim) {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto summary = sim.summary();
    hash = fnv1a(hash, &summary.tick, sizeof(summary.tick));
    const auto terrain_signature = sim.world().terrain_signature();
    hash = fnv1a(hash, &terrain_signature, sizeof(terrain_signature));
    hash = fnv1a(hash, &summary.births, sizeof(summary.births));
    hash = fnv1a(hash, &summary.deaths, sizeof(summary.deaths));
    hash = fnv1a(hash, &summary.speciation_events, sizeof(summary.speciation_events));

    for (const auto& a : sim.animals()) {
        hash = fnv1a(hash, &a.id, sizeof(a.id));
        hash = fnv1a(hash, &a.species_id, sizeof(a.species_id));
        const auto genes = a.genome.as_array();
        hash = fnv1a(hash, genes.data(), genes.size() * sizeof(float));
        hash = fnv1a(hash, &a.state.position.x, sizeof(a.state.position.x));
        hash = fnv1a(hash, &a.state.position.y, sizeof(a.state.position.y));
        hash = fnv1a(hash, &a.state.energy, sizeof(a.state.energy));
        hash = fnv1a(hash, &a.state.health, sizeof(a.state.health));
        hash = fnv1a(hash, &a.state.age, sizeof(a.state.age));
        hash = fnv1a(hash, &a.state.reproduction_cooldown, sizeof(a.state.reproduction_cooldown));
        hash = fnv1a(hash, &a.state.alive, sizeof(a.state.alive));
        hash = fnv1a(hash, &a.lineage.parent_a, sizeof(a.lineage.parent_a));
        hash = fnv1a(hash, &a.lineage.parent_b, sizeof(a.lineage.parent_b));
        hash = fnv1a(hash, &a.lineage.generation, sizeof(a.lineage.generation));
    }

    for (const auto& p : sim.world().plants()) {
        hash = fnv1a(hash, &p.id, sizeof(p.id));
        hash = fnv1a(hash, &p.position.x, sizeof(p.position.x));
        hash = fnv1a(hash, &p.position.y, sizeof(p.position.y));
        hash = fnv1a(hash, &p.energy, sizeof(p.energy));
        hash = fnv1a(hash, &p.available, sizeof(p.available));
    }

    for (const auto& c : sim.world().carcasses()) {
        hash = fnv1a(hash, &c.id, sizeof(c.id));
        hash = fnv1a(hash, &c.source_animal, sizeof(c.source_animal));
        hash = fnv1a(hash, &c.position.x, sizeof(c.position.x));
        hash = fnv1a(hash, &c.position.y, sizeof(c.position.y));
        hash = fnv1a(hash, &c.energy, sizeof(c.energy));
        hash = fnv1a(hash, &c.created_tick, sizeof(c.created_tick));
        hash = fnv1a(hash, &c.available, sizeof(c.available));
    }
    return hash;
}

}   // namespace darwinsim

#include "darwinsim/checkpoint.hpp"
#include "darwinsim/config.hpp"
#include "darwinsim/simulation.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

void print_summary(const darwinsim::Simulation& sim) {
    const auto s = sim.summary();
    std::cout << "tick=" << s.tick
              << " living=" << s.living_animals
              << " created=" << s.total_animals_created
              << " species=" << s.species_count
              << " plants=" << s.available_plants
              << " carcasses=" << s.carcasses
              << " births=" << s.births
              << " deaths=" << s.deaths
              << " death_combat=" << s.combat_deaths
              << " death_starvation=" << s.starvation_deaths
              << " death_aging=" << s.aging_deaths
              << " death_unknown=" << s.unknown_deaths
              << " dmg_combat=" << s.combat_damage_total
              << " dmg_starvation=" << s.starvation_damage_total
              << " dmg_aging=" << s.aging_damage_total
              << " speciations=" << s.speciation_events
              << " terrain_hash=0x" << std::hex << sim.world().terrain_signature()
              << " state_hash=0x" << darwinsim::state_hash(sim) << std::dec
              << '\n';
}

}    // namespace

int main(int argc, char** argv) {
    try {
        const std::string config_path = argc > 1 ? argv[1] : "config/default.yaml";
        const std::uint64_t ticks = argc > 2 ? std::stoull(argv[2]) : 1000;
        const std::string checkpoint_path = argc > 3 ? argv[3] : "";

          auto config = darwinsim::load_config(config_path);
          darwinsim::Simulation sim(config);
          sim.initialize();

          for (std::uint64_t i = 0; i < ticks; ++i) {
              sim.step_local();
              if ((i + 1) % 100 == 0) print_summary(sim);
          }

          if (!checkpoint_path.empty()) {
              darwinsim::save_checkpoint(sim, checkpoint_path);
              std::cout << "checkpoint=" << checkpoint_path << '\n';
          }
          print_summary(sim);
         return 0;
     } catch (const std::exception& ex) {
         std::cerr << "darwinsim-cli: " << ex.what() << '\n';
         return 1;
     }
}
